#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AVM_PROGMEM
#define AVM_BUTTON_A 1
#define AVM_BUTTON_B 2
#define AVM_BUTTON_U 4
#define AVM_BUTTON_D 8
#define AVM_BUTTON_L 16
#define AVM_BUTTON_R 32
#define snprintf_P snprintf

struct AvmTextCursor { int16_t x; };
inline AvmTextCursor avm_draw_text(int16_t x, int16_t, const char* text)
{
    return {static_cast<int16_t>(x + strlen(text) * 4)};
}
inline AvmTextCursor avm_draw_text_P(int16_t x, int16_t y, const char* text)
{
    return avm_draw_text(x, y, text);
}
template<typename... Args>
inline AvmTextCursor avm_draw_textf_P(int16_t x, int16_t, const char* format,
                                      Args... args)
{
    char text[48];
    snprintf(text, sizeof text, format, args...);
    return {static_cast<int16_t>(x + strlen(text) * 4)};
}
inline void avm_display(bool) {}
inline uint8_t avm_buttons() { return 0; }
inline void avm_idle() {}
inline uint16_t avm_millis() { return 0; }
inline uint16_t avm_generate_random_seed() { return 0x4312; }
