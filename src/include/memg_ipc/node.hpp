#pragma once

#include "memg_ipc/config.hpp"
#include "memg_ipc/epoll.hpp"
#include "memg_ipc/protocol.hpp"
#include "memg_ipc/socket.hpp"
#include <atomic>
#include <random>
#include <unordered_set>
#include <vector>
#include <atomic>


namespace memg {

class MemgNode;

MemgPacket make_empty_packet();
void dummy_loader(MemgPacket* packet);

using PacketCallback    = void(*)(const memg::MemgPacket* packet);
using TickCallback      = void(*)();
using DumpCallback      = memg::MemgPacket(*)();
using LoadCallback      = void(*)(MemgPacket* packet);

struct NodeContext {
    // runtime callbacks
    PacketCallback          on_packet;
    TickCallback            on_tick;

    DumpCallback            on_dump{make_empty_packet};
    LoadCallback            on_load{dummy_loader};

    ServiceConfig*          cfg;
};

class MemgNode {
public:
    explicit MemgNode(const NodeContext& ctx);
    ~MemgNode();

    void run();
    void stop();

    // sender functions
    bool send_raw(const std::string& target_token, const void* data, size_t size);
    template<typename T>
    bool send_vector(const std::string& target_token, const std::vector<T>* data) {
        return send_raw(target_token, data->data(), (data->size() * sizeof(T)));
    }

    // sub/pub fonksiyonları
    bool subscribe(const std::string& target_service_token);
    bool unsubscribe(const std::string& target_service_token);
    void publish(const void* data, size_t size);

    static MemgNode* s_get_instance();
    static void s_stop(int /*sig*/);


    inline NodeContext* get_ctx(){return &_ctx;}

private:
    // system packet handlers
    bool on_system_packet                   (const MemgPacket* packet);
    bool on_system_packet_subscribe         (const MemgPacket* packet);
    bool on_system_packet_unsubscribe       (const MemgPacket* packet);
    bool on_system_packet_heartbeat         (const MemgPacket* packet);
    bool on_system_packet_heartbeat_resp    (const MemgPacket* packet);
    bool on_system_packet_shutdown          (const MemgPacket* packet);

    void dump_cache();
    void load_cache();

    static inline uint64_t s_generate_private_key() {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32 | static_cast<uint64_t>(rd()));
    }

    // static variables holds instance
    inline static MemgNode* s_instance = nullptr;
    inline static volatile std::atomic<bool> s_stop_signal_flag = 0;

    // sub workers
    UnixSocket _socket;
    Epoll _poller;

    // one time defined variables
    NodeContext _ctx;
    uint64_t _secure_key;
    std::string _resolved_path;
    std::atomic<bool> _running{false};

    // dynamic runtime data variables
    std::unordered_set<std::string> _subscribers;

};



} // namespace memg