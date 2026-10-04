#pragma once

#include <stdint.h>

namespace rogue {

// 2*84 + 84 + 1 fits roll's nonzero uint8_t limit. Clamp both sides
// independently so accuracy retains twice evasion's weight at the boundary.
constexpr uint8_t MAX_PHYSICAL_STAT = 84;
constexpr int8_t MAX_EQUIPMENT_ENCHANT = 5;
constexpr int8_t MAX_ARMOR_ENCHANT = MAX_EQUIPMENT_ENCHANT;
constexpr uint8_t SWORD_MIN_DAMAGE = 2, SWORD_MAX_DAMAGE = 6;
constexpr uint8_t UNARMED_MIN_DAMAGE = 1, UNARMED_MAX_DAMAGE = 3;
uint8_t clamp_combat_stat(int16_t value);
bool physical_attack_hits(uint8_t accuracy, uint8_t evasion);
int8_t strength_damage_bonus(uint8_t strength);
uint8_t biased_range_roll(uint8_t minimum, uint8_t maximum, int8_t enchant);
uint8_t weapon_damage_roll(uint8_t minimum, uint8_t maximum, int8_t enchant);
uint8_t physical_raw_damage(uint8_t weapon_roll, uint8_t strength);
uint8_t effective_armor_rating(uint8_t rating, int8_t protection);
uint8_t armor_absorption(uint8_t rating, int8_t enchant);
uint8_t physical_damage_after_armor(uint8_t raw, uint8_t absorbed);
bool magic_save(uint8_t resistance, uint8_t power);
uint8_t magic_damage_after_save(uint8_t damage, bool saved);
uint8_t saturating_double(uint8_t value);

} // namespace rogue
