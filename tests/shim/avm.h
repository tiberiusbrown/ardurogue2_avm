#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AVM_PROGMEM
#define PROGMEM
#define AVM_BUTTON_A 1
#define AVM_BUTTON_B 2
#define AVM_BUTTON_U 4
#define AVM_BUTTON_D 8
#define AVM_BUTTON_L 16
#define AVM_BUTTON_R 32
#define AVM_TEXT_BLACK_TRANSPARENT 0
#define AVM_TEXT_OVERWRITE 1
#define snprintf_P snprintf

struct AvmTextCursor { int16_t x; };
extern void (*avm_test_text_hook)(int16_t, int16_t, const char*);
inline AvmTextCursor avm_draw_text(int16_t x, int16_t y, const char* text)
{
    if(avm_test_text_hook) avm_test_text_hook(x, y, text);
    return {static_cast<int16_t>(x + strlen(text) * 4)};
}
inline AvmTextCursor avm_draw_text_P(int16_t x, int16_t y, const char* text)
{
    return avm_draw_text(x, y, text);
}
template<typename... Args>
inline AvmTextCursor avm_draw_textf_P(int16_t x, int16_t y, const char* format,
                                      Args... args)
{
    char text[48];
    snprintf(text, sizeof text, format, args...);
    if(avm_test_text_hook) avm_test_text_hook(x, y, text);
    return {static_cast<int16_t>(x + strlen(text) * 4)};
}
#if defined(AVM_TEST_LOADING)
extern unsigned avm_test_displays;
extern uint16_t avm_test_millis;
inline void avm_display(bool) { ++avm_test_displays; }
#else
inline void avm_display(bool) {}
#endif
extern uint8_t avm_test_buttons[16];
extern uint8_t avm_test_button_count;
extern uint8_t avm_test_button_index;
inline uint8_t avm_buttons() {
    return avm_test_button_index < avm_test_button_count
        ? avm_test_buttons[avm_test_button_index++] : 0;
}
inline void avm_idle() {}
#if defined(AVM_TEST_LOADING)
inline uint16_t avm_millis() { return avm_test_millis; }
#else
inline uint16_t avm_millis() { return 0; }
#endif
inline uint16_t avm_generate_random_seed() { return 0x4312; }

extern uint8_t __avm_framebuffer[1024];
inline void avm_test_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool white)
{
    for(int16_t py = y; py < y + h; ++py)
        for(int16_t px = x; px < x + w; ++px) {
            if(px < 0 || px >= 128 || py < 0 || py >= 64) continue;
            uint8_t& column = __avm_framebuffer[(py >> 3) * 128 + px];
            uint8_t mask = static_cast<uint8_t>(1u << (py & 7));
            if(white) column |= mask;
            else column &= static_cast<uint8_t>(~mask);
        }
}
inline void avm_draw_filled_rect_black(int16_t x, int16_t y, uint8_t w, uint8_t h)
{ avm_test_rect(x, y, w, h, false); }
inline void avm_draw_filled_rect_white(int16_t x, int16_t y, uint8_t w, uint8_t h)
{ avm_test_rect(x, y, w, h, true); }
inline void avm_draw_sprite_overwrite(int16_t, int16_t, const uint8_t*, uint8_t) {}
inline void avm_set_text_mode(uint8_t) {}
