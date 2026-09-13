#include "memg_ipc/socket.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <system_error>

namespace memg {

UnixSocket::UnixSocket() {
    // SOCK_CLOEXEC: Fork edilen alt process'lere soketin miras kalmasını engeller
    _fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (_fd < 0) {
        throw std::system_error(errno, std::generic_category(), "Unix Domain Socket acilamadi");
    }
}

UnixSocket::~UnixSocket() {
    close_socket();
}

UnixSocket::UnixSocket(UnixSocket&& other) noexcept 
    : _fd(other._fd), _path(std::move(other._path)), _is_server(other._is_server) {
    other._fd = -1;
    other._is_server = false;
}

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept {
    if (this != &other) {
        close_socket();
        _fd = other._fd;
        _path = std::move(other._path);
        _is_server = other._is_server;
        other._fd = -1;
        other._is_server = false;
    }
    return *this;
}

void UnixSocket::bind_server(const std::string& path, mode_t permissions) {
    if (path.empty() || path.length() >= sizeof(sockaddr_un::sun_path)) {
        throw std::system_error(EINVAL, std::generic_category(), "Soket yolu cok uzun veya gecersiz");
    }

    _path = path;
    _is_server = true;

    // Önceki process çöküşünden kalan eski soket dosyasını sil
    unlink(_path.c_str());

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::system_error(errno, std::generic_category(), "Soket bind basarisiz: " + _path);
    }

    // Normal kullanıcıların (userspace) root process'e yazabilmesi için izinleri ayarla
    if (chmod(_path.c_str(), permissions) < 0) {
        throw std::system_error(errno, std::generic_category(), "Soket dosya izinleri ayarlanamadi");
    }
}

void UnixSocket::set_nonblocking(bool enable) {
    int flags = fcntl(_fd, F_GETFL, 0);
    if (flags < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl F_GETFL hatasi");
    }

    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(_fd, F_SETFL, flags) < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl F_SETFL hatasi");
    }
}

//TODO: paket gönderiminde 4byte size eklenmeli 
//      böylece bufferdan büyük paket gelirse parçalanmadan okunabilir

ssize_t UnixSocket::receive(void* buffer, size_t size) {
    ssize_t n = recv(_fd, buffer, size, 0);
    if (n < 0) {
        // Non-blocking modda veri yoksa -1 döner ve errno EAGAIN/EWOULDBLOCK olur
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        throw std::system_error(errno, std::generic_category(), "Soket recv hatasi");
    }
    return n;
}

ssize_t UnixSocket::send_to(const std::string& target_path, const void* data, size_t size) {
    if (target_path.empty() || target_path.length() >= sizeof(sockaddr_un::sun_path)) {
        throw std::system_error(EINVAL, std::generic_category(), "Hedef soket yolu gecersiz");
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, target_path.c_str(), sizeof(addr.sun_path) - 1);

    ssize_t sent = sendto(_fd, data, size, 0,
                          reinterpret_cast<struct sockaddr*>(&addr),
                          sizeof(addr));
    if (sent < 0) {
        throw std::system_error(errno, std::generic_category(), "sendto hatasi: " + target_path);
    }
    return sent;
}

void UnixSocket::close_socket() {
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
    if (_is_server && !_path.empty()) {
        unlink(_path.c_str());
        _path.clear();
        _is_server = false;
    }
}

} // namespace memg