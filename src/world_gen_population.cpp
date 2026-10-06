#include "world_gen.hpp"
#include "world.hpp"
#include "game.hpp"
#include "game_internal.hpp"

namespace rogue::generation {

// Weighted encounter lists from ArduRogue's MAP_GEN_INFOS. Zero entries are
// retried, as in the original generator.
static const uint8_t PROGMEM floor_monsters[FLOORS][12] = {
    {BAT, BAT, SNAKE, 0, 0, 0, BAT, BAT, SNAKE, 0, 0, 0},
    {SNAKE, SNAKE, SNAKE, SNAKE, RATTLESNAKE, RATTLESNAKE, SNAKE, SNAKE, SNAKE, SNAKE, RATTLESNAKE, RATTLESNAKE},
    {ZOMBIE, ZOMBIE, ZOMBIE, GOBLIN, GOBLIN, PHANTOM, ZOMBIE, ZOMBIE, ZOMBIE, GOBLIN, GOBLIN, PHANTOM},
    {ZOMBIE, GOBLIN, GOBLIN, PHANTOM, ORC, 0, ZOMBIE, GOBLIN, GOBLIN, PHANTOM, ORC, 0},
    {PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM},
    {GOBLIN, GOBLIN, GOBLIN, ORC, HOBGOBLIN, 0, GOBLIN, GOBLIN, GOBLIN, ORC, HOBGOBLIN, 0},
    {ORC, ORC, HOBGOBLIN, TARANTULA, MIMIC, 0, ORC, ORC, HOBGOBLIN, TARANTULA, MIMIC, 0},
    {ORC, HOBGOBLIN, TARANTULA, TARANTULA, TARANTULA, MIMIC, ORC, HOBGOBLIN, TARANTULA, TARANTULA, TARANTULA, MIMIC},
    {HOBGOBLIN, HOBGOBLIN, HOBGOBLIN, TARANTULA, MIMIC, INCUBUS, HOBGOBLIN, HOBGOBLIN, HOBGOBLIN, TARANTULA, MIMIC, INCUBUS},
    {MIMIC, MIMIC, MIMIC, MIMIC, TARANTULA, HOBGOBLIN, MIMIC, MIMIC, MIMIC, MIMIC, TARANTULA, HOBGOBLIN},
    {TARANTULA, HOBGOBLIN, MIMIC, INCUBUS, INCUBUS, TROLL, TARANTULA, HOBGOBLIN, MIMIC, INCUBUS, INCUBUS, TROLL},
    {HOBGOBLIN, MIMIC, INCUBUS, TROLL, TROLL, GRIFFIN, HOBGOBLIN, MIMIC, INCUBUS, TROLL, TROLL, GRIFFIN},
    {MIMIC, INCUBUS, TROLL, GRIFFIN, GRIFFIN, DRAGON, MIMIC, INCUBUS, TROLL, GRIFFIN, GRIFFIN, INCUBUS},
    {INCUBUS, TROLL, GRIFFIN, DRAGON, DRAGON, TROLL, INCUBUS, TROLL, GRIFFIN, TROLL, INCUBUS, GRIFFIN},
    {INCUBUS, ANGEL, ANGEL, DRAGON, DRAGON, DRAGON, INCUBUS, ANGEL, ANGEL, DRAGON, TROLL, GRIFFIN},
    {INCUBUS, INCUBUS, ANGEL, ANGEL, ANGEL, ANGEL, INCUBUS, INCUBUS, ANGEL, ANGEL, ANGEL, ANGEL}
};

static uint8_t floor_roll(uint16_t& seed, uint8_t limit)
{
    return static_cast<uint8_t>(next_random(seed) % limit);
}

static uint8_t floor_equipment_roll(uint16_t& seed, uint8_t limit)
{
    // Mix each output word before reducing it: consecutive xorshift outputs
    // otherwise correlate for rare subtype/enchantment/curse combinations.
    // A 32-bit output mixer breaks those bit relationships without changing
    // the underlying RNG or adding saved storage.
    uint32_t value = next_random(seed);
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<uint8_t>(value % limit);
}

static uint8_t floor_weapon_type(uint16_t& seed)
{
    // Keep the full weapon ladder, with the top tier rarer during the opening.
    uint8_t heavy = game.floor < 4 ? 2 : game.floor < 8 ? game.floor : game.floor < 10 ? 10 : 20;
    uint8_t chance = floor_equipment_roll(seed, 100);
    return chance < 25 ? DAGGER : chance < 45 ? SPEAR :
        chance < 75 ? LONG_SWORD : chance < 100 - heavy ? MACE : TWO_HANDED_SWORD;
}

static uint8_t floor_armor_type(uint16_t& seed)
{
    // Delay Plate while keeping every intrinsic tier possible at every depth.
    uint8_t plate = game.floor < 4 ? 1 : game.floor < 8 ? game.floor - 1 : game.floor < 10 ? 8 : 18;
    uint8_t chance = floor_equipment_roll(seed, 100);
    return chance < 25 ? LEATHER_ARMOR : chance < 45 ? RING_MAIL :
        chance < 65 ? SCALE_MAIL : chance < 80 ? CHAIN_MAIL :
        chance < 100 - plate ? SPLINT_MAIL : PLATE_MAIL;
}

static uint8_t floor_ring_type(uint16_t& seed)
{
    uint8_t chance = floor_equipment_roll(seed, 100);
    return
        chance < 10 ? RING_SEE_INVISIBLE :
        chance < 25 ? RING_STRENGTH :
        chance < 40 ? RING_DEXTERITY :
        chance < 55 ? RING_PROTECTION :
        chance < 68 ? RING_FIRE_IMMUNITY :
        chance < 83 ? RING_ATTACK :
        chance < 98 ? RING_SUSTENANCE :
        RING_INVISIBILITY;
}

static uint8_t floor_amulet_type(uint16_t& seed)
{
    uint8_t chance = floor_roll(seed, 16);
    if(chance == 8) return AMULET_VAMPIRE;
    return static_cast<uint8_t>(AMULET_FIRST + chance % AMULET_COUNT);
}

static uint8_t floor_potion_type(uint16_t& seed)
{
    uint8_t chance = floor_roll(seed, 20);
    if(game.floor > 0 && game.floor < 5 && (chance == POTION_COUNT + POTION_HARMING - POTION_FIRST || chance == POTION_COUNT + POTION_POISON - POTION_FIRST)) return POTION_HEALING;
    if(chance == POTION_COUNT + POTION_EXPERIENCE - POTION_FIRST)
        return game.floor == 0 || (game.floor >= 5 && game.floor < 10) ? POTION_SLOWING : POTION_HEALING;
    return static_cast<uint8_t>(POTION_FIRST + chance % POTION_COUNT);
}

constexpr uint8_t NORMAL_WAND_WEIGHT = 60;
constexpr uint8_t CURSED_WAND_WEIGHT = 10;
constexpr uint8_t UNRELIABLE_WAND_WEIGHT = 10;
constexpr uint8_t SPREADING_WAND_WEIGHT = 8;
constexpr uint8_t POWERFUL_WAND_WEIGHT = 10;
constexpr uint8_t OVERPOWERED_WAND_WEIGHT = 2;
static_assert(NORMAL_WAND_WEIGHT + CURSED_WAND_WEIGHT +
              UNRELIABLE_WAND_WEIGHT + SPREADING_WAND_WEIGHT +
              POWERFUL_WAND_WEIGHT + OVERPOWERED_WAND_WEIGHT == 100,
              "wand modifier weights must total 100");

static WandModifier floor_wand_modifier(uint16_t& seed)
{
    uint8_t chance = floor_roll(seed, 100);
    if(chance < NORMAL_WAND_WEIGHT) return WAND_NORMAL;
    chance -= NORMAL_WAND_WEIGHT;
    if(chance < CURSED_WAND_WEIGHT) return WAND_CURSED;
    chance -= CURSED_WAND_WEIGHT;
    if(chance < UNRELIABLE_WAND_WEIGHT) return WAND_UNRELIABLE;
    chance -= UNRELIABLE_WAND_WEIGHT;
    if(chance < SPREADING_WAND_WEIGHT) return WAND_SPREADING;
    chance -= SPREADING_WAND_WEIGHT;
    if(chance < POWERFUL_WAND_WEIGHT) return WAND_POWERFUL;
    return WAND_OVERPOWERED;
}


// A permutation visits each of the 2048 cells once per preference pass. Its
// two random words are taken from a slot-specific placement stream: failures
// and geometry preferences cannot consume the encounter/supply type stream.
static Position select_tile(uint16_t seed, uint8_t preference, bool monster)
{
    uint16_t start = next_random(seed) & 2047;
    uint16_t stride = next_random(seed) | 1;
    Position spawn = game.has_amulet ? game.down : game.up;
    uint16_t safe = game.floor < 3 ? 36 : 16;
    for(uint8_t pass = 0; pass < 3; ++pass) {
        uint16_t index = start;
        for(uint16_t n = 0; n < 2048; ++n, index = (index + stride) & 2047) {
            if(!(n & 63)) progress();
            Position pos = {static_cast<uint8_t>(index & 63), static_cast<uint8_t>(index >> 6)};
            if(wall_at(pos.x, pos.y) || pos == game.up || pos == game.down ||
               door_at(pos) != NONE || monster_at(pos) != NONE) continue;
            if(monster && distance_squared(pos, spawn) < safe) continue;
            bool occupied = false;
            if(!monster) {
                for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
                    if(game.ground[i].item.type && game.ground[i].pos == pos) occupied = true;
            } else if(pass == 0) {
                for(uint8_t i = 0; i < MONSTERS; ++i)
                    if(game.monsters[i].type && distance_squared(pos, game.monsters[i].pos) < 9) occupied = true;
            }
            if(occupied) continue;
            uint8_t wanted = pass == 0 ? preference : pass == 1 ? OPEN : 0;
            if(wanted && !(geometry(pos) & wanted)) continue;
            return pos;
        }
    }
    return {NONE, NONE};
}

__attribute__((noinline)) void populate_monsters(uint16_t seed)
{
    uint16_t placement_seed = seed;
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        uint8_t type = 0;
        if(!game.has_amulet && game.floor == FLOORS - 1 && i == MONSTERS - 1)
            type = LORD;
        else {
            // Encounter table retries retain the original depth distribution.
            // Every row has a nonzero entry; bound even this retry loop.
            for(uint8_t tries = 0; tries < 32 && !type; ++tries)
                type = floor_monsters[game.floor][floor_roll(seed, 12)];
            if(!type) type = floor_monsters[game.floor][0];
        }
        uint8_t appearance = type == MIMIC ? floor_roll(seed, MIMIC_APPEARANCE_COUNT) : 0;
        uint16_t placement = static_cast<uint16_t>(placement_seed ^ ((i + 1u) * 0x85ebu));
        Position pos = type == LORD ? game.down : select_tile(placement,
            i % 4 < 2 ? OPEN : i % 4 == 2 ? JUNCTION : 0, true);
        if(pos.x == NONE) continue;
        game.monsters[i] = {pos, type, monster_health(type), 0, {0, 0}, 0};
        if(type == MIMIC) set_mimic_appearance(game.monsters[i], static_cast<MimicAppearance>(appearance));
    }
}

