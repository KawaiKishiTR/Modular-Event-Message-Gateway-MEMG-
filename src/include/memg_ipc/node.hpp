#pragma once

#include "memg_ipc/config.hpp"
#include "memg_ipc/epoll.hpp"
#include "memg_ipc/protocol.hpp"
#include "memg_ipc/socket.hpp"
#include <functional>
#include <atomic>
#include <unordered_set>

namespace memg {

class MemgNode {
public:
    using PacketCallback = std::function<void(const payload_t& data, size_t size)>;
    using TickCallback = std::function<void()>;

    explicit MemgNode(const ServiceConfig& cfg);
    ~MemgNode();

    // Event döngüsünü başlatır. run() çağrılan thread burada döngüde kalır.
    void run(PacketCallback on_packet, TickCallback on_tick = nullptr);

    // Döngüyü dışarıdan durdurmak için (örn: SIGINT / Ctrl+C anında)
    void stop();

    // 1. Ham veri gönderme (APPLICATION_DATA başlığı ile otomatik sarar)
    bool send(const std::string& target_token, const void* data, size_t size);

    // 2. Dahili/Sistem paketi fırlatma
    bool send_packet(const std::string& target_token, const ControlPacket& pkt);

    // 3. Bir servise abone olma talebi atar
    bool subscribe(const std::string& target_service_token);

    // 4. Bir servisten aboneliği iptal eder
    bool unsubscribe(const std::string& target_service_token);

    // 5. Kendisine abone olan tüm servislere canlı veri yayınlar (Dead Subscriber Eviction ile)
    void publish(const void* data, size_t size);
private:
    bool on_system_packet(const ControlPacket* packet);

    ServiceConfig _cfg;
    UnixSocket _socket;
    Epoll _poller;
    std::atomic<bool> _running{false};
    std::string _resolved_path;
    std::unordered_set<std::string> _subscribers;
};

} // namespace memg