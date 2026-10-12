#pragma once
#include "model.hpp"

namespace rogue {
inline const char PROGMEM* spell_name(uint8_t spell) {
    switch(spell) {
    case MAGIC_DART: return F("Magic Dart");
    case FROSTBIND: return F("Frostbind");
    case REPULSION: return F("Repulsion");
    case CONFOUND: return F("Confound");
    case SENSE_TRAPS: return F("Sense Traps");
    case BLINK: return F("Blink");
    case SHOCKWAVE: return F("Shockwave");
    case PIERCING_BOLT: return F("Piercing Bolt");
    case BANISH: return F("Banish");
    case EXCHANGE: return F("Exchange");
    case CLEANSE: return F("Cleanse");
    default: return F("Unknown");
    }
}
inline bool studying() { return session.study_slot != NONE; }
inline const char PROGMEM* spell_description(uint8_t spell) {
    switch(spell) {
    case MAGIC_DART: return F("Range 5. First hit takes 2-4 magic damage.");
    case FROSTBIND: return F("Range 5. Slow the first hit for 4-6 turns.");
    case REPULSION: return F("Range 5. Push the first hit 1-2 tiles.");
    case CONFOUND: return F("Range 5. Confuse the first hit for 4-6 turns.");
    case SENSE_TRAPS: return F("Reveal traps within radius 4; leave them active.");
    case BLINK: return F("Random teleport within radius 3, through walls.");
    case SHOCKWAVE: return F("Push adjacent monsters outward 2-3 tiles.");
    case PIERCING_BOLT: return F("Range 5. First two hits take 3-5 damage each.");
    case BANISH: return F("Range 5. Teleport the first hit elsewhere.");
    case EXCHANGE: return F("Swap with a visible monster within radius 3.");
    case CLEANSE: return F("Remove blindness, confusion or slowing; one only.");
    default: return F("Unknown spell.");
    }
}
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
