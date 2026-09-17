#include "memg_ipc/node.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    memg::ServiceConfig cfg;
    cfg.token = "io.memg.client.test";
    cfg.is_server = false; // Server degil, sadece paket gonderici

    try {
        memg::MemgNode client_node(cfg);
        const std::string target = "io.memg.service.test";

        std::cout << "[CLIENT] Baslatildi. Hedef: " << target << std::endl;

        for (int i = 1; i <= 5; ++i) {
            std::string message = "MEMG Test Paketi #" + std::to_string(i);
            
            bool success = client_node.send(target, message.data(), message.size());
            if (success) {
                std::cout << "[CLIENT] Gonderildi: " << message << std::endl;
            } else {
                std::cerr << "[CLIENT] Gonderim basarisiz! (Server calisiyor mu?)" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }

    } catch (const std::exception& e) {
        std::cerr << "[CLIENT HATA]: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[CLIENT] Test tamamlandi." << std::endl;
    return 0;
}