#include "memg_ipc/socket.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <vector>

void throw_system_error(std::string msg) {
    throw std::system_error(errno, std::generic_category(), msg);
}

int is_recv_error(ssize_t n) {
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
        return 1;
    }
    return 0;
}

namespace memg {

UnixSocket::UnixSocket() {
    // SOCK_CLOEXEC: Fork edilen alt process'lere soketin miras kalmasını engeller
    _fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (_fd < 0) {
        throw_system_error("Unix Domain Socket acilamadi");
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
        errno = EINVAL;
        throw_system_error("Soket yolu cok uzun veya gecersiz");
    }

    _path = path;
    _is_server = true;

    // Önceki process çöküşünden kalan eski soket dosyasını sil
    unlink(_path.c_str());

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw_system_error("Soket bind basarisiz: " + _path);
    }

    // Normal kullanıcıların (userspace) root process'e yazabilmesi için izinleri ayarla
    if (chmod(_path.c_str(), permissions) < 0) {
        throw_system_error("Soket dosya izinleri ayarlanamadi");
    }
}

void UnixSocket::set_nonblocking(bool enable) {
    int flags = fcntl(_fd, F_GETFL, 0);
    if (flags < 0) {
        throw_system_error("fcntl F_GETFL hatasi");
    }

    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(_fd, F_SETFL, flags) < 0) {
        throw_system_error("fcntl F_SETFL hatasi");
    }
}

ssize_t UnixSocket::receive(void* buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    ssize_t n;
    uint32_t raw_payload_size = 0;
    uint32_t payload_size = 0;

    // 1. Önce 4 baytlık paket boyutu başlığını (Header) oku
    n = recv(_fd, &raw_payload_size, sizeof(raw_payload_size), MSG_PEEK);
    if (is_recv_error(n)) throw_system_error("Soket recv (header kontrol) hatasi");
    
    // sokette henüz 4 bayt bile yoksa bekle
    if (n < static_cast<ssize_t>(sizeof(raw_payload_size))) return 0;
    payload_size = ntohl(raw_payload_size);

    // alıcının sağladığı buffer gelen veriden küçükse
    if (payload_size > size) {
        // soket bufferını boşaltmak için dinamik bir buffer a oku ve kurtul
        std::vector<uint8_t> discard_buf(sizeof(payload_size) + payload_size);
        recv(_fd, discard_buf.data(), discard_buf.size(), 0);
        errno = EMSGSIZE;
        throw_system_error("Gelen paket tampon boyutundan buyuk! Beklenen: " + 
            std::to_string(payload_size) + ", Verilen Buffer: " + std::to_string(size));
    }

    // 3. Header + Payload'ı tek seferde oku (Datagram bütünlüğünü korumak için iovec)
    struct iovec iov[2];
    uint32_t dummy_header = 0;

    iov[0].iov_base = &dummy_header;
    iov[0].iov_len  = sizeof(dummy_header);
    iov[1].iov_base = buffer;
    iov[1].iov_len  = payload_size;

    struct msghdr msg{};
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 2;

    n = recvmsg(_fd, &msg, 0);
    if (is_recv_error(n)) throw_system_error("Soket recvmsg hatasi");

    // Gerçek okunan payload boyutunu (toplam okuma - 4 bayt header) dön
    return n >= static_cast<ssize_t>(sizeof(uint32_t))
            ? (n - sizeof(uint32_t))
            : 0;
}

ssize_t UnixSocket::send_to(const std::string& target_path, const void* data, size_t size) {
    if (target_path.empty() || target_path.length() >= sizeof(sockaddr_un::sun_path)) {
        errno = EINVAL;
        throw_system_error("Hedef soket yolu gecersiz");
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, target_path.c_str(), sizeof(addr.sun_path) - 1);

    // 4 baytlık boyutu Big-Endian formatına çevir
    uint32_t payload_size_net = htonl(static_cast<uint32_t>(size));

    // Ekstra memcpy yapmamak için iovec (Scatter-Gather) kullanıyoruz
    struct iovec iov[2];

    iov[0].iov_base = &payload_size_net;
    iov[0].iov_len  = sizeof(payload_size_net);
    iov[0].iov_base = const_cast<void*>(data);
    iov[0].iov_len  = size;

    struct msghdr msg{};
    msg.msg_name    = reinterpret_cast<struct sockaddr*>(&addr);
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 2;

    // Tek bir datagram paketi olarak [4 Byte Boyut] + [Data] şeklinde fırlat
    ssize_t sent = sendmsg(_fd, &msg, 0);
    if (sent < 0) throw_system_error("sendmsg hatasi: " + target_path);

    // Çağıran tarafa yalnızca iletilen net payload boyutunu döndür
    return sent >= static_cast<ssize_t>(sizeof(payload_size_net))
            ? (sent - sizeof(payload_size_net))
            : 0;
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