#pragma once
#include "model.hpp"

namespace rogue {
inline const char PROGMEM* spell_name(uint8_t spell) {
    switch(spell) {
#define SPELL_NAME(id, name, mp, hp, turns, target, category, weight, desc) case id: return F(name);
    SPELL_DEFINITIONS(SPELL_NAME)
#undef SPELL_NAME
    default: return F("Unknown");
    }
}
inline bool studying() { return session.study_slot != NONE; }
inline const char PROGMEM* spell_description(uint8_t spell) {
    switch(spell) {
#define SPELL_DESC(id, name, mp, hp, turns, target, category, weight, desc) case id: return F(desc);
    SPELL_DEFINITIONS(SPELL_DESC)
#undef SPELL_DESC
    default: return F("Unknown spell.");
    }
}
// Flash-resident profiles; the caller owns resources and damage attribution.
struct LightningProfile { uint8_t range, targets, minimum[3], spread[3]; };
extern const LightningProfile PROGMEM LIGHTNING_SPELL, LIGHTNING_WAND, LIGHTNING_POWERFUL;
void lightning_ray(int8_t dx, int8_t dy, const LightningProfile PROGMEM* profile, uint16_t& hit_mask);
uint8_t lightning_target(Position origin, uint16_t hit_mask);
// Returns true only for the first genuine output during this study.
inline bool study_status_output() {
    if(!studying() || session.study_message) return false;
    session.study_message = true;
    return true;
}
uint8_t player_max_mana();
void regenerate_mana();
bool begin_study(uint8_t slot);
bool advance_study(); // Exactly one ordinary turn, frontend renders after return.
void cancel_study(bool announce = true);
bool exchange_target(uint8_t index);
bool cast_spell(uint8_t spell, int8_t dx = 0, int8_t dy = 0, uint8_t target = NONE);
// Tile-by-tile forced entry. Returns distance before blockers/trap displacement.
uint8_t knockback_monster(uint8_t index, int8_t dx, int8_t dy, uint8_t distance);
void generate_spellbooks(); // Isolated postprocessing, after artifact placement.
}
