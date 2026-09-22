#include "memg_ipc/node.hpp"
#include "memg_ipc/config.hpp"
#include "memg_ipc/protocol.hpp"
#include "memg_ipc/node_helper.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <vector>

namespace memg {

MemgNode::MemgNode(const ServiceConfig& cfg) 
    : _cfg(cfg), _poller(cfg.max_events) {
    
    _resolved_path = Registry::resolve(std::string(_cfg.token));

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

bool MemgNode::send_packet(const std::string& target_token, const ControlPacket& pkt) {
    std::string target_path = Registry::resolve(target_token);
    try {
        ssize_t sent = _socket.send_to(target_path, &pkt, sizeof(pkt));
        return sent == static_cast<ssize_t>(sizeof(pkt));
    } catch (std::exception& e) {
        std::cerr << "[MEMG] [send_packet] Hata: " << e.what() << "\n";
        return false;
    }
}

bool MemgNode::send(const std::string& target_token, const void* data, size_t size) {
    ControlPacket pkt{};
    pkt.header.flags = PacketFlag::NONE;

    if (size > sizeof(pkt.body.data)) {
        std::cerr << "[MEMG] [send] Hata: Veri boyutu (" << size 
            << " bayt) maksimum payload kapasitesini (" 
            << sizeof(pkt.body.data) << " bayt) asiyor!\n";
    }

    std::memcpy(pkt.body.data, data, size);
    return send_packet(target_token, pkt);
}

bool MemgNode::subscribe(const std::string& target_service_token) {
    ControlPacket pkt{};
    pkt.header.type = static_cast<uint16_t>(SystemCommand::SUBSCRIBE);
    pkt.header.flags = PacketFlag::IN_SYSTEM;

    std::strncpy(pkt.body.sender.token, _cfg.token, sizeof(pkt.body.sender.token) - 1);
    return send_packet(target_service_token, pkt);
}

bool MemgNode::unsubscribe(const std::string& target_service_token) {
    ControlPacket pkt{};
    pkt.header.type = static_cast<uint16_t>(SystemCommand::UNSUBSCRIBE);
    pkt.header.flags = PacketFlag::IN_SYSTEM;

    std::strncpy(pkt.body.sender.token, _cfg.token, sizeof(pkt.body.sender.token) - 1);
    return send_packet(target_service_token, pkt);
}

bool MemgNode::on_system_packet_subscribe(const memg::ControlPacket* packet) {
    std::string sub_token(packet->body.sender.token,
                strnlen(packet->body.sender.token, 
                        sizeof(packet->body.sender.token)));
    if (!sub_token.empty()) {
        _subscribers.insert(sub_token);
        std::cout << "[MEMG] Abone eklendi: " << sub_token << "\n";
    }
    return true;
}

bool MemgNode::on_system_packet_unsubscribe(const memg::ControlPacket* packet) {
    std::string sub_token(packet->body.sender.token,
            strnlen(packet->body.sender.token, 
                    sizeof(packet->body.sender.token)));
    _subscribers.erase(sub_token);
    std::cout << "[MEMG] Abone cikarildi: " << sub_token << "\n";
    return true;  
}

bool MemgNode::on_system_packet_heartbeat(const memg::ControlPacket* packet) {
    ControlPacket pkt{};
    pkt.header.flags = PacketFlag::IN_SYSTEM;
    pkt.header.type = static_cast<uint16_t>(SystemCommand::HEARTBEAT_RESP);

    strncpy(pkt.body.sender.token, _cfg.token, sizeof(pkt.body.sender.token) - 1);

    std::string target_token(packet->body.sender.token,
        strnlen(packet->body.sender.token, sizeof(packet->body.sender.token)));

    send_packet(target_token, pkt);
    return true;
}

bool MemgNode::on_system_packet_heartbeat_resp(const memg::ControlPacket* packet) {
    std::cout << "[MEMG] [HEARTBEAT_RESP] token: " << packet->body.sender.token; 
    return true;
}

bool MemgNode::on_system_packet(const memg::ControlPacket* packet) {
    uint16_t type = packet->header.type;

           if (type == static_cast<uint16_t>(SystemCommand::SUBSCRIBE)) {
        return on_system_packet_subscribe(packet);
    } else if (type == static_cast<uint16_t>(SystemCommand::UNSUBSCRIBE)) {
        return on_system_packet_unsubscribe(packet);   
    } else if (type == static_cast<uint16_t>(SystemCommand::HEARTBEAT)) {
        return on_system_packet_heartbeat(packet);
    } else if (type == static_cast<uint16_t>(SystemCommand::HEARTBEAT_RESP)) {
        return on_system_packet_heartbeat_resp(packet);
    } else return false;
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
    std::vector<uint8_t> buffer(4096);

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

                const ControlPacket* pkt = reinterpret_cast<const ControlPacket*>(buffer.data());
                const PacketHeader*  hdr = reinterpret_cast<const PacketHeader* >(&pkt->header);
                if (!validate_header(hdr)) continue; // header valid değilse devam et

                if (has_flag(PacketFlag::IN_SYSTEM, hdr->flags)) {
                    on_system_packet(pkt);
                } else {
                    if (!on_packet) continue; // on_packet fonksiyonu yoksa denemeden devam et
                    on_packet(pkt->body, static_cast<size_t>(bytes - sizeof(PacketHeader)));
                }
            }
        }
    }
}

} // namespace memg