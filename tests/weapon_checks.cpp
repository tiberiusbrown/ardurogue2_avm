#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace rogue;
void require(bool condition, const char* reason);
static constexpr unsigned SAMPLES = 100000;

static unsigned weapon_sample(int8_t enchant, bool print)
{
    unsigned counts[5] = {}, total = 0;
    game.random_state = 0x1234;
    for(unsigned i = 0; i < SAMPLES; ++i) {
        uint8_t damage = weapon_damage_roll(SWORD_MIN_DAMAGE, SWORD_MAX_DAMAGE, enchant);
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
    // The same info byte must decode and update identically for either type,
    // including unused codes, without escaping the cap or changing flags.
    for(unsigned info = 0; info <= 255; ++info) {
        for(int8_t enchant : {-128, -5, -2, 0, 2, 5, 127}) {
            Item sword{SWORD, static_cast<uint8_t>(info)};
            Item armor{ARMOR, static_cast<uint8_t>(info)};
            int8_t decoded = equipment_enchant(sword);
            require(decoded == equipment_enchant(armor) &&
                    decoded >= -MAX_EQUIPMENT_ENCHANT && decoded <= MAX_EQUIPMENT_ENCHANT,
                    "equipment types decode enchantment differently or outside the cap");
            set_equipment_enchant(sword, enchant);
            set_equipment_enchant(armor, enchant);
            int8_t expected = enchant < -MAX_EQUIPMENT_ENCHANT ? -MAX_EQUIPMENT_ENCHANT :
                enchant > MAX_EQUIPMENT_ENCHANT ? MAX_EQUIPMENT_ENCHANT : enchant;
            require(sword.info == armor.info && equipment_enchant(sword) == expected &&
                    (sword.info & ~ITEM_VALUE_MASK) == (info & ~ITEM_VALUE_MASK),
                    "equipment types update enchantment differently or change flags");
        }
    }
    for(uint8_t type : {SWORD, ARMOR}) {
        for(uint8_t rating = 1; rating <= MAX_ITEM_ARMOR_RATING; ++rating) {
            for(int8_t enchant = -5; enchant <= 5; ++enchant) {
                for(uint8_t flags : {uint8_t(0), ITEM_CURSED, ITEM_IDENTIFIED,
                                     uint8_t(ITEM_CURSED | ITEM_IDENTIFIED)}) {
                    Item item = make_equipment(type, enchant, rating);
                    item.info |= flags;
                    require(equipment_enchant(item) == enchant &&
                            (item.info & ~ITEM_VALUE_MASK) == flags &&
                            (type != ARMOR || armor_rating(item) == rating),
                            "equipment rating/enchantment/flags overlap");
                    set_equipment_enchant(item, -128);
                    require(equipment_enchant(item) == -5 &&
                            (item.info & ~ITEM_VALUE_MASK) == flags &&
                            (type != ARMOR || armor_rating(item) == rating),
                            "negative enchant cap changed armor rating or flags");
                    set_equipment_enchant(item, 127);
                    require(equipment_enchant(item) == 5 &&
                            (item.info & ~ITEM_VALUE_MASK) == flags,
                            "positive enchant cap changed equipment flags");
                    if(type == ARMOR) {
                        set_armor_rating(item, 255);
                        require(armor_rating(item) == MAX_ITEM_ARMOR_RATING &&
                                equipment_enchant(item) == 5,
                                "armor rating setter changed enchantment or overflowed");
                    }
                }
            }
        }
    }
    require(armor_rating({FOOD, 63}) == 0 && equipment_enchant({FOOD, 63}) == 0,
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
    for(uint8_t type : {SWORD, ARMOR}) {
        for(int8_t enchant : {-5, 0, 5}) {
            equipment_fixture();
            game.inventory[0] = make_equipment(type, enchant, 4);
            game.inventory[0].info |= ITEM_CURSED;
            require(use_inventory(0) && item_is_cursed(game.inventory[0]) &&
                    equipment_enchant(game.inventory[0]) == enchant,
                    "equipping cursed gear changed enchantment");
            game.inventory[2] = make_equipment(type, 0, 2);
            require(!use_inventory(2) && !drop_inventory(0),
                    "cursed gear could be replaced or dropped");
            game.ground[0] = {game.player, make_equipment(type, -2, 1)};
            require(!swap_ground_item(0, 0), "cursed gear could be exchanged");
            game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
            require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
                    equipment_enchant(game.inventory[0]) == enchant &&
                    (type != ARMOR || armor_rating(game.inventory[0]) == 4),
                    "remove curse changed positive, zero or negative enchantment");
            game.inventory[0].info |= ITEM_CURSED;
            for(unsigned scroll = 0; scroll < 12; ++scroll) {
                game.inventory[1] = {SCROLL_ENCHANT, 1};
                int8_t expected = enchant < 5 ? ++enchant : 5;
                require(use_inventory(1, 0) &&
                        game.inventory[1].type == NO_ITEM &&
                        equipment_enchant(game.inventory[0]) == expected &&
                        item_is_cursed(game.inventory[0]) &&
                        item_is_identified(game.inventory[0]) &&
                        (type != ARMOR || armor_rating(game.inventory[0]) == 4),
                        "enchant scroll changed capability/curse or failed to saturate");
            }
            game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
            require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
                    equipment_enchant(game.inventory[0]) == 5 &&
                    (type != ARMOR || armor_rating(game.inventory[0]) == 4),
                    "remove curse changed equipment enchantment or rating");
            require(use_inventory(2), "remove curse did not permit equipment replacement");
        }
    }
    // Raw info zero represents -MAX_EQUIPMENT_ENCHANT, not an empty stack.
    for(int8_t enchant : {-MAX_EQUIPMENT_ENCHANT, 0}) {
        equipment_fixture();
        game.inventory[0] = make_equipment(SWORD, enchant);
        require(drop_inventory(0) && game.ground[0].item.type == SWORD &&
                equipment_enchant(game.ground[0].item) == enchant,
                "dropping a zero-code or zero-enchant sword silently discarded it");
        require(take_item(0) == PICKUP_TAKEN && game.inventory[0].type == SWORD,
                "zero-code or zero-enchant sword could not be picked up");
    }

    for(uint8_t type : {SWORD, ARMOR}) {
        for(int8_t enchant : {-5, 0, 5}) {
            equipment_fixture();
            Item item = make_equipment(type, enchant, 4);
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
    game.inventory[0] = make_equipment(ARMOR, -3, 4);
    game.inventory[0].info |= ITEM_CURSED;
    game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
    require(use_inventory(1, 0) && !item_is_cursed(game.inventory[0]) &&
            equipment_enchant(game.inventory[0]) == -3 && armor_rating(game.inventory[0]) == 4,
            "remove curse erased a negative enchantment");
}

static void check_generated_equipment()
{
    unsigned swords = 0, armors = 0;
    for(uint16_t seed = 1; seed <= 40; ++seed) {
        start_new(seed);
        for(uint8_t floor = 0; floor < FLOORS; ++floor) {
            game.floor = floor;
            make_floor();
            for(const GroundItem& ground : game.ground) {
                const Item& item = ground.item;
                if(item.type == SWORD) {
                    ++swords;
                    require(equipment_enchant(item) == floor / 4,
                            "generated sword retained additive value semantics");
                } else if(item.type == ARMOR) {
                    ++armors;
                    require(armor_rating(item) == 1 + floor / 4 &&
                            equipment_enchant(item) == 0,
                            "generated armor lost its inherent depth rating");
                }
            }
        }
    }
    require(swords && armors, "generation test did not encounter both equipment kinds");
}

void check_weapon_and_equipment_rules()
{
    check_weapon_math();
    check_equipment_encoding();
    check_equipment_actions();
    check_generated_equipment();
}
