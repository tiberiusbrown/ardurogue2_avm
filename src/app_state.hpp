#pragma once

#include <stdint.h>

namespace rogue {

enum Mode : uint8_t {
    TITLE, PLAY, MENU, THROW_DIRECTION, FULL_MAP, END
};

struct Ui {
    uint8_t mode, selection, previous_buttons, held_direction;
    uint16_t next_repeat_ms;
    bool has_save, dirty, repeat_suppressed;
};

extern Ui ui;

} // namespace rogue
