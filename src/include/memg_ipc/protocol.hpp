#pragma once

#include <cstdint>



namespace memg {

inline constexpr uint16_t PROTOCOL_MAGIC        = 0x4D47;
inline constexpr uint16_t PROTOCOL_VERSION      = 1;
inline constexpr uint16_t PROTOCOL_TOKEN_SIZE   = 48;


// Paket hizalamasını 1 bayta sabitle (Padding eklenmesini engelle)
#pragma pack(push, 1)

// =======================
//      PACKET HEADER
// =======================
enum class PacketFlag : uint16_t {
    NONE        = 0x0000,
    ENCRYPTED   = 0x0001,
    COMPRESSED  = 0x0002,
    MINIMALIZED = 0x0004,
    IN_SYSTEM   = 0x0008, // 1: sistem kontrol paketi 0: aplikasyon verisi 
    FORWARDED   = 0x0010,
    NESTED      = 0x0020, // payload içinde başka bir paket var
    SIGNED      = 0x0040  // imza / yetkilendirme doğrulaması gerekir
};

inline PacketFlag operator|(PacketFlag a, PacketFlag b) {
    return static_cast<PacketFlag>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline bool has_flag(PacketFlag flags, PacketFlag target) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(target)) == static_cast<uint16_t>(flags);
}



// ======================
//  SYSTEM COMMAND FLAGS
// ======================
enum class SystemCommand : uint16_t {
    SUBSCRIBE       = 0x0001,
    UNSUBSCRIBE     = 0x0002,
    HEARTBEAT       = 0x0003,
    HEARTBEAT_RESP  = 0x0004,
};


// Sabit Boyutlu Standart Başlık (8 Bayt)
struct PacketHeader {
    uint16_t   magic{PROTOCOL_MAGIC};             // 'MG' (0x4D, 0x47)
    uint16_t   version{PROTOCOL_VERSION};         // Protokol sürümü
    uint16_t   type{0};                           // Komut veya veri tipi
    PacketFlag flags{PacketFlag::NONE};           // 2 bayt bayraklar
};





// Dahili Komut: Kimlik / Servis Token'ı Belirten Paket
struct TokenPayload {
    char token[PROTOCOL_TOKEN_SIZE]; // Servis kimliği (örn: "io.memg.service.rgb")
};

typedef union PacketBody {
    uint8_t         data[PROTOCOL_TOKEN_SIZE];
    TokenPayload    sender;
} packetbody_t;





// Sabit Protokol Taşıyıcı Paketi (Header + 48 Byte Sabit Payload)
struct ControlPacket {
    PacketHeader    header;
    packetbody_t    body;
};

#pragma pack(pop)

// Güvenlik Doğrulaması: Bellek boyutlarının tam beklenen byte'ta olduğunu garanti et
static_assert(sizeof(PacketHeader) == 8, "PacketHeader 8 bayt olmalidir!");
static_assert(sizeof(ControlPacket) == 56, "ControlPacket 56 bayt olmalidir (8B Header + 48B Payload)!");

} // namespace memg