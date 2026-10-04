#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "inventory_view.hpp"
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace rogue;
void require(bool condition, const char* reason);
static constexpr unsigned SAMPLES = 100000;

struct WeaponCase { uint8_t type, minimum, maximum; int8_t accuracy; };
static constexpr WeaponCase weapons[] = {
    {DAGGER, 1, 4, 2}, {SPEAR, 2, 5, 1}, {LONG_SWORD, 2, 6, 0},
    {MACE, 3, 7, -1}, {TWO_HANDED_SWORD, 4, 8, -2}
};
static constexpr uint8_t armors[] = {
    LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, CHAIN_MAIL, SPLINT_MAIL, PLATE_MAIL
};
static constexpr uint8_t equipment_types[] = {
    DAGGER, SPEAR, LONG_SWORD, MACE, TWO_HANDED_SWORD,
    LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, CHAIN_MAIL, SPLINT_MAIL, PLATE_MAIL
};

static bool expected_weapon(uint8_t type)
{
    for(const auto& weapon : weapons) if(weapon.type == type) return true;
    return false;
}

static bool expected_armor(uint8_t type)
{
    for(uint8_t armor : armors) if(armor == type) return true;
    return false;
}

static uint8_t expected_rating(uint8_t type)
{
    for(unsigned i = 0; i < sizeof armors; ++i)
        if(armors[i] == type) return static_cast<uint8_t>(i + 1);
    return 0;
}

static void check_equipment_definitions_and_ranges()
{
    static_assert(LONG_SWORD == 12 && TWO_HANDED_SWORD - LONG_SWORD + 1 == 5 &&
                  PLATE_MAIL - CHAIN_MAIL + 1 == 6 &&
                  POTION_COUNT == 10 && RING_COUNT == 8 && AMULET_COUNT == 8 &&
                  SCROLL_COUNT == 9 && WAND_COUNT == 7,
                  "item groups are no longer contiguous or changed size");
    for(const auto& weapon : weapons) {
        WeaponDefinition definition = weapon_definition(weapon.type);
        require(definition.minimum_damage == weapon.minimum &&
                definition.maximum_damage == weapon.maximum && definition.accuracy == weapon.accuracy,
                "weapon definition differs from the mundane roster");
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            for(unsigned seed = 1; seed <= 2048; ++seed) {
                game.random_state = static_cast<uint16_t>(seed);
                Item item = make_equipment(weapon.type, enchant);
                uint8_t damage = weapon_damage_roll(definition.minimum_damage,
                    definition.maximum_damage, equipment_enchant(item));
                require(damage >= weapon.minimum && damage <= weapon.maximum &&
                        weapon_definition(item.type).accuracy == weapon.accuracy,
                        "enchantment changed weapon damage bounds or accuracy");
            }
        }
    }
    for(uint8_t type : armors) {
        uint8_t rating = expected_rating(type);
        require(armor_definition(type).rating == rating, "armor definition differs from the mundane roster");
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            for(unsigned seed = 1; seed <= 2048; ++seed) {
                game.random_state = static_cast<uint16_t>(seed);
                Item item = make_equipment(type, enchant);
                uint8_t absorbed = armor_absorption(armor_definition(item.type).rating,
                    equipment_enchant(item));
                require(absorbed >= rating / 2 && absorbed <= rating &&
                        armor_definition(item.type).rating == rating,
                        "enchantment changed armor rating or absorption bounds");
            }
        }
    }
}

static unsigned weapon_sample(int8_t enchant, bool print)
{
    unsigned counts[5] = {}, total = 0;
    game.random_state = 0x1234;
    for(unsigned i = 0; i < SAMPLES; ++i) {
        WeaponDefinition sword = weapon_definition(LONG_SWORD);
        uint8_t damage = weapon_damage_roll(sword.minimum_damage, sword.maximum_damage, enchant);
        require(damage >= 2 && damage <= 6, "enchantment changed sword capability");
        total += damage;
        ++counts[damage - 2];
    }
    require(counts[4] < SAMPLES && counts[0] < SAMPLES,
            "capped enchantment made weapon damage deterministic");
    if(print)
        std::printf("sword 2..6 enchant %+d: mean %.4f; damage 2..6: %u %u %u %u %u\n",
                    enchant, double(total) / SAMPLES,
                    counts[0], counts[1], counts[2], counts[3], counts[4]);
    return total;
}

