#pragma once

#include <memg_ipc/PacketBuilder.hpp>
#include <memg_ipc/PacketReader.hpp>
#include <cstdint>

namespace memg::apis::rgb {

inline constexpr uint16_t RGB_API_VERSION = 0x0001; 

// RGB Servisine Özel TLV Tag Tanımları
enum Tag : system::TagType {
    API_VERSION     = 0xffff,
    MODE            = 0x0001, // Çalışma Modu (Statik, Nefes Alma, Müzik)
    RED             = 0x000a,
    GREEN           = 0x000b,
    BLUE            = 0x000c,
    RGB             = 0x000d,
    BRIGHTNESS      = 0x000e,
};

enum Mode : uint8_t {
    STATIC    = 0,
    SPECTRUM  = 1,
    WALLPAPER = 2,
};

#pragma pack(push, 1)
struct ColorRGB {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
};
#pragma pack(pop)

inline PacketBuilder make_packet() {
    PacketBuilder builder;
    builder.add(API_VERSION, RGB_API_VERSION);
    return builder;
}

// 1. Statik Renk Paketi Üretici (Helper Factory)
inline MemgPacket make_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t br) {
    ColorRGB col{r, g, b};
    return make_packet()
        .add(MODE, static_cast<uint8_t>(Mode::STATIC))
        .add(RGB, col)
        .add(BRIGHTNESS, br)
        .build();
}

// 2. mode değiştirme paketi yapıcı
inline MemgPacket make_set_mode(Mode mode) {
    return make_packet()
        .add(MODE, static_cast<uint8_t>(mode))
        .build();
}

} // namespace memg::apis::rgb