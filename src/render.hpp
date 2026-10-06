#pragma once

#include <avm.h>
#include "model.hpp"
#include "inventory_view.hpp"

namespace rogue {

// Frame indices in the shared 4x4 item sprite sheet.
uint8_t item_icon(uint8_t type);
uint8_t mimic_icon(MimicAppearance appearance);
// Packed terrain occupancy for 13 screen columns, with opaque map clipping.
uint16_t wall_row_bits(uint8_t y, int16_t left);
void status_clear();
// Restore a modal's dungeon background only if it must be displayed mid-turn.
void defer_play_render();
void restore_play_render();
void render_play();
// Loading occupies the dungeon pane; the ordinary stats/status stay visible.
void begin_generation_render();
void update_generation_render(uint8_t percent = 255);
void end_generation_render();
void animate_ray(Position origin, int8_t dx, int8_t dy, uint8_t steps);
void animate_arrow(Position origin, int8_t dx, int8_t dy, uint8_t steps);
void animate_fire_burst(Position center);
void animate_spreading_rays(Position origin, const uint8_t steps[4]);
void animate_fire_bursts(const Position* centers, uint8_t count,
                         bool powerful);
void render_inventory(const char AVM_PROGMEM* prompt,
                      const InventoryView& view, uint8_t selection, uint8_t top,
                      uint8_t total);
void render_yesno_prompt(const char AVM_PROGMEM* prompt_text,
                         const Item* item);
void render();

} // namespace rogue
