#pragma once

#include <stdint.h>

namespace rogue {

enum Mode : uint8_t {
    TITLE, PLAY, MENU, PROJECTILE_DIRECTION, WAND_DIRECTION, FULL_MAP, END, OBJECT_MAP, SPELL_MENU, SPELL_DIRECTION, SPELL_TARGET
};

struct Ui {
    Mode mode;
    uint8_t selection, previous_buttons, held_direction;
    uint16_t next_repeat_ms;
    bool has_save, dirty, repeat_suppressed;
    uint8_t use_target = 0xff;
};

extern Ui ui;

} // namespace rogue
