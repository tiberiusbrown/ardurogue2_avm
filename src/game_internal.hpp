#pragma once

#include "model.hpp"

namespace rogue {

// Shared only by gameplay implementation files and world generation.
MonsterInfo monster_info(uint8_t type);
uint16_t next_random(uint16_t& state);
uint8_t roll(uint8_t limit);
int8_t ring_bonus(uint8_t type);
int8_t amulet_bonus(uint8_t type);
bool player_is_invisible();
void heal_player(uint8_t amount);
void gain_xp(uint8_t amount);
void identify_type(uint8_t type);
void identify_item(uint8_t slot);
void monster_status(const Monster& monster, const char PROGMEM* message);
void set_monster_effect(Monster& monster, MonsterEffect effect,
                        uint8_t duration);
void defeat_monster(uint8_t index);
void apply_monster_potion(uint8_t type, uint8_t index);

} // namespace rogue
