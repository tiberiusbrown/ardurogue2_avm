#include "game.hpp"
#include "game_internal.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace rogue;
void require(bool condition, const char* reason);

static constexpr unsigned SAMPLES = 100000;

// Native-only counters; no simulator storage or arrays enter the AVM image.
static unsigned absorption_sample(int8_t enchant, bool print)
{
    unsigned counts[4] = {}, total = 0;
    game.random_state = 0x1234;
    for(unsigned i = 0; i < SAMPLES; ++i) {
        uint8_t block = armor_absorption(6, enchant);
        require(block >= 3 && block <= 6, "enchantment changed armor range");
        total += block;
        ++counts[block - 3];
    }
    for(unsigned count : counts)
        require(count > 0, "enchantment removed a possible absorption outcome");
    if(print)
        std::printf("armor 6 enchant %+d: mean %.4f; blocks 3..6: %u %u %u %u\n",
                    enchant, double(total) / SAMPLES,
                    counts[0], counts[1], counts[2], counts[3]);
    // Max probability is 1-(3/4)^(1+enchant), not an early 100% clamp.
    if(enchant >= 0) {
        const unsigned expected[] = {25000, 43750, 57813, 68359, 76270, 82202};
        require(counts[3] + 2000 > expected[enchant] &&
                counts[3] < expected[enchant] + 2000,
                "armor maximum probability differs from repeated-roll design");
    }
    return total;
}

void print_armor_distributions()
{
    for(int8_t enchant = -5; enchant <= 5; ++enchant)
        absorption_sample(enchant, true);
    // Compare identical raw damage under old flat armor and new absorption;
    // this quantifies the mechanic without changing monster numerical values.
    for(uint8_t rating : {0, 1, 3, 5, 8}) {
        unsigned total = 0;
        game.random_state = 0x1234;
        for(unsigned i = 0; i < SAMPLES; ++i)
            total += physical_damage_after_armor(7, armor_absorption(rating, 0));
        std::printf("raw 7 armor %u: flat %u; randomized mean damage %.4f\n",
                    rating, physical_damage_after_armor(7, rating), double(total) / SAMPLES);
    }
}

static void check_arithmetic()
{
    // All byte ratings, including the requested 0..6, 8 and overflow boundary.
    for(unsigned rating = 0; rating <= 255; ++rating) {
        for(int enchant : {-128, -5, -2, 0, 2, 5, 127}) {
            game.random_state = 0x1234;
            for(unsigned i = 0; i < 1000; ++i) {
                uint8_t block = armor_absorption(rating, enchant);
                require(block >= rating / 2 && block <= rating,
                        "armor absorption out of theoretical range");
            }
        }
    }
    unsigned totals[11];
    for(int8_t enchant = -5; enchant <= 5; ++enchant)
        totals[enchant + 5] = absorption_sample(enchant, false);
    require(totals[5] > 445000 && totals[5] < 455000 &&
            totals[6] > 507000 && totals[6] < 518000 &&
            totals[7] > 538000 && totals[7] < 550000 &&
            totals[8] > 556000 && totals[8] < 568000,
            "armor expected absorption differs from design");
    for(unsigned i = 1; i < 11; ++i)
        require(totals[i] > totals[i - 1], "enchantment did not improve mean");
    require(totals[6] - totals[5] > totals[7] - totals[6] &&
            totals[7] - totals[6] > totals[8] - totals[7] &&
            totals[5] - totals[4] > totals[4] - totals[3] &&
            totals[4] - totals[3] > totals[3] - totals[2],
            "positive or negative enchantment lacks diminishing returns");
    for(unsigned i = 0; i < 5; ++i) {
        unsigned paired = totals[i] + totals[10 - i];
        require(paired > 895000 && paired < 905000,
                "negative enchantment distribution is not symmetric");
    }
    for(unsigned raw = 0; raw <= 255; ++raw)
        for(unsigned absorbed = 0; absorbed <= 255; ++absorbed)
            require(physical_damage_after_armor(raw, absorbed) ==
                    (raw > absorbed ? raw - absorbed : 1),
                    "landed physical hit damage/minimum is incorrect");
    const int8_t bonuses[] = {-2, -2, -2, -1, -1, 0, 0, 1, 1, 2, 2, 3};
    for(unsigned strength = 0; strength <= 255; ++strength)
        require(strength_damage_bonus(strength) == bonuses[strength < 11 ? strength : 11],
                "STR damage modifier mapping changed");
    require(effective_armor_rating(6, 2) == 8 &&
            effective_armor_rating(6, -2) == 4 &&
            effective_armor_rating(1, -127) == 0 &&
            effective_armor_rating(255, 127) == 255,
            "protection rating adjustment overflowed");
    for(unsigned damage = 0; damage <= 255; ++damage) {
        require(magic_damage_after_save(damage, true) == (damage + 1) / 2 &&
                magic_damage_after_save(damage, false) == damage,
                "MR save failed to halve upward or retain full damage");
        require(saturating_double(damage) == (damage > 127 ? 255 : damage * 2),
                "cursed fire doubling overflowed");
    }
    for(int8_t enchant : {-5, 5}) {
        game.random_state = 0x9876;
        uint8_t expected = armor_absorption(6, enchant);
        uint16_t state = game.random_state;
        game.random_state = 0x9876;
        require(armor_absorption(6, enchant < 0 ? -128 : 127) == expected &&
                game.random_state == state, "enchantment roll cap changed");
    }
    unsigned low_accuracy = 0, high_accuracy = 0, high_evasion = 0;
    unsigned saves = 0, strong_saves = 0;
    game.random_state = 0x1234;
    for(unsigned i = 0; i < SAMPLES; ++i) {
        low_accuracy += physical_attack_hits(4, 4);
        high_accuracy += physical_attack_hits(8, 4);
        high_evasion += physical_attack_hits(4, 8);
        saves += magic_save(2, 8);
        strong_saves += magic_save(8, 8);
        require(!magic_save(0, 255), "zero MR saved against magic");
        physical_attack_hits(255, 255);
        magic_save(255, 255);
    }
    require(low_accuracy > 68000 && low_accuracy < 70500 &&
            high_accuracy > low_accuracy && high_evasion < low_accuracy,
            "DEX accuracy/evasion weighting changed");
    require(saves > 17000 && saves < 19500 && strong_saves > saves,
            "opposed magic save probability changed");
    require(clamp_combat_stat(-127) == 0 && clamp_combat_stat(382) == 84,
            "physical stat clamp changed");
}

