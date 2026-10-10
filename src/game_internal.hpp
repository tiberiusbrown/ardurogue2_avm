#pragma once

#include "model.hpp"
#include "combat_math.hpp"

namespace rogue {

// Shared only by gameplay implementation files and world generation.
MonsterInfo monster_info(uint8_t type);
uint16_t monster_flags(uint8_t type);
uint8_t monster_strength(uint8_t type);
uint8_t monster_dexterity(uint8_t type);
uint8_t monster_speed(uint8_t type);
uint8_t monster_armor(uint8_t type);
uint8_t monster_health(uint8_t type);
uint8_t monster_xp(uint8_t type);
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
void damage_monster(uint8_t index, uint8_t damage, bool player_attack);
void hurt_player(uint8_t damage);
// Turn-free melee resolution; only successful hits activate weapon effects.
void attack_monster(uint8_t index);
void destroy_inventory_item(uint8_t slot);
void force_monster(uint8_t index, int8_t dx, int8_t dy, bool powerful = false);
void force_player(int8_t dx, int8_t dy);
uint8_t player_speed_cost();
// Artifact ring modifiers have fixed magnitude, independent of instance value.
int8_t artifact_ring_bonus(uint8_t type, int8_t positive, int8_t negative);
int8_t player_fire_effect();
uint8_t player_strength();
uint8_t player_dexterity();
uint8_t player_accuracy();
uint8_t player_ranged_accuracy(uint8_t bow_type);
uint8_t player_armor_rating();
int8_t player_armor_enchant();
void player_take_magic_damage(uint8_t damage, uint8_t power);
void player_take_fire_damage(uint8_t damage, uint8_t power);
void fire_burst_damage(Position center, bool player_attack,
                       uint8_t radius = 1);
void animate_arrow(Position origin, int8_t dx, int8_t dy, uint8_t steps);
void animate_ray(Position origin, int8_t dx, int8_t dy, uint8_t steps);
void animate_fire_burst(Position center);
void animate_spreading_rays(Position origin, const uint8_t steps[4]);
void animate_fire_bursts(const Position* centers, uint8_t count,
                         bool powerful);
void apply_monster_potion(uint8_t type, uint8_t index);

} // namespace rogue
