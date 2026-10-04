#pragma once

#include "model.hpp"

namespace rogue {

void start_new(uint16_t seed);
void finish(RunResult result);
uint8_t monster_at(Position pos);
uint8_t item_at(Position pos);
uint8_t ground_item_before(Position pos, uint8_t before);
Item ground_item_info(uint8_t index);
uint8_t player_max_hp();
uint8_t player_light_radius();
bool player_can_see_monster(uint8_t index);
void end_turn();
void move_player(int8_t dx, int8_t dy);
enum PickupResult : uint8_t { PICKUP_INVALID, PICKUP_TAKEN, PICKUP_NEEDS_SWAP };
PickupResult take_item(uint8_t index);
bool swap_ground_item(uint8_t index, uint8_t slot);
bool inventory_item_removable(uint8_t slot);
enum DropDisposition : uint8_t {
    DROP_INVALID, DROP_GROUND, DROP_DISCARD_ALL, DROP_DISCARD_REST
};
DropDisposition drop_disposition(uint8_t slot);
bool take_stairs();
void action();
bool use_inventory(uint8_t slot, uint8_t target_slot = NONE);
bool drop_inventory(uint8_t slot, bool discard = false);
bool throw_potion(uint8_t slot, int8_t dx, int8_t dy);
bool use_wand(uint8_t slot, int8_t dx, int8_t dy);
uint8_t monster_effect(const Monster& monster, MonsterEffect effect);
bool potion_identified(uint8_t type);
uint8_t potion_color(uint8_t type);
bool item_type_identified(uint8_t type);
uint8_t item_appearance(uint8_t type);

} // namespace rogue