void print_weapon_distributions()
{
    for(int8_t enchant = -5; enchant <= 5; ++enchant)
        weapon_sample(enchant, true);
}

static void check_weapon_math()
{
    for(uint16_t seed = 1; seed <= 200; ++seed) {
        for(int8_t enchant : {-5, 0, 5}) {
            game.random_state = seed;
            uint8_t expected = biased_range_roll(2, 6, enchant);
            uint16_t state = game.random_state;
            game.random_state = seed;
            require(weapon_damage_roll(2, 6, enchant) == expected && game.random_state == state,
                    "weapon wrapper changed the shared biased roll");
            game.random_state = seed;
            expected = biased_range_roll(3, 6, enchant);
            state = game.random_state;
            game.random_state = seed;
            require(armor_absorption(6, enchant) == expected && game.random_state == state,
                    "armor wrapper changed the shared biased roll");
        }
    }
    unsigned totals[11];
    for(int8_t enchant = -5; enchant <= 5; ++enchant)
        totals[enchant + 5] = weapon_sample(enchant, false);
    for(unsigned i = 1; i < 11; ++i)
        require(totals[i] > totals[i - 1], "weapon enchantment did not improve mean");
    require(totals[5] > 397000 && totals[5] < 403000 &&
            totals[6] > 477000 && totals[6] < 483000 &&
            totals[7] > 517000 && totals[7] < 523000,
            "weapon means differ from repeated uniform rolls");
    require(totals[6] - totals[5] > totals[7] - totals[6] &&
            totals[7] - totals[6] > totals[8] - totals[7] &&
            totals[5] - totals[4] > totals[4] - totals[3] &&
            totals[4] - totals[3] > totals[3] - totals[2],
            "positive or negative weapon enchantment lacks diminishing returns");
    for(unsigned i = 0; i < 5; ++i)
        require(totals[i] + totals[10 - i] > 795000 &&
                totals[i] + totals[10 - i] < 805000,
                "weapon disadvantage is not symmetric with advantage");

    // All initial RNG states cover the endpoints even at the effective cap.
    for(int8_t enchant : {-5, -2, 0, 2, 5}) {
        bool saw_min = false, saw_max = false;
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            game.random_state = static_cast<uint16_t>(seed);
            uint8_t damage = weapon_damage_roll(2, 6, enchant);
            require(damage >= 2 && damage <= 6, "sword roll escaped fixed bounds");
            saw_min |= damage == 2;
            saw_max |= damage == 6;
        }
        require(saw_min && saw_max, "enchantment removed an inherent weapon endpoint");
    }
    for(int8_t enchant : {-5, 5}) {
        game.random_state = 0x9876;
        uint8_t damage = weapon_damage_roll(2, 6, enchant);
        uint16_t state = game.random_state;
        game.random_state = 0x9876;
        require(weapon_damage_roll(2, 6, enchant < 0 ? -128 : 127) == damage &&
                game.random_state == state, "weapon enchantment cap changed roll count");
    }
    for(unsigned i = 0; i < 10000; ++i) {
        uint8_t unarmed = weapon_damage_roll(UNARMED_MIN_DAMAGE, UNARMED_MAX_DAMAGE, 0);
        require(unarmed >= 1 && unarmed <= 3, "unarmed roll escaped its range");
        weapon_damage_roll(0, 255, 5); // The full byte range must not call roll(0).
        require(weapon_damage_roll(250, 255, -5) >= 250,
                "high damage range wrapped");
    }
    require(weapon_damage_roll(20, 12, 5) == 12 &&
            weapon_damage_roll(255, 255, -128) == 255,
            "invalid or constant damage range handling changed");
    for(unsigned damage = 0; damage <= 255; ++damage) {
        for(unsigned strength = 0; strength <= 255; ++strength) {
            int expected = static_cast<int>(damage) + strength_damage_bonus(strength);
            if(expected < 1) expected = 1;
            if(expected > 255) expected = 255;
            uint16_t state = game.random_state;
            require(physical_raw_damage(damage, strength) == expected &&
                    game.random_state == state,
                    "STR did not modify the completed weapon roll independently");
        }
    }
}

