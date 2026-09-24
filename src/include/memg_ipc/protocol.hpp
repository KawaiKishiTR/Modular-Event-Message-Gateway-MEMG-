#pragma once

#include <cstdint>
#include <vector>
#include <string>

/*
[ 2B Magic ('MG') ] [ 2B Version ] [ 2B Flags ] [ 2B Body Length ] = 8 BAYT
*/

namespace memg {

inline constexpr uint16_t PROTOCOL_MAGIC        = 0x4D47;
inline constexpr uint16_t PROTOCOL_VERSION      = 0x0002;
inline constexpr uint16_t PROTOCOL_TOKEN_SIZE   = 48;

typedef std::vector<uint8_t> MemgPacket;

// HEADER Flags
enum class PacketFlag : uint16_t {
    NONE        = 0x0000,
    ENCRYPTED   = 0x0001,
    COMPRESSED  = 0x0002,
    IN_SYSTEM   = 0x0004, // 1: sistem kontrol paketi 0: aplikasyon verisi 
};

inline PacketFlag operator+(PacketFlag a, PacketFlag b) {
    return static_cast<PacketFlag>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline PacketFlag operator-(PacketFlag a, PacketFlag b) {
    return static_cast<PacketFlag>(static_cast<uint16_t>(a) & static_cast<uint16_t>((~static_cast<uint16_t>(b))));
}

inline PacketFlag operator|(PacketFlag a, PacketFlag b) {
    return a + b;
}

inline bool has_flag(PacketFlag flags, PacketFlag target) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(target)) == static_cast<uint16_t>(target);
}


// Paket hizalamasını 1 bayta sabitle (Padding eklenmesini engelle)
#pragma pack(push, 1)

// PACKET HEADER
// Sabit Boyutlu Standart Başlık (8 Bayt)
struct PacketHeader {
    uint16_t   magic{PROTOCOL_MAGIC};             // 'MG' (0x4D, 0x47)
    uint16_t   version{PROTOCOL_VERSION};         // Protokol sürümü
    PacketFlag flags{PacketFlag::NONE};           // 2 bayt bayraklar
    uint16_t   body_size{0};                      // 2 bayt body size 
};


// Sabit Protokol Taşıyıcı Paketi (Header + 48 Byte Sabit Payload)
struct ControlPacket {
    PacketHeader    header;
    uint8_t         body[];
};

#pragma pack(pop)

// Güvenlik Doğrulaması: Bellek boyutlarının tam beklenen byte'ta olduğunu garanti et
static_assert(sizeof(PacketHeader) == 8, "PacketHeader 8 bayt olmalidir!");

} // namespace memg

namespace memg::system {
    
typedef uint16_t TagType;
typedef uint16_t LenType;
    
enum Tag : uint16_t {
    TOKEN       = 0x8001,
    COMMAND     = 0x8002,
};

enum Command : uint16_t {
    SUBSCRIBE       = 0x0001,
    UNSUBSCRIBE     = 0x0002,
    HEARTBEAT       = 0x0003,
    HEARTBEAT_RESP  = 0x0004,
};

} // namespace memg::system

#include "memg_ipc/PacketBuilder.hpp"
namespace memg::system {


inline MemgPacket make_heartbeat(const std::string& my_token) {
    return PacketBuilder(static_cast<uint16_t>(PacketFlag::IN_SYSTEM))
        .add(COMMAND, HEARTBEAT)
        .add_raw(TOKEN, my_token.data(), static_cast<uint16_t>(my_token.size()))
        .build();
}

inline MemgPacket make_heartbeat_response(const std::string& my_token) {
    return PacketBuilder(static_cast<uint16_t>(PacketFlag::IN_SYSTEM))
        .add(COMMAND, HEARTBEAT_RESP)
        .add_raw(TOKEN, my_token.data(), static_cast<uint16_t>(my_token.size()))
        .build();
}

inline MemgPacket make_subscribe(const std::string& my_token) {
    return PacketBuilder(static_cast<uint16_t>(PacketFlag::IN_SYSTEM))
        .add(COMMAND, SUBSCRIBE)
        .add_raw(TOKEN, my_token.data(), static_cast<uint16_t>(my_token.size()))
        .build();
}

inline MemgPacket make_unsubscribe(const std::string& my_token) {
    return PacketBuilder(static_cast<uint16_t>(PacketFlag::IN_SYSTEM))
        .add(COMMAND, UNSUBSCRIBE)
        .add_raw(TOKEN, my_token.data(), static_cast<uint16_t>(my_token.size()))
        .build();
}

} // namespace memg::system