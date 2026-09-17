#include "memg_ipc/node.hpp"
#include <vector>

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

bool MemgNode::send(const std::string& target_token, const void* data, size_t size) {
    std::string target_path = Registry::resolve(target_token);
    try {
        ssize_t sent = _socket.send_to(target_path, data, size);
        return sent == static_cast<ssize_t>(size);
    } catch (...) {
        return false;
    }
}

//TODO: poller static global bir değişken olmalı ve her memg instance için 
//      yeni üretmek yerine 1 poller ile birden fazla sokete bağlanabilmeliyiz
void MemgNode::run(PacketCallback on_packet, TickCallback on_tick) {
    _running = true;
    std::vector<uint8_t> buffer(4096);

    while (_running) {
        int nfds = _poller.wait(_cfg.timeout_ms);

        if (nfds > 0) {
            for (int i = 0; i < nfds; ++i) {
                if (_poller.get_event(i).data.fd == _socket.get_fd()) {
                    while (true) {
                        ssize_t bytes = _socket.receive(buffer.data(), buffer.size());
                        if (bytes <= 0) break;
                        if (on_packet) {
                            on_packet(buffer.data(), static_cast<size_t>(bytes));
                        }
                    }
                }
            }
        } else if (nfds == 0) {
            // Timeout gerçekleşti (Tick)
            if (on_tick) {
                on_tick();
            }
        }
    }
}

} // namespace memg