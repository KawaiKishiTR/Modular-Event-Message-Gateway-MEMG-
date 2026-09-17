#pragma once

#include <cstdint>

namespace memg {

// Paket hizalamasını 1 bayta sabitle (Padding eklenmesini engelle)
#pragma pack(push, 1)

// Sistem ve Uygulama Komut Tipleri
enum class PacketType : uint16_t {
    // 0x0001 - 0x00FF: libmemg_ipc Dahili Kontrol Paketleri
    SUBSCRIBE       = 0x0001, // Bir servise abone olma isteği
    UNSUBSCRIBE     = 0x0002, // Abonelikten ayrılma isteği
    HEARTBEAT       = 0x0003, // Canlılık kontrolü
    HEARTBEAT_RESP  = 0x0004, // Canlılık yanıtı

    // 0x0100 - 0xFFFF: Aplikasyon Seviyesi Veri Paketleri
    APPLICATION_DATA = 0x0100  // RGB, Müzik spektrumu veya özel servis verisi
};

// Sabit Boyutlu Standart Başlık (8 Bayt)
struct PacketHeader {
    uint16_t   magic{0x4D47};                     // 'MG' (0x4D, 0x47)
    uint16_t   version{1};                        // Protokol sürümü
    PacketType type{PacketType::APPLICATION_DATA};// Komut veya veri tipi
    uint16_t   reserved{0};                       // Gelecek kullanımı için rezerv
};

// Dahili Komut: Kimlik / Servis Token'ı Belirten Paket
struct TokenPayload {
    char token[48]; // Servis kimliği (örn: "io.memg.service.rgb")
};

// Sabit Protokol Taşıyıcı Paketi (Header + 48 Byte Sabit Payload)
struct ControlPacket {
    PacketHeader header;
    union {
        uint8_t      data[48];
        TokenPayload subscriber;
    } payload;
};

#pragma pack(pop)

// Güvenlik Doğrulaması: Bellek boyutlarının tam beklenen byte'ta olduğunu garanti et
static_assert(sizeof(PacketHeader) == 8, "PacketHeader 8 bayt olmalidir!");
static_assert(sizeof(ControlPacket) == 56, "ControlPacket 56 bayt olmalidir (8B Header + 48B Payload)!");

} // namespace memg