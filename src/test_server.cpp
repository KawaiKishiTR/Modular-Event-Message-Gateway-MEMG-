#include "memg_ipc/node.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_shutdown{false};
static memg::MemgNode* g_node_ptr{nullptr};

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\n[SERVER] Kapatma sinyali alindi..." << std::endl;
        g_shutdown = true;
        if (g_node_ptr) {
            g_node_ptr->stop();
        }
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    memg::ServiceConfig cfg;
    cfg.token = "io.memg.service.test";
    cfg.is_server = true;
    cfg.timeout_ms = 1000; // 1 saniyede bir tick tetiklensin

    std::cout << "[SERVER] Baslatiliyor... Token: " << cfg.token << std::endl;

    try {
        memg::MemgNode server_node(cfg);
        g_node_ptr = &server_node;

        server_node.run(
            // 1. Paket geldiginde calisacak callback:
            [](const uint8_t* data, size_t size) {
                std::string msg(reinterpret_cast<const char*>(data), size);
                std::cout << "[SERVER] Paket Alindi (" << size << " bayt): " << msg << std::endl;
            },
            // 2. Timeout (Tick) callback:
            []() {
                std::cout << "[SERVER] [Tick] Veri bekleniyor..." << std::endl;
            }
        );

    } catch (const std::exception& e) {
        std::cerr << "[SERVER HATA]: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[SERVER] Basariyla durduruldu." << std::endl;
    return 0;
}