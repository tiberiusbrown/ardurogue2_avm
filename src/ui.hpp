#pragma once

#include <avm.h>
#include "inventory_view.hpp"

namespace rogue {

// Returns after movement so the main loop can prompt with the input frame unwound.
bool handle_input(uint8_t buttons);
void prompt_ground_items();
void prompt_stairs();

// Returns an inventory slot, or NONE when canceled or no item matches.
uint8_t choose_item(const char AVM_PROGMEM* prompt_text,
                    ItemTypeFilter item_type_filter);

// A confirms; B cancels. The optional item is formatted like status(Item).
bool yesno(const char AVM_PROGMEM* prompt_text);
bool yesno(const char AVM_PROGMEM* prompt_text, Item item);

} // namespace rogue
