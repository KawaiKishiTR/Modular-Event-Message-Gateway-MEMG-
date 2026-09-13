#pragma once

#include "memg_ipc/config.hpp"
#include "memg_ipc/epoll.hpp"
#include "memg_ipc/socket.hpp"
#include <functional>
#include <atomic>

namespace memg {

class MemgNode {
public:
    using PacketCallback = std::function<void(const uint8_t* data, size_t size)>;
    using TickCallback = std::function<void()>;

    explicit MemgNode(const ServiceConfig& cfg);
    ~MemgNode();

    // Event döngüsünü başlatır. run() çağrılan thread burada döngüde kalır.
    void run(PacketCallback on_packet, TickCallback on_tick = nullptr);

    // Döngüyü dışarıdan durdurmak için (örn: SIGINT / Ctrl+C anında)
    void stop();

    // Başka bir servisin token'ına veri fırlatır (Örn: "io.memg.service.rgb")
    bool send(const std::string& target_token, const void* data, size_t size);

private:
    ServiceConfig _cfg;
    UnixSocket _socket;
    Epoll _poller;
    std::atomic<bool> _running{false};
    std::string _resolved_path;
};

} // namespace memg