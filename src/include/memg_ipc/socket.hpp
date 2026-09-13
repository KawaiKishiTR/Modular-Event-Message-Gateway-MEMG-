#pragma once

#include <string>
#include <sys/types.h>

namespace memg {

class UnixSocket {
public:
    UnixSocket();
    ~UnixSocket();

    // RAII Güvenliği: Socket tanıtıcısı kopyalanamaz, sadece taşınabilir[cite: 2]
    UnixSocket(const UnixSocket&) = delete;
    UnixSocket& operator=(const UnixSocket&) = delete;
    UnixSocket(UnixSocket&& other) noexcept;
    UnixSocket& operator=(UnixSocket&& other) noexcept;

    // Sunucu Tarafı Kurulumu:
    // Belirtilen yolda soket dosyası oluşturur, bind eder ve yetkileri ayarlar
    void bind_server(const std::string& path, mode_t permissions = 0666);

    // I/O İşlemleri (Datagram / DGRAM)
    // Non-blocking veya blocking ham okuma
    ssize_t receive(void* buffer, size_t size);

    // Hedef soket dosyasına (örneğin /tmp/memg_rgb.sock) doğrudan paket fırlatma
    ssize_t send_to(const std::string& target_path, const void* data, size_t size);

    // Soketi non-blocking moda alma (epoll ile tam uyum için)
    void set_nonblocking(bool enable = true);

    // Manuel temizleme ve soket kapatma
    void close_socket();

    int get_fd() const noexcept { return _fd; }
    const std::string& get_path() const noexcept { return _path; }

private:
    int _fd{-1};
    std::string _path{};
    bool _is_server{false};
};

} // namespace memg