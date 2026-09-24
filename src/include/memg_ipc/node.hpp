#pragma once

#include "memg_ipc/config.hpp"
#include "memg_ipc/epoll.hpp"
#include "memg_ipc/protocol.hpp"
#include "memg_ipc/socket.hpp"
#include <functional>
#include <atomic>
#include <unordered_set>
#include <vector>

namespace memg {

class MemgNode {
public:
    using PacketCallback = std::function<void(const MemgPacket* data)>;
    using TickCallback = std::function<void()>;

    explicit MemgNode(const ServiceConfig& cfg);
    ~MemgNode();

    // Event döngüsünü başlatır. run() çağrılan thread burada döngüde kalır.
    void run(PacketCallback on_packet, TickCallback on_tick = nullptr);

    // Döngüyü dışarıdan durdurmak için (örn: SIGINT / Ctrl+C anında)
    void stop();

    bool send_raw(const std::string& target_token, const void* data, size_t size);
    
    template<typename T>
    bool send_vector(const std::string& target_token, const std::vector<T>* data) {
        return send_raw(target_token, data->data(), (data->size() * sizeof(T)));
    }

    // 1. Ham veri gönderme (APPLICATION_DATA başlığı ile otomatik sarar)
    bool send(const std::string& target_token, const void* data, size_t size);

    // 3. Bir servise abone olma talebi atar
    bool subscribe(const std::string& target_service_token);

    // 4. Bir servisten aboneliği iptal eder
    bool unsubscribe(const std::string& target_service_token);

    // 5. Kendisine abone olan tüm servislere canlı veri yayınlar (Dead Subscriber Eviction ile)
    void publish(const void* data, size_t size);
private:
    bool on_system_packet(const MemgPacket* packet);
    bool on_system_packet_subscribe(const MemgPacket* packet);
    bool on_system_packet_unsubscribe(const MemgPacket* packet);
    bool on_system_packet_heartbeat(const MemgPacket* packet) ;
    bool on_system_packet_heartbeat_resp(const MemgPacket* packet);
    

    ServiceConfig _cfg;
    UnixSocket _socket;
    Epoll _poller;
    std::atomic<bool> _running{false};
    std::string _resolved_path;
    std::unordered_set<std::string> _subscribers;
};

} // namespace memg