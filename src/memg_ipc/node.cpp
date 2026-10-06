#include "memg_ipc/node.hpp"
#include "memg_ipc/PacketBuilder.hpp"
#include "memg_ipc/PacketReader.hpp"
#include "memg_ipc/config.hpp"
#include "memg_ipc/protocol.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <csignal>
#include <fstream>


namespace memg {

MemgPacket make_empty_packet() {return PacketBuilder().build();}
void dummy_loader(MemgPacket* packet) {};

// ===================
// === CONSTRUCTOR ===
// ===================
MemgNode::MemgNode(const NodeContext& ctx) 
    :   _ctx(ctx),
        _secure_key(s_generate_private_key()),
        _poller(ctx.cfg->max_events),
        _resolved_path(Registry::get_socket_file(ctx.cfg->token)) {
    
    s_instance = this;
    signal(SIGINT,  s_stop);
    signal(SIGTERM, s_stop);

    if (ctx.cfg->is_server) {
        Registry::ensure_directory(_resolved_path);
        _socket.bind_server(_resolved_path, ctx.cfg->permissions);
        _socket.set_nonblocking(true);
        _poller.add(_socket.get_fd(), EPOLLIN);
    }
}

MemgNode::~MemgNode() {
    stop();
}

// ==============
// === SENDER ===
// ==============
bool MemgNode::send_raw(const std::string& target_token, const void* data, size_t size) {
    std::string target_path = Registry::get_socket_file(target_token);
    try {
        ssize_t sent = _socket.send_to(target_path, data, size);
        return sent == size;
    } catch (std::exception& e) {
        std::cerr << "[MEMG] [send_raw] Hata: " << e.what() << "\n";
        return false;
    }
}

void MemgNode::publish(const void* data, size_t size) {
    auto it = _subscribers.begin();
    while (it != _subscribers.end()) {
        const std::string& sub_token = *it;
        bool ok = send_raw(sub_token, data, size);
        if (!ok) {
            std::cout << "[MEMG] Aboneye erisilemedi, listeden siliniyor: " << sub_token << "\n";
            it = _subscribers.erase(it);
        } else {
            ++it;
        }
    }
}

bool MemgNode::subscribe(const std::string& target_service_token) {
    MemgPacket  pkt  = system::make_subscribe(_ctx.cfg->token);
    return send_vector(target_service_token, &pkt);
}

bool MemgNode::unsubscribe(const std::string& target_service_token) {
    MemgPacket  pkt  = system::make_unsubscribe(_ctx.cfg->token);
    return send_vector(target_service_token, &pkt);
}

// =================
// === ON PACKET ===
// =================

bool MemgNode::on_system_packet_subscribe(const MemgPacket* packet) {
    PacketReader reader(packet);
    std::string sender;
    reader.get(system::SENDER, sender);

    if (!sender.empty()) {
        _subscribers.insert(sender);
        std::cout << "[MEMG] Abone eklendi: " << sender  << "\n";
        return true;
    }
    return false;
}

bool MemgNode::on_system_packet_unsubscribe(const MemgPacket* packet) {
    PacketReader reader(packet);
    std::string sender;
    reader.get(system::SENDER, sender);

    _subscribers.erase(sender);
    std::cout << "[MEMG] Abone cikarildi: " << sender << "\n";
    return true;  
}

bool MemgNode::on_system_packet_heartbeat(const MemgPacket* packet) {
    MemgPacket  pkt  = system::make_heartbeat_response(_ctx.cfg->token);

    std::string sender;
    PacketReader(packet).get(system::SENDER, sender);

    return send_vector(sender, &pkt);
}

bool MemgNode::on_system_packet_heartbeat_resp(const MemgPacket* packet) {
    std::string sender;
    PacketReader(packet).get(system::SENDER, sender);
    std::cout << "[MEMG] [HEARTBEAT_RESP] token: " << sender; 
    return true;
}

bool MemgNode::on_system_packet(const MemgPacket* packet) {
    uint16_t command;
    PacketReader(packet).get(system::COMMAND, command);

    switch (command) {
        case system::SUBSCRIBE:         return on_system_packet_subscribe(packet);
        case system::UNSUBSCRIBE:       return on_system_packet_unsubscribe(packet);
        case system::HEARTBEAT:         return on_system_packet_heartbeat(packet);
        case system::HEARTBEAT_RESP:    return on_system_packet_heartbeat_resp(packet);
        default:                        return false;
    }
}

void MemgNode::run() {
    _running = true;
    MemgPacket buffer(4096);

    while (_running) {
        int nfds = _poller.wait(_ctx.cfg->timeout_ms);

        if (s_stop_signal_flag) break;
        if (nfds < 0) continue; // hata kodu gelmişse devam et
        if (nfds == 0) {
            _ctx.on_tick(); continue;
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
                if (!reader.is_valid()) {continue;}

                if (has_flag(PacketFlag::IN_SYSTEM, static_cast<PacketFlag>(reader.flags()))) {
                    on_system_packet(&packet);
                } else {
                    _ctx.on_packet(&packet);
                }
            }
        }
    }
    _running = false;
    dump_cache();
}

void MemgNode::stop() {
    s_stop_signal_flag = 1;    
}

void MemgNode::dump_cache() {
    MemgPacket dump_value = _ctx.on_dump();
    std::ofstream file;

    std::string file_path(Registry::get_cache_file(_ctx.cfg->token));
    Registry::ensure_directory(file_path);

    file.open(file_path);
    if (!file.is_open()) return;

    // Paketin ham bayt verisi (Raw Buffer) dogrudan dosyaya yazilir
    file.write(reinterpret_cast<const char*>(dump_value.data()), dump_value.size());
    file.close();
}

void MemgNode::load_cache() {
    std::ifstream file;

    std::string file_path(Registry::get_cache_file(_ctx.cfg->token));
    Registry::ensure_directory(file_path);

    file.open(file_path);
    if (!file.is_open()) return;

    std::streamsize file_size = file.tellg();
    if (file_size <= 0) {
        file.close();
        return;
    }

    // Dosya basina donulur ve veri okunur
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(file_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        file.close();
        return;
    }
    file.close();

    if (!PacketReader(&buffer).is_valid()) return;
    
    _ctx.on_load(&buffer);
}

void MemgNode::s_stop(int /*sig*/) {
    s_stop_signal_flag = 1;
}

MemgNode* MemgNode::s_get_instance() {
    return s_instance;
}

/* TODO:
açılan soketler kendi scopeuna bağlı olarak gerekli config dosyasına adresini yazmalı artık yeter dimi :D 
*/

} // namespace memg