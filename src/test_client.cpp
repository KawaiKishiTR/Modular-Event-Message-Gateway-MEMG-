#include "memg_ipc/node.hpp"
#include "memg_ipc/protocol.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

void send_packets(memg::MemgNode& client_node, std::string target) {
    for (int i = 1; i <= 10; ++i) {
        std::string message = "MEMG Test Paketi #" + std::to_string(i);
        
        bool success = client_node.send(target, message.data(), message.size());
        if (success) {
            std::cout << "[CLIENT] Gonderildi: " << message << std::endl;
        } else {
            std::cerr << "[CLIENT] Gonderim basarisiz! (Server calisiyor mu?)" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

int main() {
    memg::ServiceConfig cfg;
    char token[] = "io.memg.client.test";
    strncpy(cfg.token, token, sizeof(token));
    cfg.is_server = true; // Server degil, sadece paket gonderici

    try {
        memg::MemgNode client_node(cfg);
        const std::string target = "io.memg.service.test";

        std::cout << "[CLIENT] Baslatildi. Hedef: " << target << std::endl;
        
        memg::ControlPacket pkt{};
        pkt.header.type = memg::PacketType::HEARTBEAT;
        strncpy(pkt.payload.sender.token, cfg.token, sizeof(pkt.payload.sender.token));

        client_node.send_packet(target, pkt); // heartbeat

        pkt.header.type = memg::PacketType::SUBSCRIBE;
        client_node.send_packet(target, pkt); // subscribe
        
        send_packets(client_node, target);


    } catch (const std::exception& e) {
        std::cerr << "[CLIENT HATA]: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[CLIENT] Test tamamlandi." << std::endl;
    return 0;
}