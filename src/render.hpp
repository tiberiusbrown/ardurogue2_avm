#pragma once

#include <avm.h>
#include "model.hpp"
#include "inventory_view.hpp"

namespace rogue {

void status_clear();
void render_play();
void animate_ray(Position origin, int8_t dx, int8_t dy, uint8_t steps);
void animate_fire_burst(Position center);
void render_inventory(const char AVM_PROGMEM* prompt,
                      const InventoryView& view, uint8_t selection, uint8_t top);
void render_yesno_prompt(const char AVM_PROGMEM* prompt_text,
                         const Item* item);
void render();

} // namespace rogue
