#include "memg_ipc/protocol.hpp"
#include <functional>


bool validate_magic(const memg::PacketHeader* hdr) {
    return hdr->magic == memg::PROTOCOL_MAGIC;
}

bool validate_version(const memg::PacketHeader* hdr) {
    return hdr->version == memg::PROTOCOL_VERSION;
}

bool validate_header(const memg::PacketHeader* hdr) {
    std::function<bool(const memg::PacketHeader* hdr)> validators[] = {
        validate_magic, validate_version, NULL
    };
    
    uint16_t c = 0;
    bool continue_ = true;

    while (continue_) {
        std::function<bool(const memg::PacketHeader* hdr)> func = validators[c];
        if (func == NULL) return true;
        
        continue_ = func(hdr);
        c++;
    }
    return continue_;
}