static void combat_fixture()
{
    start_new(0x1234);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.inventory, 0, sizeof game.inventory);
    std::memset(game.walls, 0, sizeof game.walls);
    game.door_count = 0;
    game.player = {10, 10};
    game.hp = game.max_hp = 240;
    game.hunger = 255;
}

static void check_player_pipeline()
{
    combat_fixture();
    uint8_t strength = game.strength;
    gain_xp(200);
    require(game.level >= 4 && game.strength == strength &&
            game.magic_resistance == 2 + game.level / 4 &&
            player_accuracy() == 4 + (game.level - 1) / 3,
            "levels changed damage or failed to grant modest accuracy/MR");
    combat_fixture();
    game.inventory[0] = make_equipment(CHAIN_MAIL, 0);
    game.armor_slot = 0;
    game.inventory[1] = {RING_PROTECTION, 2};
    game.ring_slots[0] = 1;
    require(player_armor_rating() == 6, "positive protection did not add rating");
    game.inventory[1].info |= ITEM_CURSED;
    require(player_armor_rating() == 2, "cursed protection did not lower rating");
    game.armor_slot = NONE;
    require(player_armor_rating() == 0, "cursed protection underflowed rating");
    game.inventory[1].info &= ~ITEM_CURSED;
    require(player_armor_rating() == 2, "protection without armor lost its rating");
    game.inventory[1] = {RING_STRENGTH, 3};
    require(player_strength() == 8, "strength ring did not modify STR");
    game.weakened = 2;
    require(player_strength() == 6, "weakness did not modify effective STR");
    game.inventory[1].info |= ITEM_CURSED;
    game.weakened = 255;
    require(player_strength() == 1, "effective STR underflowed");
    game.weakened = 0;
    game.strength = 255;
    game.inventory[1].info &= ~ITEM_CURSED;
    require(player_strength() == 255, "effective STR overflowed");
    game.inventory[1] = {RING_DEXTERITY, 3};
    require(player_dexterity() == 7 && player_accuracy() == 7,
            "DEX ring failed to affect accuracy/evasion");
    game.inventory[1].info |= ITEM_CURSED;
    require(player_dexterity() == 1 && player_accuracy() == 1,
            "cursed DEX ring failed to reduce accuracy/evasion");
    game.dexterity = 255;
    require(player_dexterity() == MAX_PHYSICAL_STAT, "DEX overflowed");
    combat_fixture();
    game.inventory[0] = {RING_ATTACK, 3};
    game.ring_slots[0] = 0;
    require(player_accuracy() == 7 && player_dexterity() == 4 &&
            player_strength() == 5, "attack ring changed stats beyond accuracy");
    game.inventory[0].info |= ITEM_CURSED;
    require(player_accuracy() == 1, "cursed attack ring did not lower accuracy");

    // Paired seeds verify actual player damage entry points, independent of
    // physical equipment and stats, for both successful and failed MR saves.
    bool saw_save = false, saw_failure = false;
    for(uint16_t seed = 1; seed <= 200; ++seed) {
        combat_fixture();
        game.random_state = seed;
        bool saved = magic_save(2, 8);
        saw_save |= saved;
        saw_failure |= !saved;
        uint8_t expected = saved ? 5 : 9;
        game.random_state = seed;
        player_take_magic_damage(9, 8);
        require(game.hp == 240 - expected, "magic damage entry point ignored MR");
        game.hp = 240;
        game.dexterity = game.strength = 255;
        game.inventory[0] = make_equipment(CHAIN_MAIL, 5);
        game.armor_slot = 0;
        game.inventory[1] = {RING_PROTECTION, 63};
        game.inventory[2] = {RING_FIRE_IMMUNITY, 1};
        game.ring_slots[0] = 1;
        game.ring_slots[1] = 2;
        game.random_state = seed;
        player_take_magic_damage(9, 8);
        require(game.hp == 240 - expected,
                "physical armor, protection, DEX, STR or fire immunity changed magic");
        game.hp = 240;
        game.random_state = seed;
        player_take_fire_damage(9, 8);
        require(game.hp == 240 && game.random_state == seed,
                "fire immunity did not prevent damage before MR");
        game.inventory[2].info |= ITEM_CURSED;
        player_take_fire_damage(9, 8);
        require(game.hp == 240 - (saved ? 9 : 18),
                "cursed fire vulnerability did not double before MR");
        game.hp = 240;
        game.random_state = seed;
        player_take_magic_damage(9, 8);
        require(game.hp == 240 - expected, "cursed fire ring affected generic magic");
        game.hp = 240;
        game.random_state = seed;
        player_take_fire_damage(255, 8);
        require(game.hp == (saved ? 112 : 0), "large fire damage wrapped");
    }
    require(saw_save && saw_failure, "magic pipeline test did not cover both saves");

    // Same seeded absorption and accuracy with other stats changed; raw damage
    // changes only with STR/sword. These helpers are the live melee pipeline.
    for(uint16_t seed = 1; seed <= 200; ++seed) {
        combat_fixture();
        game.inventory[0] = make_equipment(CHAIN_MAIL, 0);
        game.armor_slot = 0;
        game.random_state = seed;
        uint8_t block = armor_absorption(player_armor_rating(), 0);
        game.random_state = seed;
        uint8_t raw = physical_raw_damage(2, player_strength());
        game.random_state = seed;
        bool hit = physical_attack_hits(player_accuracy(), 4);
        game.strength = 11;
        game.random_state = seed;
        require(physical_attack_hits(player_accuracy(), 4) == hit,
                "STR affected physical accuracy");
        game.random_state = seed;
        require(physical_raw_damage(2, player_strength()) == raw + 3,
                "STR failed to affect raw physical damage");
        game.dexterity = 12;
        game.magic_resistance = 127;
        game.random_state = seed;
        require(armor_absorption(player_armor_rating(), 0) == block,
                "DEX, STR or MR affected absorption");
        game.random_state = seed;
        uint8_t strong_raw = physical_raw_damage(2, player_strength());
        game.inventory[1] = {RING_ATTACK, 63};
        game.ring_slots[0] = 1;
        game.random_state = seed;
        require(physical_raw_damage(2, player_strength()) == strong_raw,
                "DEX, MR or attack ring affected raw damage");
    }
}

