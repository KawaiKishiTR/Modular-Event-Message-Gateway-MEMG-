#pragma once

#include "memg_ipc/protocol.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace memg {

class PacketReader {
public:
    PacketReader(const uint8_t* data, size_t size) : _data(data), _size(size) {}

    bool is_valid() const {
        if (_size < sizeof(PacketHeader)) return false;
        return  _data[0] == static_cast<uint8_t>( PROTOCOL_MAGIC         & 0xff) &&
                _data[1] == static_cast<uint8_t>((PROTOCOL_MAGIC   >> 8) & 0xff) &&
                _data[2] == static_cast<uint8_t>( PROTOCOL_VERSION       & 0xff) &&
                _data[3] == static_cast<uint8_t>((PROTOCOL_VERSION >> 8) & 0xff);
    }

    uint16_t flags() {
        return static_cast<uint16_t>(_data[4] | (static_cast<uint16_t>(_data[5]) << 8)); 
    }

    template<typename ValType>
    bool get(uint16_t tag, ValType& out_value) const {
        size_t offset = sizeof(PacketHeader);
        while (not_reached_end(offset)) {

            if (offset + get_current_len(offset) + get_TL_lenght() > _size) {return false;} // paket boyutu aşılıyorsa çık
            if (get_current_tag(offset) != tag) {offset += get_current_len(offset) + get_TL_lenght();continue;} // doğru tag değilse atla

            return do_memcopy(offset, out_value); //doğru tag ise oku
        }
        return false; // tag pakette yok
    }

private:

    system::TagType get_current_tag(size_t &offset) const {return static_cast<uint16_t>(_data[offset+0] | (static_cast<uint16_t>(_data[offset+1]) << 8));}
    system::LenType get_current_len(size_t &offset) const {return static_cast<uint16_t>(_data[offset+2] | (static_cast<uint16_t>(_data[offset+3]) << 8));}
    size_t          get_TL_lenght  ()               const {return sizeof(system::TagType) + sizeof(system::LenType);}
    bool            not_reached_end(size_t &offset) const {return offset + get_TL_lenght() <= _size;}

    template<typename ValType>
    bool do_memcopy(size_t& offset, ValType& out_value) const {
        if (get_current_len(offset) == sizeof(ValType)) {
            std::memcpy(&out_value, _data + offset + get_TL_lenght(), sizeof(ValType));
            return true; // tag bulma başarılı
        } else return false; // boyut uygun değil
    }

    bool do_memcopy(size_t& offset, std::string& out_value) const {
        if (offset + get_TL_lenght() + get_current_len(offset) <= _size) {
            out_value.assign(reinterpret_cast<const char*>(_data + offset + get_TL_lenght()), get_current_len(offset));
            return true;
        } else return false;
    }

    const uint8_t*  _data;
    size_t          _size;
};

}