static void check_equipment_encoding()
{
    for(unsigned type = 0; type <= 255; ++type) {
        require(is_weapon(type) == expected_weapon(type) && is_armor(type) == expected_armor(type) &&
                is_equipment(type) == (expected_weapon(type) || expected_armor(type)),
                "equipment predicates classify unrelated item types");
        if(is_equipment(type))
            require(inventory_group(type) == (is_weapon(type) ? WEAPONS : ARMORS),
                    "equipment does not use its generic inventory group");
    }
    // The same info byte must decode and update identically for either type,
    // including unused codes, without escaping the cap or changing flags.
    for(unsigned info = 0; info <= 255; ++info) {
        for(int8_t enchant : {-128, -5, -2, 0, 2, 5, 127}) {
            Item sword{LONG_SWORD, static_cast<uint8_t>(info)};
            Item armor{CHAIN_MAIL, static_cast<uint8_t>(info)};
            int8_t decoded = equipment_enchant(sword);
            require(decoded == equipment_enchant(armor) &&
                    decoded >= -MAX_EQUIPMENT_ENCHANT && decoded <= MAX_EQUIPMENT_ENCHANT,
                    "equipment types decode enchantment differently or outside the cap");
            require(armor_definition(armor.type).rating == 4 && armor_definition(sword.type).rating == 0,
                    "instance info changed inherent armor capability");
            WeaponDefinition definition = weapon_definition(sword.type);
            require(definition.minimum_damage == 2 && definition.maximum_damage == 6 &&
                    definition.accuracy == 0,
                    "instance info changed inherent weapon capability");
            set_equipment_enchant(sword, enchant);
            set_equipment_enchant(armor, enchant);
            int8_t expected = enchant < -MAX_EQUIPMENT_ENCHANT ? -MAX_EQUIPMENT_ENCHANT :
                enchant > MAX_EQUIPMENT_ENCHANT ? MAX_EQUIPMENT_ENCHANT : enchant;
            require(sword.info == armor.info && equipment_enchant(sword) == expected &&
                    (sword.info & ITEM_VALUE_MASK) == expected + MAX_EQUIPMENT_ENCHANT &&
                    (sword.info & ~ITEM_VALUE_MASK) == (info & ~ITEM_VALUE_MASK),
                    "equipment types update enchantment differently or change flags");
        }
    }
    for(uint8_t type : equipment_types) {
        for(unsigned info = 0; info <= 255; ++info) {
            Item item{type, static_cast<uint8_t>(info)};
            Item baseline{LONG_SWORD, static_cast<uint8_t>(info)};
            require(equipment_enchant(item) == equipment_enchant(baseline),
                    "equipment subtype changed enchantment decoding");
            set_equipment_enchant(item, -2);
            set_equipment_enchant(baseline, -2);
            require(item.type == type && item.info == baseline.info,
                    "equipment subtype changed instance encoding or flags");
        }
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            for(uint8_t flags : {uint8_t(0), ITEM_CURSED, ITEM_IDENTIFIED,
                                 uint8_t(ITEM_CURSED | ITEM_IDENTIFIED)}) {
                Item item = make_equipment(type, enchant);
                item.info |= flags;
                require(equipment_enchant(item) == enchant &&
                        (item.info & ~ITEM_VALUE_MASK) == flags &&
                        armor_definition(item.type).rating == expected_rating(type),
                        "equipment enchantment/flags changed inherent capability");
                set_equipment_enchant(item, -128);
                require(equipment_enchant(item) == -5 &&
                        (item.info & ~ITEM_VALUE_MASK) == flags,
                        "negative enchant cap changed equipment flags");
                set_equipment_enchant(item, 127);
                require(equipment_enchant(item) == 5 &&
                        (item.info & ~ITEM_VALUE_MASK) == flags &&
                        armor_definition(item.type).rating == expected_rating(type),
                        "positive enchant cap changed equipment capability or flags");
            }
        }
    }
    WeaponDefinition sword = weapon_definition(LONG_SWORD);
    WeaponDefinition unarmed = weapon_definition(NO_ITEM);
    require(sword.minimum_damage == 2 && sword.maximum_damage == 6 && sword.accuracy == 0 &&
            unarmed.minimum_damage == 1 && unarmed.maximum_damage == 3 && unarmed.accuracy == 0,
            "weapon type definitions lost inherent damage or accuracy");
    require(armor_definition(FOOD).rating == 0 && equipment_enchant({FOOD, 63}) == 0,
            "non-equipment acquired combat stats");
}

