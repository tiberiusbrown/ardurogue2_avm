#pragma once

#include "model.hpp"

namespace rogue::generation {

enum Family : uint8_t {
    SMALL, MEDIUM, LARGE, SHORT_PASSAGE, LONG_PASSAGE, BENT_PASSAGE,
    L_CHAMBER, CROSS_CHAMBER, T_CHAMBER, DOUBLE_CHAMBER, PILLARED_HALL,
    GALLERY, ALCOVE, WIDE_HALL, IRREGULAR_A, IRREGULAR_B, FAMILIES
};

// Base bitmaps have no border; clearance has an extra cell on every side.
// All eight transforms are enabled initially. The mask allows individual
// templates to restrict rotations/reflections without storing more bitmaps.
struct Template {
    uint8_t w, h, transforms, reserved;
    uint16_t carve[12];
    uint16_t clearance[14];
    uint8_t padding[8]; // 64-byte stride avoids AVM wide address multiplies.
};

constexpr Template buffered(Template t)
{
    for(uint8_t y = 0; y < t.h + 2; ++y) {
        uint16_t row = 0;
        for(int8_t dy = -1; dy <= 1; ++dy) {
            int8_t source = static_cast<int8_t>(y) - 1 + dy;
            if(source >= 0 && source < t.h) row |= t.carve[source];
        }
        t.clearance[y] = static_cast<uint16_t>(row | (row << 1) | (row << 2));
    }
    return t;
}

static constexpr Template PROGMEM templates[] = {
    buffered({9, 9, 0xff, 0, {0x00f,0x00f,0x00f,0x00f,0x00f,0x1ff,0x1ff,0x1ff,0x1ff}, {}, {}}),
    buffered({9, 9, 0xff, 0, {0x038,0x038,0x038,0x1ff,0x1ff,0x1ff,0x038,0x038,0x038}, {}, {}}),
    buffered({11, 8, 0xff, 0, {0x7ff,0x7ff,0x7ff,0x0f8,0x0f8,0x0f8,0x0f8,0x0f8}, {}, {}}),
    buffered({13, 7, 0xff, 0, {0x1f1f,0x1f1f,0x1fff,0x1fff,0x1fff,0x1f1f,0x1f1f}, {}, {}}),
    buffered({13, 9, 0xff, 0, {0x1fff,0x1fff,0x1dbb,0x1fff,0x1fff,0x1fff,0x1dbb,0x1fff,0x1fff}, {}, {}}),
    buffered({14, 4, 0xff, 0, {0x3fff,0x3fff,0x3fff,0x3fff}, {}, {}}),
    buffered({4, 4, 0xff, 0, {0x00f,0x00f,0x00f,0x007}, {}, {}}),
    buffered({11, 6, 0xff, 0, {0x7ff,0x7ff,0x7ff,0x7ff,0x7ff,0x7ff}, {}, {}}),
    buffered({10, 9, 0xff, 0, {0x03c,0x07e,0x0ff,0x1ff,0x3fc,0x3fe,0x1fe,0x0f8,0x078}, {}, {}}),
    buffered({11, 8, 0xff, 0, {0x07f,0x07f,0x3ff,0x7fe,0x7fe,0x7f8,0x3f8,0x078}, {}, {}})
};
static_assert(sizeof(Template) == 64, "template stride must be a power of two");
static_assert(sizeof templates / sizeof templates[0] == FAMILIES - L_CHAMBER,
              "feature template table mismatch");

// A power-of-two stride keeps the flash lookup to shifts and adds.
struct SocketRows { uint16_t rows[4][16]; };

constexpr uint16_t template_row(const Template PROGMEM& t, int8_t y)
{
    return y < 0 || y >= t.h ? 0 : t.carve[y];
}

constexpr SocketRows socket_rows(const Template PROGMEM& t)
{
    SocketRows result = {};
    for(uint8_t dir = 0; dir < 4; ++dir)
        for(int8_t y = 0; y < t.h; ++y) {
            uint16_t row = t.carve[y];
            if(dir == 0) result.rows[dir][y] = static_cast<uint16_t>(row & (row >> 1) & ~(row << 1) & ~(row << 2));
            else if(dir == 2) result.rows[dir][y] = static_cast<uint16_t>(row & (row << 1) & ~(row >> 1) & ~(row >> 2));
            else {
                int8_t forward = dir == 1 ? 1 : -1;
                result.rows[dir][y] = static_cast<uint16_t>(row & template_row(t, y + forward) &
                    ~template_row(t, y - forward) & ~template_row(t, y - 2 * forward));
            }
        }
    return result;
}

static constexpr SocketRows PROGMEM template_sockets[] = {
    socket_rows(templates[0]), socket_rows(templates[1]), socket_rows(templates[2]),
    socket_rows(templates[3]), socket_rows(templates[4]), socket_rows(templates[5]),
    socket_rows(templates[6]), socket_rows(templates[7]), socket_rows(templates[8]),
    socket_rows(templates[9])
};
static_assert(sizeof(SocketRows) == 128, "socket row stride");

struct Style {
    uint8_t weights[FAMILIES];
    uint16_t coverage;
    uint8_t loop_min, loop_range, detour, door_chance, door_limit;
};
static constexpr Style PROGMEM styles[4] = {
    {{12,22,12,12,7,9,5,4,3,3,3,2,2,2,1,1}, 710, 2,3,16,70,8},
    {{29,6,1,14,15,16,2,1,1,1,1,1,9,1,1,1}, 590, 1,3,18,55,7},
    {{8,19,15,10,8,4,2,7,6,3,5,6,1,5,0,1}, 780, 4,4,12,94,11},
    {{12,15,8,10,7,9,7,3,2,3,3,1,3,2,8,7}, 680, 2,4,14,50,7}
};

constexpr uint8_t total_weight(const Style PROGMEM& style)
{
    uint8_t total = 0;
    for(uint8_t weight : style.weights) total += weight;
    return total;
}

static constexpr uint8_t PROGMEM style_weight_totals[] = {
    total_weight(styles[0]), total_weight(styles[1]),
    total_weight(styles[2]), total_weight(styles[3])
};

} // namespace rogue::generation
