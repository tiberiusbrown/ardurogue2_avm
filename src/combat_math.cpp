#include "combat_math.hpp"

namespace rogue {

// The only dependency of the combat arithmetic is the existing seeded RNG.
uint8_t roll(uint8_t limit);

uint8_t clamp_combat_stat(int16_t value)
{
    if(value < 0) return 0;
    if(value > MAX_PHYSICAL_STAT) return MAX_PHYSICAL_STAT;
    return static_cast<uint8_t>(value);
}

bool physical_attack_hits(uint8_t accuracy, uint8_t evasion)
{
    accuracy = clamp_combat_stat(accuracy);
    evasion = clamp_combat_stat(evasion);
    return roll(static_cast<uint8_t>(accuracy * 2u + evasion + 1u)) >= evasion;
}

int8_t strength_damage_bonus(uint8_t strength)
{
    if(strength <= 2) return -2;
    if(strength <= 4) return -1;
    if(strength <= 6) return 0;
    if(strength <= 8) return 1;
    if(strength <= 10) return 2;
    return 3;
}

uint8_t physical_raw_damage(uint8_t weapon_value, uint8_t strength)
{
    // Preserve the old starting base of 2, plus generic sword value and 0..2.
    int16_t damage = 2 + static_cast<int16_t>(weapon_value) +
        strength_damage_bonus(strength) + roll(3);
    if(damage < 1) return 1;
    if(damage > 255) return 255;
    return static_cast<uint8_t>(damage);
}

uint8_t effective_armor_rating(uint8_t rating, int8_t protection)
{
    // Protection changes the rating fed to absorption, never subtracts HP.
    int16_t value = static_cast<int16_t>(rating) + protection;
    if(value < 0) return 0;
    if(value > 255) return 255;
    return static_cast<uint8_t>(value);
}

uint8_t armor_absorption(uint8_t rating, int8_t enchant)
{
    if(!rating) return 0;
    uint8_t base = rating / 2;
    uint8_t range = static_cast<uint8_t>(rating - base + 1);
    if(enchant > MAX_ARMOR_ENCHANT) enchant = MAX_ARMOR_ENCHANT;
    if(enchant < -MAX_ARMOR_ENCHANT) enchant = -MAX_ARMOR_ENCHANT;
    uint8_t count = static_cast<uint8_t>(1 + (enchant < 0 ? -enchant : enchant));
    uint8_t extra = roll(range);
    while(--count) {
        uint8_t candidate = roll(range);
        if((enchant > 0 && candidate > extra) ||
           (enchant < 0 && candidate < extra))
            extra = candidate;
    }
    return static_cast<uint8_t>(base + extra);
}

uint8_t physical_damage_after_armor(uint8_t raw, uint8_t absorbed)
{
    return raw > absorbed ? static_cast<uint8_t>(raw - absorbed) : 1;
}

bool magic_save(uint8_t resistance, uint8_t power)
{
    // Each side caps at 127: sum + 1 is at most 255 and never zero.
    if(resistance > 127) resistance = 127;
    if(power > 127) power = 127;
    return roll(static_cast<uint8_t>(resistance + power + 1u)) < resistance;
}

uint8_t magic_damage_after_save(uint8_t damage, bool saved)
{
    return saved ? static_cast<uint8_t>(damage / 2 + damage % 2) : damage;
}

uint8_t saturating_double(uint8_t value)
{
    return value > 127 ? 255 : static_cast<uint8_t>(value * 2u);
}

} // namespace rogue