static void check_live_melee()
{
    // With zero-armor targets, the live unarmed path must expose 1..3,
    // and the -2 STR modifier must clamp every landed unarmed hit to 1.
    unsigned unarmed_outcomes[4] = {};
    for(uint16_t seed = 1; seed <= 1000; ++seed) {
        uint8_t damage[2];
        for(unsigned variant = 0; variant < 2; ++variant) {
            combat_fixture();
            game.monsters[0] = {{11, 10}, BAT, 100, 255, {0, 0}, 0};
            game.random_state = seed;
            if(variant) game.strength = 1;
            move_player(1, 0);
            damage[variant] = static_cast<uint8_t>(100 - game.monsters[0].hp);
        }
        require(damage[0] <= 3 && damage[1] == (damage[0] ? 1 : 0),
                "live unarmed range or weak-STR minimum changed");
        ++unarmed_outcomes[damage[0]];
    }
    require(unarmed_outcomes[1] && unarmed_outcomes[2] && unarmed_outcomes[3],
            "live unarmed damage did not expose its full inherent range");

    combat_fixture();
    game.inventory[0] = make_equipment(LONG_SWORD, 2);
    game.weapon_slot = 0;
    game.monsters[0] = {{11, 10}, ORC, 100, 255, {0, 0}, 0};
    Game fixture = game;
    unsigned hits[6] = {}, total_damage[6] = {};
    for(uint16_t seed = 1; seed <= 1000; ++seed) {
        uint8_t damage[6];
        for(unsigned variant = 0; variant < 6; ++variant) {
            game = fixture;
            session.ended = false;
            game.random_state = seed;
            if(variant == 1) game.strength = 11;
            if(variant == 2) game.dexterity = 12;
            if(variant == 3) {
                game.inventory[1] = {RING_ATTACK, 4};
                game.ring_slots[0] = 1;
            }
            if(variant == 4) set_equipment_enchant(game.inventory[0], 5);
            if(variant == 5) set_equipment_enchant(game.inventory[0], -5);
            move_player(1, 0);
            damage[variant] = static_cast<uint8_t>(100 - game.monsters[0].hp);
            hits[variant] += damage[variant] != 0;
            total_damage[variant] += damage[variant];
        }
        require((damage[0] != 0) == (damage[1] != 0),
                "live melee STR changed whether the player hit");
        require((damage[0] != 0) == (damage[4] != 0) &&
                (damage[0] != 0) == (damage[5] != 0),
                "weapon enchantment changed physical accuracy");
        if(damage[0])
            require(damage[1] > damage[0], "live melee STR failed to increase damage");
        if(damage[0] && damage[2])
            require(damage[0] == damage[2], "live melee DEX changed damage on a hit");
        if(damage[0] && damage[3])
            require(damage[0] == damage[3], "live attack ring added damage");
    }
    require(hits[2] > hits[0] && hits[3] > hits[0] &&
            total_damage[1] > total_damage[0], "live player combat stats failed");
    require(total_damage[4] > total_damage[0] && total_damage[0] > total_damage[5],
            "live melee ignored signed weapon enchantment");

    combat_fixture();
    game.monsters[0] = {{11, 10}, GOBLIN, 100, 0, {0, 0}, 0};
    fixture = game;
    unsigned unarmored_hits = 0, evasive_hits = 0;
    unsigned high_enchant_damage = 0, low_enchant_damage = 0;
    for(uint16_t seed = 1; seed <= 1000; ++seed) {
        uint8_t damage[6];
        for(unsigned variant = 0; variant < 6; ++variant) {
            game = fixture;
            session.ended = false;
            game.random_state = seed;
            if(variant == 1) {
                game.inventory[0] = make_equipment(CHAIN_MAIL, 5);
                game.inventory[1] = {RING_PROTECTION, 63};
                game.ring_slots[0] = 1;
                game.armor_slot = 0;
            }
            if(variant == 2) {
                game.inventory[0] = {RING_DEXTERITY, 8};
                game.ring_slots[0] = 0;
            }
            if(variant == 3) {
                game.strength = 255;
                game.magic_resistance = 127;
            }
            if(variant >= 4) {
                game.inventory[0] = make_equipment(CHAIN_MAIL, variant == 4 ? 5 : -5);
                game.armor_slot = 0;
                require(player_armor_rating() == 4 &&
                        player_armor_enchant() == (variant == 4 ? 5 : -5),
                        "live armor confused inherent rating with enchantment");
            }
            end_turn();
            damage[variant] = static_cast<uint8_t>(240 - game.hp);
        }
        unarmored_hits += damage[0] != 0;
        evasive_hits += damage[2] != 0;
        require(damage[0] == damage[3], "STR or MR changed incoming melee damage");
        require((damage[4] != 0) == (damage[0] != 0) &&
                (damage[5] != 0) == (damage[0] != 0),
                "armor enchantment changed physical evasion");
        high_enchant_damage += damage[4];
        low_enchant_damage += damage[5];
        require(damage[1] == (damage[0] ? 1 : 0),
                "live armor changed evasion or failed landed-hit minimum");
        if(damage[0] && damage[2])
            require(damage[0] == damage[2], "DEX ring changed incoming damage on a hit");
    }
    require(evasive_hits < unarmored_hits, "live DEX ring failed to improve evasion");
    require(high_enchant_damage < low_enchant_damage,
            "live monster damage ignored signed armor enchantment");
}

void check_combat_rules()
{
    check_arithmetic();
    check_player_pipeline();
    check_live_melee();
}
