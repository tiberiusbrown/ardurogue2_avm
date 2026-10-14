#pragma once
// Agent preferences use only public spell metadata and displayed character state.
namespace sim {
inline bool study_budget_available(uint32_t known, uint8_t floor) {
    uint8_t learned = 0;
    for(uint8_t s = 1; s < rogue::SPELL_COUNT; ++s) learned += (known & rogue::spell_bit(s)) != 0;
    return learned < 2 + floor / 2;
}
inline int spell_utility(uint8_t spell, uint32_t known, uint8_t floor, bool bow) {
    using namespace rogue;
    bool control = known & (spell_bit(CONFOUND) | spell_bit(FROSTBIND) | spell_bit(SLUMBER));
    bool mobility = known & (spell_bit(BLINK) | spell_bit(PHASE_STEP) | spell_bit(EXCHANGE));
    switch(spell) {
    case SUMMON_HOUND: return known & (spell_bit(SUMMON_SPIDER)|spell_bit(SUMMON_WISP)|spell_bit(SUMMON_GOLEM)) ? 70 : floor < 8 ? 160 : 130;
    case SUMMON_SPIDER: return floor >= 8 ? 145 : known & spell_bit(SUMMON_HOUND) ? 60 : 125;
    case SUMMON_WISP: return floor >= 7 ? 155 : 65;
    case SUMMON_GOLEM: return floor >= 12 ? 150 : 65;
    case BLINK: return mobility ? 140 : 210;
    case PHASE_STEP: return mobility ? (floor >= 7 ? 150 : 105) : 190;
    case EXCHANGE: return mobility ? (floor >= 6 ? 135 : 85) : 185;
    case SHOCKWAVE: return floor >= 4 ? 170 : 100;
    case BANISH: return floor >= 5 ? 175 : 110;
    case CLEANSE: return 165;
    case SLUMBER: return control ? (floor >= 8 ? 135 : 95) : 155;
    case CONFOUND: return control ? (floor >= 5 ? 90 : 60) : 150;
    case FROSTBIND: return control ? (floor >= 5 ? 90 : 55) : 140;
    case REPULSION: return mobility ? (floor >= 5 ? 90 : 65) : 125;
    case SENSE_TRAPS: return floor >= 4 ? 100 : 70;
    case PIERCING_BOLT: return bow ? (floor >= 6 ? 100 : 60) : floor >= 6 ? 140 : 80;
    case CHAIN_LIGHTNING: return floor >= 8 ? 170 : 85;
    case FIREBURST: return floor >= 10 ? 160 : 75;
    case BLOOD_BOLT: return floor >= 8 ? 125 : 55;
    default: return 0;
    }
}
inline int study_priority(uint8_t spell, uint32_t known, uint8_t floor, bool bow, bool legacy = false) {
    uint8_t duration = rogue::spell_study_turns(spell, legacy);
    return duration ? spell_utility(spell, known, floor, bow) * 40 / duration : 0;
}
inline uint8_t preferred_book_spell(uint8_t book, uint32_t known, uint8_t floor, bool bow, bool legacy = false, uint8_t maximum_mana = 16) {
    uint8_t best = rogue::NONE;
    int score = 0;
    for(uint8_t s = 1; s < rogue::SPELL_COUNT; ++s) {
        if(!(rogue::unknown_book_spells(book, known) & rogue::spell_bit(s))) continue;
        if(rogue::summon_spell(s) && rogue::spell_cost(s,legacy)>maximum_mana) continue;
        int utility = study_priority(s, known, floor, bow, legacy);
        if(utility > score) { best = s; score = utility; }
    }
    return best;
}
inline uint8_t summon_preference(uint8_t rank,uint8_t floor) {
    using namespace rogue;
    constexpr uint8_t early[]={SUMMON_HOUND,SUMMON_SPIDER,SUMMON_WISP,SUMMON_GOLEM};
    constexpr uint8_t middle[]={SUMMON_WISP,SUMMON_SPIDER,SUMMON_HOUND,SUMMON_GOLEM};
    constexpr uint8_t late[]={SUMMON_GOLEM,SUMMON_WISP,SUMMON_SPIDER,SUMMON_HOUND};
    return floor>=11 ? late[rank] : floor>=6 ? middle[rank] : early[rank];
}
inline int remaining_book_value(uint8_t book, uint32_t known, uint8_t floor, bool bow, bool legacy = false) {
    int best = 0, total = 0;
    for(uint8_t s = 1; s < rogue::SPELL_COUNT; ++s) {
        if(!(rogue::unknown_book_spells(book, known) & rogue::spell_bit(s))) continue;
        int value = study_priority(s, known, floor, bow, legacy);
        if(value < 80) continue; // Value only members worth their time at this stage.
        if(value > best) best = value;
        total += value;
    }
    return best + (total - best) / 10;
}
}
