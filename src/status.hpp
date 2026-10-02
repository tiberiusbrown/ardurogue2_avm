#pragma once

#include "model.hpp"

namespace rogue {

constexpr uint8_t ITEM_TEXT_CAPACITY = 40;
void format_item(Item item, char (&buffer)[ITEM_TEXT_CAPACITY]);
void draw_item_text(int16_t x, int16_t y, Item item);
void reset_status_position();
uint8_t status_baseline();
// One-shot modifiers survive empty input and leading spaces.
void status_suffix(char c);
void status_capitalize();
// Emit one complete word; return the next nonempty word, or nullptr.
const char* status_word(const char* word);
void status_words(const char* words);
#if defined(__AVM__)
const char AVM_PROGMEM* status_word(const char AVM_PROGMEM* word);
void status_words(const char AVM_PROGMEM* words);
#endif
void status(const char PROGMEM* words);
void status(const char PROGMEM* words, char punctuation);
void status(Item item);
void status(Item item, char punctuation);
void status(MonsterType monster);
void status(MonsterType monster, char punctuation);
void status_number(uint8_t value);
void status_number(uint8_t value, char punctuation);

} // namespace rogue
