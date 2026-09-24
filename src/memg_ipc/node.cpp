#include "memg_ipc/node.hpp"
#include "memg_ipc/PacketReader.hpp"
#include "memg_ipc/config.hpp"
#include "memg_ipc/protocol.hpp"
#include "memg_ipc/node_helper.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <sys/types.h>

namespace memg {

MemgNode::MemgNode(const ServiceConfig& cfg) 
    : _cfg(cfg), _poller(cfg.max_events) {
    
    _resolved_path = Registry::resolve(_cfg.token);

    if (_cfg.is_server) {
        Registry::ensure_directory(_resolved_path);
        _socket.bind_server(_resolved_path, _cfg.permissions);
        _socket.set_nonblocking(true);
        _poller.add(_socket.get_fd(), EPOLLIN);
    }
}

MemgNode::~MemgNode() {
    stop();
}

void MemgNode::stop() {
    _running = false;
}

bool MemgNode::send_raw(const std::string& target_token, const void* data, size_t size) {
    std::string target_path = Registry::resolve(target_token);
    try {
        ssize_t sent = _socket.send_to(target_path, data, size);
        return sent == size;
    } catch (std::exception& e) {
        std::cerr << "[MEMG] [send_raw] Hata: " << e.what() << "\n";
        return false;
    }
}


bool MemgNode::subscribe(const std::string& target_service_token) {
    MemgPacket  pkt  = system::make_subscribe(_cfg.token);
    return send_vector(target_service_token, &pkt);
}

bool MemgNode::unsubscribe(const std::string& target_service_token) {
    MemgPacket  pkt  = system::make_unsubscribe(_cfg.token);
    return send_vector(target_service_token, &pkt);
}

bool MemgNode::on_system_packet_subscribe(const MemgPacket* packet) {
    PacketReader reader(packet->data(), packet->size());
    std::string sub_token;
    reader.get(system::TOKEN, sub_token);

    if (!sub_token.empty()) {
        _subscribers.insert(sub_token);
        std::cout << "[MEMG] Abone eklendi: " << sub_token << "\n";
        return true;
    }
    return false;
}

bool MemgNode::on_system_packet_unsubscribe(const MemgPacket* packet) {
    PacketReader reader(packet->data(), packet->size());
    std::string sub_token;
    reader.get(system::TOKEN, sub_token);

    _subscribers.erase(sub_token);
    std::cout << "[MEMG] Abone cikarildi: " << sub_token << "\n";
    return true;  
}

bool MemgNode::on_system_packet_heartbeat(const MemgPacket* packet) {
    MemgPacket  pkt  = system::make_heartbeat_response(_cfg.token);

    std::string target_token;
    PacketReader(packet->data(), packet->size()).get(system::TOKEN, target_token);

    return send_vector(reinterpret_cast<const std::string&>(target_token), &pkt);
}

bool MemgNode::on_system_packet_heartbeat_resp(const MemgPacket* packet) {
    std::string token;
    PacketReader(packet->data(), packet->size()).get(system::TOKEN, token);
    std::cout << "[MEMG] [HEARTBEAT_RESP] token: " << token; 
    return true;
}

bool MemgNode::on_system_packet(const MemgPacket* packet) {
    uint16_t command;
    PacketReader(packet->data(), packet->size()).get(system::COMMAND, command);

    switch (command) {
        case system::SUBSCRIBE:         return on_system_packet_subscribe(packet);
        case system::UNSUBSCRIBE:       return on_system_packet_unsubscribe(packet);
        case system::HEARTBEAT:         return on_system_packet_heartbeat(packet);
        case system::HEARTBEAT_RESP:    return on_system_packet_heartbeat_resp(packet);
        default:                        return false;
    }
}

void MemgNode::publish(const void* data, size_t size) {
    auto it = _subscribers.begin();
    while (it != _subscribers.end()) {
        const std::string& sub_token = *it;
        bool ok = send(sub_token, data, size);
        if (!ok) {
            std::cout << "[MEMG] Aboneye erisilemedi, listeden siliniyor: " << sub_token << "\n";
            it = _subscribers.erase(it);
        } else {
            ++it;
        }
    }
}

//TODO: poller static global bir değişken olmalı ve her memg instance için 
//      yeni üretmek yerine 1 poller ile birden fazla sokete bağlanabilmeliyiz
void MemgNode::run(PacketCallback on_packet, TickCallback on_tick) {
    _running = true;
    MemgPacket buffer(4096);

    while (_running) {
        int nfds = _poller.wait(_cfg.timeout_ms);

        if (nfds < 0) continue; // hata kodu gelmişse devam et
        if (nfds == 0) {
            if (!on_tick) continue; // on_tick fonksiyonu yoksa denemeden devam et
            on_tick(); continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if ((_poller.get_event(i).data.fd != _socket.get_fd())) continue;

            while (true) {
                ssize_t bytes = _socket.receive(buffer.data(), buffer.size());
                if (bytes <= 0) break;

                // en az header büyüklüğünde olmalı
                if (static_cast<size_t>(bytes) < sizeof(PacketHeader)) continue;

                const MemgPacket     packet(buffer.begin(), buffer.begin() + bytes);
                PacketReader reader(packet.data(), packet.size());

                if (has_flag(PacketFlag::IN_SYSTEM, static_cast<PacketFlag>(reader.flags()))) {
                    on_system_packet(&packet);
                } else {
                    if (!on_packet) continue; // on_packet fonksiyonu yoksa denemeden devam et
                    on_packet(&packet);
                }
            }
        }
    }
}

} // namespace memg