#pragma once

#include <avm.h>
#include "inventory_view.hpp"

namespace rogue {

// Return wand/projectile actions so the main loop activates them after this UI frame
// unwinds, leaving room for dungeon rendering on the 256-byte VM stack.
enum InputAction : uint8_t {
    INPUT_NONE, INPUT_MOVED, INPUT_WAND_UP, INPUT_WAND_RIGHT,
    INPUT_WAND_DOWN, INPUT_WAND_LEFT, INPUT_WAND_IMMEDIATE,
    INPUT_PROJECTILE_UP, INPUT_PROJECTILE_RIGHT, INPUT_PROJECTILE_DOWN, INPUT_PROJECTILE_LEFT
};
InputAction handle_input(uint8_t buttons);
void prompt_ground_items();
void prompt_stairs();

// Returns an inventory slot, or NONE when canceled or no item matches.
uint8_t choose_item(const char AVM_PROGMEM* prompt_text,
                    ItemTypeFilter item_type_filter);

// A confirms; B cancels. The optional item is formatted like status(Item).
bool yesno(const char AVM_PROGMEM* prompt_text);
bool yesno(const char AVM_PROGMEM* prompt_text, Item item);

} // namespace rogue