static void equipment_fixture()
{
    start_new(0x1234);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.inventory, 0, sizeof game.inventory);
    std::memset(game.ground, 0, sizeof game.ground);
    game.hunger = 255;
}

static void check_equipment_actions()
{
    for(const auto& weapon : weapons) {
        equipment_fixture();
        game.level = 7;
        game.inventory[1] = {RING_ATTACK, 3};
        game.inventory[2] = {RING_DEXTERITY, 2};
        game.ring_slots[0] = 1;
        game.ring_slots[1] = 2;
        game.weapon_slot = 0;
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            game.inventory[0] = make_equipment(weapon.type, enchant);
            require(player_accuracy() == game.dexterity + 2 + 2 + weapon.accuracy + 3,
                    "live accuracy omitted weapon type or included enchantment");
            game.inventory[0].info |= ITEM_CURSED;
            require(player_accuracy() == game.dexterity + 2 + 2 + weapon.accuracy + 3,
                    "weapon curse changed inherent accuracy");
        }
    }
    for(uint8_t type : armors) {
        equipment_fixture();
        game.armor_slot = 0;
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            game.inventory[0] = make_equipment(type, enchant);
            require(player_armor_rating() == expected_rating(type) && player_armor_enchant() == enchant,
                    "live armor rating or enchantment ignored the subtype definition");
        }
    }
    for(uint8_t type : equipment_types) {
        equipment_fixture();
        game.inventory[0] = make_equipment(type, -2);
        game.inventory[1] = make_equipment(type, -1);
        require(use_inventory(0) && (is_weapon(type) ? game.weapon_slot : game.armor_slot) == 0,
                "generic equipment did not use its designated slot");
        action();
        require(session.repeat_slot == 0 && equipment_enchant(game.inventory[0]) == -2,
                "repeat action failed for negative non-cursed equipment");
        require(use_inventory(1) && drop_inventory(0),
                "negative non-cursed equipment was not replaceable/removable");
    }
    for(uint8_t type : equipment_types) {
        for(int8_t enchant : {-5, 0, 5}) {
            equipment_fixture();
            game.inventory[0] = make_equipment(type, enchant);
            game.inventory[0].info |= ITEM_CURSED;
            require(use_inventory(0) && item_is_cursed(game.inventory[0]) &&
                    equipment_enchant(game.inventory[0]) == enchant,
                    "equipping cursed gear changed enchantment");
            game.inventory[2] = make_equipment(type, 0);
            require(!use_inventory(2) && !drop_inventory(0),
                    "cursed gear could be replaced or dropped");
            game.ground[0] = {game.player, make_equipment(type, -2)};
            require(!swap_ground_item(0, 0), "cursed gear could be exchanged");
            game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
            require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
                    game.inventory[0].type == type &&
                    equipment_enchant(game.inventory[0]) == enchant &&
                    (!is_armor(type) || armor_definition(game.inventory[0].type).rating == expected_rating(type)),
                    "remove curse changed positive, zero or negative enchantment");
            game.inventory[0].info |= ITEM_CURSED;
            for(unsigned scroll = 0; scroll < 12; ++scroll) {
                game.inventory[1] = {SCROLL_ENCHANT, 1};
                int8_t expected = enchant < 5 ? ++enchant : 5;
                require(use_inventory(1, 0) &&
                        game.inventory[0].type == type &&
                        game.inventory[1].type == NO_ITEM &&
                        equipment_enchant(game.inventory[0]) == expected &&
                        item_is_cursed(game.inventory[0]) &&
                        item_is_identified(game.inventory[0]) &&
                        (!is_armor(type) || armor_definition(game.inventory[0].type).rating == expected_rating(type)),
                        "enchant scroll changed capability/curse or failed to saturate");
            }
            game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
            require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
                    equipment_enchant(game.inventory[0]) == 5 &&
                    (!is_armor(type) || armor_definition(game.inventory[0].type).rating == expected_rating(type)),
                    "remove curse changed equipment enchantment or rating");
            require(use_inventory(2), "remove curse did not permit equipment replacement");
        }
    }
    // Raw info zero represents -MAX_EQUIPMENT_ENCHANT, not an empty stack.
    for(int8_t enchant : {-MAX_EQUIPMENT_ENCHANT, 0}) {
        equipment_fixture();
        game.inventory[0] = make_equipment(LONG_SWORD, enchant);
        require(drop_inventory(0) && game.ground[0].item.type == LONG_SWORD &&
                equipment_enchant(game.ground[0].item) == enchant,
                "dropping a zero-code or zero-enchant sword silently discarded it");
        require(take_item(0) == PICKUP_TAKEN && game.inventory[0].type == LONG_SWORD,
                "zero-code or zero-enchant sword could not be picked up");
    }

    for(uint8_t type : equipment_types) {
        for(int8_t enchant : {-5, 0, 5}) {
            equipment_fixture();
            Item item = make_equipment(type, enchant);
            item.info |= ITEM_CURSED | ITEM_IDENTIFIED;
            game.inventory[0] = item; // Cursed gear is removable before equipping.
            require(drop_inventory(0) && game.ground[0].item.type == type &&
                    game.ground[0].item.info == item.info &&
                    take_item(0) == PICKUP_TAKEN &&
                    game.inventory[0].type == type && game.inventory[0].info == item.info,
                    "equipment pickup/drop lost rating, enchantment or flags");
        }
    }

    // The same item can be cursed with negative enchantment or uncursed with it.
    equipment_fixture();
    game.inventory[0] = make_equipment(CHAIN_MAIL, -3);
    game.inventory[0].info |= ITEM_CURSED;
    game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
    require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
            equipment_enchant(game.inventory[0]) == -3 && armor_definition(game.inventory[0].type).rating == 4,
            "remove curse erased a negative enchantment");
}

