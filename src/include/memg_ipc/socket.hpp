#pragma once
#include "memg_ipc/config.hpp"
#include <string>
#include <sys/types.h>
#include <vector>

namespace memg {

class UnixSocket {
public:
    UnixSocket(SocketConfig* cfg);
    ~UnixSocket();

    // RAII Güvenliği: Socket tanıtıcısı kopyalanamaz, sadece taşınabilir[cite: 2]
    UnixSocket(const UnixSocket&) = delete;
    UnixSocket& operator=(const UnixSocket&) = delete;
    UnixSocket(UnixSocket&& other) noexcept;
    UnixSocket& operator=(UnixSocket&& other) noexcept;

    // I/O İşlemleri (Datagram / DGRAM)
    // Non-blocking veya blocking ham okuma
    ssize_t receive(void* buffer, size_t size);

    // Hedef soket dosyasına (örneğin /tmp/memg_rgb.sock) doğrudan paket fırlatma
    ssize_t send_to(const std::string& target_path, const void* data, size_t size);

    template<typename T>
    inline ssize_t receive(std::vector<T>* buffer) {
        return receive(buffer->data(), buffer->size() * sizeof(T));
    }

    template<typename T>
    inline ssize_t send_to(const std::string& target_path, std::vector<T>* data) {
        return send_to(target_path, data->data(), data->size() * sizeof(T));
    }

    // Manuel temizleme ve soket kapatma
    void close_socket();

    int get_fd() const noexcept { return _fd; }
    const std::string& get_path() const noexcept { return _path; }

private:
    // Sunucu Tarafı Kurulumu:
    // Belirtilen yolda soket dosyası oluşturur, bind eder ve yetkileri ayarlar
    void bind_server();

    // Soketi non-blocking moda alma (epoll ile tam uyum için)
    void set_nonblocking();

    int _fd{-1};
    std::string _path{};
    SocketConfig* _cfg;
};

} // namespace memg