#pragma once
#include "memg_ipc/protocol.hpp"
#include <algorithm>
#include <cstddef>
#include <sys/types.h>
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace memg {

class PacketBuilder {
public:
    explicit PacketBuilder(uint16_t flags = 0) {
        // 8byte header için ayır
        _buffer.resize(512);
        _offset = sizeof(PacketHeader);
        PacketHeader* _hdr = reinterpret_cast<PacketHeader*>(_buffer.data());

        _buffer[0] = static_cast<uint8_t>( PROTOCOL_MAGIC         & 0xff);
        _buffer[1] = static_cast<uint8_t>((PROTOCOL_MAGIC   >> 8) & 0xff);
        _buffer[2] = static_cast<uint8_t>( PROTOCOL_VERSION       & 0xff);
        _buffer[3] = static_cast<uint8_t>((PROTOCOL_VERSION >> 8) & 0xff);
        _buffer[4] = static_cast<uint8_t>( flags                  & 0xff);
        _buffer[5] = static_cast<uint8_t>((flags            >> 8) & 0xff);
        _buffer[6] = 0x00;
        _buffer[7] = 0x00;
    }

    PacketBuilder& add_raw(system::TagType tag, const void* data, system::LenType size) {
        size_t entry_size = size + sizeof(tag) + sizeof(size);

        if (_offset + entry_size > _buffer.size()) {
            _buffer.resize(_offset + std::max<size_t>(512, entry_size));
        }

        // Tag (Little-Endian)
        _buffer[_offset + 0] = static_cast<uint8_t>(tag & 0xFF);
        _buffer[_offset + 1] = static_cast<uint8_t>((tag >> 8) & 0xFF);

        // Size (Little-Endian)
        _buffer[_offset + 2] = static_cast<uint8_t>(size & 0xFF);
        _buffer[_offset + 3] = static_cast<uint8_t>((size >> 8) & 0xFF);

        _offset += sizeof(tag) + sizeof(size);
        std::memcpy(_buffer.data() + _offset, data, size);
        _offset += size;
        return *this;
    }

    template<typename T>
    PacketBuilder& add(uint16_t tag, const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Tip trivial olmali");
        return add_raw(tag, &value, static_cast<uint16_t>(sizeof(T)));
    }

    const MemgPacket& build() {
        // get rid of extra bytes
        if (_offset < _buffer.size()) {_buffer.resize(_offset);}

        // body_size update
        ssize_t size = _buffer.size() - sizeof(PacketHeader);
        _buffer[6] = static_cast<uint8_t>( size       & 0xff);
        _buffer[7] = static_cast<uint8_t>((size >> 8) & 0xff);

        return _buffer;
    }

private:
    MemgPacket _buffer;
    size_t _offset;
};

} // namespace memg