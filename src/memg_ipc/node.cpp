#include "memg_ipc/node.hpp"
#include "memg_ipc/config.hpp"
#include "memg_ipc/protocol.hpp"
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
    pkt.header.type = PacketType::APPLICATION_DATA;

    if (size > sizeof(pkt.payload.data)) {
        std::cerr << "[MEMG] [send] Hata: Veri boyutu (" << size 
            << " bayt) maksimum payload kapasitesini (" 
            << sizeof(pkt.payload.data) << " bayt) asiyor!\n";
    }

    std::memcpy(pkt.payload.data, data, size);
    return send_packet(target_token, pkt);
}

bool MemgNode::subscribe(const std::string& target_service_token) {
    ControlPacket pkt{};
    pkt.header.type = PacketType::SUBSCRIBE;

    std::strncpy(pkt.payload.sender.token, _cfg.token, sizeof(pkt.payload.sender.token) - 1);
    return send_packet(target_service_token, pkt);
}

bool MemgNode::unsubscribe(const std::string& target_service_token) {
    ControlPacket pkt{};
    pkt.header.type = PacketType::UNSUBSCRIBE;

    std::strncpy(pkt.payload.sender.token, _cfg.token, sizeof(pkt.payload.sender.token) - 1);
    return send_packet(target_service_token, pkt);
}

bool MemgNode::on_system_packet(const memg::ControlPacket* packet) {
    if (packet->header.magic != 0x4D47) return true; // magic hatalı ise yut

    switch (packet->header.type) {
        case PacketType::SUBSCRIBE: {
            std::string sub_token(packet->payload.sender.token,
                        strnlen(packet->payload.sender.token, 
                                sizeof(packet->payload.sender.token)));
            if (!sub_token.empty()) {
                _subscribers.insert(sub_token);
                std::cout << "[MEMG] Abone eklendi: " << sub_token << "\n";
            }
            return true;
        }
        case PacketType::UNSUBSCRIBE: {
            std::string sub_token(packet->payload.sender.token,
                        strnlen(packet->payload.sender.token, 
                            sizeof(packet->payload.sender.token)));
            _subscribers.erase(sub_token);
            std::cout << "[MEMG] Abone cikarildi: " << sub_token << "\n";
            return true;
        }
        case PacketType::HEARTBEAT: {
            //TODO: return HEARTBEAT_RESP in here
            ControlPacket pkt{};
            pkt.header.type = PacketType::HEARTBEAT_RESP;
            strncpy(pkt.payload.sender.token, _cfg.token, sizeof(pkt.payload.sender.token) - 1);

            std::string target_token(packet->payload.sender.token,
                strnlen(packet->payload.sender.token, sizeof(packet->payload.sender.token)));

            send_packet(target_token, pkt);
            return true;
        }
        case PacketType::HEARTBEAT_RESP:
            std::cout << "[MEMG] [HEARTBEAT_RESP] token: " << packet->payload.sender.token; 
            return true;
        case PacketType::APPLICATION_DATA:
            return false; // this handled in MemgNode::run > on_packet
        default: return true; // bilinmeyen paketleri yut
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
                bool is_consumed = on_system_packet(pkt);
                if (is_consumed) continue;

                if (!on_packet) continue; // on_packet fonksiyonu yoksa denemeden devam et
                
                on_packet(pkt->payload, static_cast<size_t>(bytes - sizeof(PacketHeader)));
            }
        }
    }
}

} // namespace memg