__attribute__((noinline)) void populate_items(uint16_t seed, uint16_t equipment_seed)
{
    // Fresh ascent threats, but no replenishing ordinary supplies. Slot 15 on
    // the final descent remains reserved for the defeated Lord's Yendor drop.
    if(game.has_amulet) return;
    uint16_t placement_seed = seed;
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        uint8_t chance = floor_roll(seed, 72);
        uint8_t type = chance < 20 ? FOOD : chance < 40
            ? floor_potion_type(seed) :
              chance < 48 ? static_cast<uint8_t>(SCROLL_FIRST +
                                                floor_roll(seed, SCROLL_COUNT)) :
              chance < 56 ? floor_weapon_type(equipment_seed) :
              chance < 64 ? floor_armor_type(equipment_seed) :
              chance < 68 ? static_cast<uint8_t>(WAND_FIRST +
                                                 floor_roll(seed, WAND_COUNT)) :
              chance < 70
                ? floor_ring_type(seed)
                : floor_amulet_type(seed);
        uint8_t info = is_wand(type) ? static_cast<uint8_t>(3 + floor_roll(seed, 8)) : 1;
        if((is_ring(type) || is_amulet(type)) && floor_roll(seed, 8) == 0)
            info |= ITEM_CURSED;
        if(is_wand(type)) {
            Item wand = {type, info};
            set_wand_modifier(wand, floor_wand_modifier(seed));
            info = wand.info;
        }
        if(is_equipment(type)) {
            // Enchantment and curse are independent of depth and of each other.
            uint8_t chance = floor_equipment_roll(equipment_seed, 100);
            int8_t enchant = chance < 5 ? -2 : chance < 15 ? -1 :
                chance < 85 ? 0 : chance < 95 ? 1 : 2;
            Item equipment = make_equipment(type, enchant);
            if(floor_equipment_roll(equipment_seed, 8) == 0) equipment.info |= ITEM_CURSED;
            info = equipment.info;
        }
        if(game.floor == FLOORS - 1 && i == 15)
            continue;
        uint16_t placement = static_cast<uint16_t>(placement_seed ^ ((i + 1u) * 0x9e37u));
        uint8_t preference = (is_equipment(type) || is_wand(type)) && (i % 3 == 0)
            ? DEAD_END : OPEN;
        Position pos = select_tile(placement, preference, false);
        if(pos.x != NONE) game.ground[i] = {pos, {type, info}};
    }
}

} // namespace rogue::generation
