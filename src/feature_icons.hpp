#pragma once
#include "model.hpp"
namespace rogue {
inline constexpr uint8_t PROGMEM feature_icons[] = {
    4, 4,
    0x0, 0x0, 0x0, 0x0, // ID 0
    0, 0, 0, 0, // hidden frame
    0x8, 0xe, 0x8, 0xe, // ID 2
    0, 0, 0, 0, // hidden frame
    0x1, 0x6, 0x4, 0x8, // ID 4
    0, 0, 0, 0, // hidden frame
    0xc, 0x7, 0xe, 0x4, // ID 6
    0, 0, 0, 0, // hidden frame
    0x5, 0x2, 0xf, 0x2, // ID 8
    0, 0, 0, 0, // hidden frame
    0x9, 0x6, 0x6, 0x9, // ID 10
    0, 0, 0, 0, // hidden frame
    0x6, 0x9, 0x9, 0x6, // ID 12
    0, 0, 0, 0, // hidden frame
    0x5, 0xa, 0x5, 0xa, // ID 14
    0, 0, 0, 0, // hidden frame
    0x3, 0x1, 0xc, 0x8, // ID 16
    0, 0, 0, 0, // hidden frame
    0x9, 0xf, 0x6, 0x9, // ID 18
    0, 0, 0, 0, // hidden frame
    0xa, 0x5, 0xb, 0x4, // ID 20
    0, 0, 0, 0, // hidden frame
    0x8, 0xb, 0xf, 0x8, // ID 22
    0, 0, 0, 0, // hidden frame
    0xc, 0xa, 0xf, 0xc, // ID 24
    0, 0, 0, 0, // hidden frame
    0xc, 0x7, 0x7, 0xc, // ID 26
    0, 0, 0, 0, // hidden frame
    0xd, 0x6, 0x6, 0xd, // ID 28
    0, 0, 0, 0, // hidden frame
    0xe, 0xb, 0x3, 0xe, // ID 30
    0, 0, 0, 0, // hidden frame
    0x6, 0xf, 0x5, 0x6, // ID 32
    0, 0, 0, 0, // hidden frame
    0xf, 0x9, 0x9, 0xf, // ID 34
    0, 0, 0, 0, // hidden frame
    0xe, 0x9, 0xd, 0xe, // ID 36
    0, 0, 0, 0, // hidden frame
    0x8, 0xc, 0xa, 0x8, // ID 38
    0, 0, 0, 0, // hidden frame
    0x2, 0x7, 0xf, 0x2, // ID 40
    0, 0, 0, 0, // hidden frame
};
static_assert(sizeof(feature_icons) == 2 + 4 * (FEATURE_HOLY_SYMBOL + 2), "feature atlas missing frames");
constexpr bool valid_feature_icons() {
    for(uint8_t id = 0; id <= FEATURE_HOLY_SYMBOL; id += 2) {
        uint8_t visible = 0;
        for(uint8_t col = 0; col < 4; ++col) {
            visible |= feature_icons[2 + id * 4 + col];
            if(feature_icons[2 + (id + 1) * 4 + col]) return false;
        }
        if(id && !visible) return false;
    }
    return true;
}
static_assert(valid_feature_icons(), "feature visible/hidden frame mismatch");
} // namespace rogue
