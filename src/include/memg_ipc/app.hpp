#pragma once

#include "memg_ipc/node.hpp"
#include <iostream>

namespace memg {

template <typename Derived>
class MemgApp {
public:
    explicit MemgApp(NodeContext ctx) 
        : _node(ctx) {
        
        s_app_instance = static_cast<Derived*>(this);

        // Statik dagiticilar (Dispatchers) otomatik baglanir
        _node.get_ctx()->on_packet = s_packet_dispatcher;
        _node.get_ctx()->on_tick   = s_tick_dispatcher;
        _node.get_ctx()->on_dump   = s_dump_dispatcher;
        _node.get_ctx()->on_load   = s_load_dispatcher;
    }

    virtual ~MemgApp() {
        if (s_app_instance == this) {
            s_app_instance = nullptr;
        }
    }

    // Calistirma dongusu (Life-cycle template method)
    void run() {
        Derived* self = static_cast<Derived*>(this);

        // 1. Donanim / servis baslatma kancasi (Setup hook)
        if (!self->setup()) {
            std::cerr << "[MEMG-APP] Baslatma hatasi, servis durduruluyor.\n";
            return;
        }

        // 2. Event loop calistirilir
        _node.run();

        // 3. Kapanis / kaynak temizleme kancasi (Cleanup hook)
        self->cleanup();
    }

    // Alt siniftan node erisimi gerekirse
    inline MemgNode& get_node() { return _node; }
    inline static Derived* instance() { return s_app_instance; }

    // --- Varsayilan / Opsiyonel Davranislar ---
    // Tureyen sinif bunlari ezmezse (override etmezse) bunlar calisir:
    bool setup() { return true; }
    void cleanup() {}
    void on_tick() {}
    MemgPacket dump_state() { return make_empty_packet(); }
    void load_state(MemgPacket* /*packet*/) {}

private:
    // Statik Dagitici Kopruleri (Static Dispatchers)
    static void s_packet_dispatcher(const MemgPacket* packet) {
        if (s_app_instance) {
            s_app_instance->on_packet(packet);
        }
    }

    static void s_tick_dispatcher() {
        if (s_app_instance) {
            s_app_instance->on_tick();
        }
    }

    static MemgPacket s_dump_dispatcher() {
        if (s_app_instance) {
            return s_app_instance->dump_state();
        }
        return make_empty_packet();
    }

    static void s_load_dispatcher(MemgPacket* packet) {
        if (s_app_instance) {
            s_app_instance->load_state(packet);
        }
    }

protected:
    MemgNode _node;
    inline static Derived* s_app_instance = nullptr;
};

} // namespace memg