static void check_generated_equipment()
{
    unsigned counts[2][5][2] = {}, depth_counts[FLOORS][2][5] = {};
    unsigned subtype_counts[11][5][2] = {}, category_counts[INVENTORY_GROUPS] = {};
    unsigned generated = 0;
    unsigned appearances[MIMIC_APPEARANCE_COUNT] = {};
    for(uint16_t seed = 1; seed <= 4096; ++seed) {
        start_new(seed);
        for(uint8_t floor = 0; floor < FLOORS; ++floor) {
            game.floor = floor;
            make_floor();
            for(const Monster& monster : game.monsters) {
                if(monster.type != MIMIC) continue;
                require((monster.state & ~MIMIC_APPEARANCE_MASK) == 0 &&
                        (monster.state >> MIMIC_APPEARANCE_SHIFT) < MIMIC_APPEARANCE_COUNT,
                        "generated mimic state is not a semantic appearance");
                ++appearances[mimic_appearance(monster)];
            }
            for(const GroundItem& ground : game.ground) {
                const Item& item = ground.item;
                if(item.type == NO_ITEM) continue;
                ++generated;
                ++category_counts[inventory_group(item.type)];
                if(!is_equipment(item.type)) continue;
                int8_t enchant = equipment_enchant(item);
                require(enchant >= -2 && enchant <= 2 && !item_is_identified(item) &&
                        (item.info & ITEM_VALUE_MASK) == enchant + MAX_EQUIPMENT_ENCHANT &&
                        armor_definition(item.type).rating == expected_rating(item.type),
                        "generated equipment violates instance state or type capability");
                ++counts[is_weapon(item.type) ? 0 : 1][enchant + 2][item_is_cursed(item) ? 1 : 0];
                ++depth_counts[floor][is_weapon(item.type) ? 0 : 1][enchant + 2];
                for(unsigned subtype = 0; subtype < sizeof equipment_types; ++subtype)
                    if(item.type == equipment_types[subtype])
                        ++subtype_counts[subtype][enchant + 2][item_is_cursed(item) ? 1 : 0];
            }
        }
    }
    const unsigned expected_percent[5] = {5, 10, 70, 10, 5};
    for(const auto& equipment : counts) {
        unsigned total = 0, cursed = 0;
        for(const auto& bucket : equipment) {
            total += bucket[0] + bucket[1];
            cursed += bucket[1];
        }
        require(total > 10000, "equipment generation sample is too small");
        require(cursed * 100 > total * 10 && cursed * 100 < total * 15,
                "equipment curse probability differs from one eighth");
        for(unsigned i = 0; i < 5; ++i) {
            unsigned bucket = equipment[i][0] + equipment[i][1];
            require(bucket * 100 > total * (expected_percent[i] - 2) &&
                    bucket * 100 < total * (expected_percent[i] + 2),
                    "equipment enchantment distribution differs from 5/10/70/10/5");
            require(equipment[i][1] * 100 > bucket * 8 &&
                    equipment[i][1] * 100 < bucket * 17,
                    "equipment curse probability depends on enchantment");
        }
    }
    // Original category weights out of 72; equipment expansion changes only subtypes.
    const unsigned category_weights[] = {8, 8, 2, 2, 4, 20, 8, 20};
    for(unsigned category = 0; category < 8; ++category)
        require(category_counts[category] * 7200 > generated * (category_weights[category] * 100 - 72) &&
                category_counts[category] * 7200 < generated * (category_weights[category] * 100 + 72),
                "subtype generation changed overall loot category probability");
    const unsigned subtype_weights[] = {25, 20, 30, 15, 10, 25, 20, 20, 15, 12, 8};
    for(unsigned subtype = 0; subtype < sizeof equipment_types; ++subtype) {
        unsigned total = 0, category_total = 0, cursed = 0;
        for(const auto& bucket : subtype_counts[subtype]) {
            total += bucket[0] + bucket[1];
            cursed += bucket[1];
        }
        for(const auto& bucket : counts[subtype < 5 ? 0 : 1])
            category_total += bucket[0] + bucket[1];
        require(total > 500 && total * 100 > category_total * (subtype_weights[subtype] - 2) &&
                total * 100 < category_total * (subtype_weights[subtype] + 2),
                "equipment subtype omitted or incorrectly weighted");
        require(cursed * 100 > total * 8 && cursed * 100 < total * 17,
                "equipment curse probability depends on subtype");
        for(unsigned enchant = 0; enchant < 5; ++enchant) {
            const auto& bucket = subtype_counts[subtype][enchant];
            require((bucket[0] + bucket[1]) * 100 > total * (expected_percent[enchant] - 4) &&
                    (bucket[0] + bucket[1]) * 100 < total * (expected_percent[enchant] + 4) &&
                    bucket[0] && bucket[1],
                    "subtype determines enchantment or curse/enchantment combinations are missing");
            require(bucket[1] * 100 > (bucket[0] + bucket[1]) * 6 &&
                    bucket[1] * 100 < (bucket[0] + bucket[1]) * 20,
                    "equipment curse probability depends on subtype/enchantment together");
        }
    }
    for(const auto& depth : depth_counts) {
        for(const auto& equipment : depth) {
            unsigned total = 0;
            for(unsigned count : equipment) total += count;
            for(unsigned enchant = 0; enchant < 5; ++enchant)
                require(equipment[enchant] * 100 > total * (expected_percent[enchant] - 4) &&
                        equipment[enchant] * 100 < total * (expected_percent[enchant] + 4),
                        "enchantment distribution depends on floor number");
        }
    }
    for(unsigned count : appearances)
        require(count != 0, "generation omitted a mimic appearance category");
}

void check_weapon_and_equipment_rules()
{
    check_equipment_definitions_and_ranges();
    check_weapon_math();
    check_equipment_encoding();
    check_equipment_actions();
    check_generated_equipment();
}
