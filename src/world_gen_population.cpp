#include "world_gen.hpp"
#include "features.hpp"
#include "world.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "sim_hooks.hpp"

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
    uint8_t chance = floor_equipment_roll(seed, 100);
    return weapon_type_for_roll(chance, game.floor);
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
        chance < 94 ? RING_SUSTENANCE :
        RING_ILLUMINATION; // Invisibility now uses the unique artifact schedule.
}

static uint8_t floor_amulet_type(uint16_t& seed)
{
    uint8_t chance = floor_roll(seed, 17);
    if(chance == 16) return AMULET_BRILLIANCE;
    if(chance == 8) return AMULET_VAMPIRE;
    return static_cast<uint8_t>(AMULET_FIRST + chance % (AMULET_WISDOM - AMULET_FIRST + 1));
}

static uint8_t floor_light_type(uint16_t& seed)
{
    uint8_t chance = floor_roll(seed, 100);
    if(game.floor < 3) return chance < 30 ? CANDLE : chance < 90 ? TORCH : LANTERN;
    if(game.floor < 8) return chance < 15 ? CANDLE : chance < 60 ? TORCH : chance < 95 ? LANTERN : GLOWING_ORB;
    return chance < 5 ? CANDLE : chance < 30 ? TORCH : chance < 90 ? LANTERN : GLOWING_ORB;
}

static uint8_t floor_shield_type(uint16_t& seed)
{
    uint8_t chance = floor_equipment_roll(seed, 100);
    return chance < 45 ? BUCKLER : chance < 80 ? ROUND_SHIELD : TOWER_SHIELD;
}

static uint8_t floor_potion_type(uint16_t& seed)
{
    uint16_t sight_seed = static_cast<uint16_t>(seed ^ 0x53e9u);
    uint8_t sight_roll = floor_roll(sight_seed, 100);
    uint8_t chance = floor_roll(seed, 22);
    uint8_t sight_percent = BALANCE_VALUE("sight-percent", SEE_INVISIBLE_POTION_PERCENT);
    if(game.floor >= 2 && game.floor <= 4)
        sight_percent = BALANCE_VALUE("sight-early-percent", SEE_INVISIBLE_EARLY_PERCENT);
    if(sight_roll < sight_percent) return POTION_SEE_INVISIBLE;
    // A slot-local availability draw leaves ordinary generation streams intact.
    // Existing types retain their original relative selection and early redirects.
    uint16_t extra_seed = static_cast<uint16_t>(seed ^ 0x791bu);
    if(floor_roll(extra_seed, 100) < BALANCE_VALUE("new-consumables-percent", 25)) {
        static const uint8_t PROGMEM additions[] = {
            POTION_EXTRA_HEALING, POTION_EXTRA_HEALING, POTION_RESTORE_VITALITY,
            POTION_HASTE, POTION_LEVITATION, POTION_FIRE_RESISTANCE, POTION_FIRE
        };
        uint8_t type = additions[floor_roll(extra_seed, sizeof additions)];
        if(type == POTION_RESTORE_VITALITY && game.floor < 5) return POTION_EXTRA_HEALING;
        if(type == POTION_FIRE && game.floor > 0 && game.floor < 5) return POTION_HEALING;
        return type;
    }
    if(game.floor > 0 && game.floor < 5 && (chance == 11 + POTION_HARMING - POTION_FIRST || chance == 11 + POTION_POISON - POTION_FIRST)) return POTION_HEALING;
    if(chance == 11 + POTION_EXPERIENCE - POTION_FIRST)
        return game.floor == 0 || (game.floor >= 5 && game.floor < 10) ? POTION_SLOWING : POTION_HEALING;
    return static_cast<uint8_t>(POTION_FIRST + chance % 11);
}

static uint8_t floor_scroll_type(uint16_t& seed)
{
    uint8_t original = floor_roll(seed, 9);
    uint16_t extra_seed = static_cast<uint16_t>(seed ^ 0x39adu);
    if(floor_roll(extra_seed, 100) >= BALANCE_VALUE("new-consumables-percent", 25))
        return static_cast<uint8_t>(SCROLL_FIRST + original);
    static const uint8_t PROGMEM additions[] = {
        SCROLL_HOLD_MONSTER, SCROLL_HOLD_MONSTER, SCROLL_DETECT_TRAPS, SCROLL_DETECT_TRAPS,
        SCROLL_SUMMONING, SCROLL_MASS_TELEPORT, SCROLL_FIRE, SCROLL_DETECT_OBJECTS, SCROLL_DETECT_OBJECTS
    };
    return additions[floor_roll(extra_seed, sizeof additions)];
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
    // Passage cleanup removes every dead end before population. The
    // failed preference pass would only restart this same permutation.
    for(uint8_t pass = preference == DEAD_END ? 1 : 0; pass < 3; ++pass) {
        uint8_t wanted = pass == 0 ? preference : pass == 1 ? OPEN : 0;
        uint16_t index = start;
        for(uint16_t n = 0; n < 2048; ++n, index = (index + stride) & 2047) {
            if(!(n & 63)) progress();
            Position pos = {static_cast<uint8_t>(index & 63), static_cast<uint8_t>(index >> 6)};
            // Geometry is immutable here; reject rare preferences before
            // looking up occupants or checking monster spacing.
            if(wall_at(pos.x, pos.y) || (wanted && !(geometry(pos) & wanted))) continue;
            if(pos == game.up || pos == game.down ||
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
            return pos;
        }
    }
    return {NONE, NONE};
}

__attribute__((noinline)) void populate_monsters(uint16_t seed)
{
    uint16_t placement_seed = seed;
    uint8_t count = BALANCE_VALUE("monsters", MONSTERS);
    if(game.floor == 4) count = BALANCE_VALUE("phantom-count", count);
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t type = 0;
        if(!game.has_amulet && game.floor == FLOORS - 1 && i == count - 1)
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

uint8_t encounter_type(uint8_t floor, uint16_t& seed)
{
    for(uint8_t n = 0; n < 32; ++n) {
        uint8_t type = floor_monsters[floor][floor_roll(seed, 12)];
        if(type) return type;
    }
    return floor_monsters[floor][0];
}

Item useful_item(uint16_t& seed)
{
    switch(floor_roll(seed, 4)) {
    case 0: return {FOOD, 1};
    case 1: return {floor_potion_type(seed), 1};
    case 2: return make_equipment(floor_weapon_type(seed), 0);
    default: return {floor_scroll_type(seed), 1};
    }
}

uint8_t artifact_type(uint8_t index)
{
    static const uint8_t PROGMEM types[ARTIFACT_COUNT] = {
        STORMBRINGER, GLASS_SWORD, HAMMER_OF_RUIN, DRAGONHIDE, TITAN_PLATE,
        RING_INVISIBILITY, RING_REPRISAL, RING_HUNT, AMULET_PHOENIX_HEART, AMULET_HEART_OF_GIANT
    };
    return index < ARTIFACT_COUNT ? types[index] : static_cast<uint8_t>(NO_ITEM);
}

static uint16_t artifact_seed(uint16_t run_seed, uint8_t type)
{
    // Run-wide domain, independent of floor, ascent and all ordinary streams.
    uint8_t legacy = type;
    switch(type) {
    case STORMBRINGER: legacy = 20; break; case GLASS_SWORD: legacy = 21; break;
    case HAMMER_OF_RUIN: legacy = 22; break; case DRAGONHIDE: legacy = 29; break;
    case TITAN_PLATE: legacy = 30; break; case RING_INVISIBILITY: legacy = 39; break;
    case RING_REPRISAL: legacy = 40; break; case RING_HUNT: legacy = 41; break;
    case AMULET_PHOENIX_HEART: legacy = 50; break; case AMULET_HEART_OF_GIANT: legacy = 51; break;
    }
    uint32_t value = static_cast<uint32_t>(run_seed) + legacy * 0x9e3779b9u + ARTIFACTS;
    value = (value ^ (value >> 16)) * 0x7feb352du;
    value = (value ^ (value >> 15)) * 0x846ca68bu;
    return static_cast<uint16_t>(value ^ (value >> 16));
}

uint8_t artifact_floor(uint16_t run_seed, uint8_t type)
{
    if(!is_artifact(type)) return NONE;
    uint16_t seed = artifact_seed(run_seed, type);
    if(floor_roll(seed, ARTIFACT_SELECTION_DENOMINATOR)) return NONE;
    return static_cast<uint8_t>(ARTIFACT_FIRST_FLOOR +
        floor_roll(seed, ARTIFACT_LAST_FLOOR - ARTIFACT_FIRST_FLOOR + 1));
}

static Item artifact_item(uint8_t type)
{
    uint16_t seed = artifact_seed(game.run_seed, type);
    // Separate equipment outputs from the two scheduling draws.
    next_random(seed);
    next_random(seed);
    if(is_equipment(type)) {
        uint8_t chance = floor_equipment_roll(seed, 100);
        Item item = make_equipment(type, chance < 5 ? -2 : chance < 15 ? -1 :
            chance < 85 ? 0 : chance < 95 ? 1 : 2);
        if(!floor_equipment_roll(seed, ARTIFACT_CURSE_DENOMINATOR)) item.info |= ITEM_CURSED;
        return item;
    }
    return {type, static_cast<uint8_t>(1 |
        (!floor_equipment_roll(seed, ARTIFACT_CURSE_DENOMINATOR) ? ITEM_CURSED : 0))};
}

__attribute__((noinline)) static void place_artifacts()
{
    if(game.floor < ARTIFACT_FIRST_FLOOR || game.floor > ARTIFACT_LAST_FLOOR) return;
    uint16_t seed = floor_seed(ARTIFACTS);
    for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) {
        uint8_t type = artifact_type(a);
        if(artifact_floor(game.run_seed, type) != game.floor) continue;
        uint8_t start = floor_roll(seed, ground_item_capacity());
        uint8_t selected = NONE;
        for(uint8_t pass = 0; pass < 2 && selected == NONE; ++pass) {
            for(uint8_t n = 0; n < ground_item_capacity(); ++n) {
                uint8_t slot = static_cast<uint8_t>((start + n) % ground_item_capacity());
                uint8_t old = game.ground[slot].item.type;
                if(!old || old == YENDOR_AMULET || is_artifact(old) ||
                   (slot == yendor_ground_slot() && game.floor == FLOORS - 1)) continue;
                if(!pass && (old == FOOD || old == ARROWS)) continue;
                selected = slot;
                break;
            }
        }
        // Eligible production floors have accessible occupied ordinary slots,
        // and there are only ten artifacts. Previously placed types are skipped.
        if(selected != NONE) game.ground[selected].item = artifact_item(type);
    }
}

__attribute__((noinline)) void populate_ordinary_items(uint16_t seed, uint16_t equipment_seed)
{
    // Fresh ascent threats, but no replenishing ordinary supplies. The last slot on
    // the final descent remains reserved for the defeated Lord's Yendor drop.
    if(game.has_amulet) return;
    uint16_t placement_seed = seed;
    for(uint8_t i = 0; i < ground_item_capacity(); ++i) {
        uint8_t chance = floor_roll(seed, 87);
        uint8_t type = chance < FOOD_WEIGHT ? FOOD : chance < 20 ? ARROWS : chance < 40
            ? floor_potion_type(seed) :
              chance < 48 ? floor_scroll_type(seed) :
              chance < 56 ? floor_weapon_type(equipment_seed) :
              chance < 64 ? floor_armor_type(equipment_seed) :
              chance < 68 ? static_cast<uint8_t>(WAND_FIRST +
                                                 floor_roll(seed, WAND_COUNT)) :
              chance < 70
                ? floor_ring_type(seed)
                : chance < 72 ? floor_amulet_type(seed) :
                  chance < 82 ? floor_light_type(seed) : floor_shield_type(equipment_seed);
        uint8_t equipment = BALANCE_VALUE("early-equipment-percent", 0);
        if(equipment && game.floor < 4) {
            uint16_t bonus_seed = static_cast<uint16_t>(placement_seed ^ ((i + 1u) * 0x713bu));
            uint8_t bonus = floor_roll(bonus_seed, 100);
            if(is_armor(type) && bonus < equipment) type = CHAIN_MAIL;
            if(is_weapon(type) && !is_bow(type) && bonus < equipment) type = LONG_SWORD;
        }
        uint8_t info = is_wand(type) ? static_cast<uint8_t>(3 + floor_roll(seed, 8)) : 1;
        if(is_light(type)) info = light_initial_fuel(type);
        if(is_ammo(type)) {
            uint16_t quantity_seed = static_cast<uint16_t>(floor_seed(AMMO_QUANTITY) ^ ((i + 1u) * 0x9e37u));
            info = static_cast<uint8_t>(ARROW_BUNDLE_MIN + floor_roll(quantity_seed, ARROW_BUNDLE_MAX - ARROW_BUNDLE_MIN + 1));
        }
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
        if(game.floor == FLOORS - 1 && i == yendor_ground_slot())
            continue;
        uint16_t placement = static_cast<uint16_t>(placement_seed ^ ((i + 1u) * 0x9e37u));
        uint8_t preference = (is_equipment(type) || is_wand(type)) && (i % 3 == 0)
            ? DEAD_END : OPEN;
        Position pos = select_tile(placement, preference, false);
        if(pos.x != NONE) game.ground[i] = {pos, {type, info}};
    }
}

void populate_items(uint16_t seed, uint16_t equipment_seed)
{
    populate_ordinary_items(seed, equipment_seed);
    if(game.has_amulet) return;
#if defined(ARDUROGUE2_SIM)
    if(sim::artifacts_enabled()) place_artifacts();
#else
    place_artifacts();
#endif
    if(game.floor >= 1 && game.floor <= 3) {
        for(const GroundItem& ground : game.ground) if(is_light(ground.item.type)) return;
        for(GroundItem& ground : game.ground) {
            uint8_t type = ground.item.type;
            if(!type || is_artifact(type) || type == YENDOR_AMULET || type == FOOD || type == ARROWS) continue;
            ground.item = {TORCH, light_initial_fuel(TORCH)};
            return;
        }
        // A floor consisting entirely of essential supplies still needs a light.
        for(GroundItem& ground : game.ground) if(ground.item.type == FOOD || ground.item.type == ARROWS) {
            ground.item = {TORCH, light_initial_fuel(TORCH)}; return;
        }
    }
}

static uint8_t feature_geometry(uint8_t type)
{
    switch(type) {
    case FEATURE_FLAME_TRAP: return JUNCTION | CORRIDOR;
    case FEATURE_SNARE_TRAP: return CORRIDOR | CORNER;
    case FEATURE_TELEPORT_TRAP: return OPEN | JUNCTION;
    case FEATURE_MAGIC_DRAIN_TRAP: return DEAD_END | CORRIDOR;
    case FEATURE_SUMMONING_TRAP: return OPEN | JUNCTION;
    case FEATURE_HOLY_SYMBOL: return OPEN | ROOMY;
    case FEATURE_HEALING_SPRING: case FEATURE_SACRIFICIAL_ALTAR:
    case FEATURE_ENCHANTED_FORGE: case FEATURE_SARCOPHAGUS: return DEAD_END;
    case FEATURE_RUNESTONE: return NEAR_WALL | DEAD_END;
    default: return feature_is_trap(type) ? CORRIDOR : OPEN;
    }
}
// A local path is also a full-floor path. Failure to prove it is conservative.
// Closed doors are passable topology: movement opens them normally.
__attribute__((noinline)) bool teleport_tile_safe(Position pos)
{
    if(wall_at(pos.x, pos.y)) return false;
    uint32_t passable = 0;
    for(uint8_t i = 0; i < 25; ++i) {
        if(i == 12) continue;
        if(!wall_at(static_cast<int16_t>(pos.x) + i % 5 - 2,
                    static_cast<int16_t>(pos.y) + i / 5 - 2)) passable |= 1UL << i;
    }
    uint32_t neighbors = passable & ((1UL << 7) | (1UL << 11) | (1UL << 13) | (1UL << 17));
    if(!neighbors) return true;
    uint32_t reached = neighbors & (~neighbors + 1u);
    constexpr uint32_t left = (1UL << 0) | (1UL << 5) | (1UL << 10) | (1UL << 15) | (1UL << 20);
    constexpr uint32_t right = left << 4;
    for(uint8_t n = 0; n < 24; ++n) {
        uint32_t next = reached | (passable & ((reached << 5) | (reached >> 5) |
            ((reached & ~right) << 1) | ((reached & ~left) >> 1)));
        if(next == reached) break;
        reached = next;
    }
    return (reached & neighbors) == neighbors;
}
__attribute__((noinline)) static Position select_feature_tile(uint16_t& seed, uint8_t type)
{
    uint16_t start = next_random(seed) & 2047, stride = next_random(seed) | 1;
    uint8_t wanted = feature_geometry(type);
    bool safe_teleport = type == FEATURE_TELEPORT_TRAP;
#if defined(ARDUROGUE2_SIM)
    if(sim::original_features(sim::FeatureVariant::Placement)) {
        safe_teleport = false;
        if(type == FEATURE_TELEPORT_TRAP) wanted = JUNCTION;
    }
#endif
    // Spacing/geometry may relax. Connectivity and exclusions never do.
    for(uint8_t pass = 0; pass < 3; ++pass) {
        uint16_t cell = start;
        for(uint16_t n = 0; n < 2048; ++n, cell = (cell + stride) & 2047) {
            if(!(n & 63)) progress();
            Position pos{static_cast<uint8_t>(cell & 63), static_cast<uint8_t>(cell >> 6)};
            if(wall_at(pos.x, pos.y) || (pass < 2 && !(geometry(pos) & wanted))) continue;
            if(pos == game.player || pos == game.up || pos == game.down || door_at(pos) != NONE ||
               monster_at(pos) != NONE || item_at(pos) != NONE || feature_at(pos) != NONE) continue;
            uint8_t dx = pos.x > game.player.x ? pos.x - game.player.x : game.player.x - pos.x;
            uint8_t dy = pos.y > game.player.y ? pos.y - game.player.y : game.player.y - pos.y;
            if(feature_is_trap(type) && dx + dy <= 3) continue;
            bool close = false;
            if(pass == 0) for(const MapFeature& f : game.features)
                if(f.id && distance_squared(pos, f.pos) < 9) { close = true; break; }
            if(close || (safe_teleport && !teleport_tile_safe(pos))) continue;
            return pos;
        }
    }
    return {NONE, NONE};
}
__attribute__((noinline)) void populate_features(uint16_t seed)
{
#if defined(ARDUROGUE2_SIM)
    if(!sim::features_enabled()) return;
#endif
    uint8_t unholy = 0;
    for(const Monster& m : game.monsters) if(undead_or_demonic(m.type)) ++unholy;
    uint8_t depth = game.floor, roster_depth = game.floor;
#if defined(ARDUROGUE2_SIM)
    if(sim::original_features(sim::FeatureVariant::Early) || sim::original_features(sim::FeatureVariant::Density)) depth = 4;
    if(sim::original_features(sim::FeatureVariant::Early) || sim::original_features(sim::FeatureVariant::Roster)) roster_depth = 4;
#endif
    uint8_t count = depth < 2 ? OPENING_FEATURE_MIN + floor_roll(seed, 3) :
        depth < 4 ? MIDDLE_FEATURE_MIN + floor_roll(seed, 3) : DEEP_FEATURE_MIN + floor_roll(seed, 5);
    bool original_holy = false;
#if defined(ARDUROGUE2_SIM)
    original_holy = sim::original_features(sim::FeatureVariant::Holy);
#endif
    bool holy_selected = !original_holy && floor_roll(seed, 100) < holy_symbol_chance(unholy);
    bool holy_placed = false;
    for(uint8_t slot = 0; slot < count; ++slot) {
        uint8_t type;
        if(original_holy) {
            uint8_t weight = holy_placed ? 0 : holy_symbol_weight(unholy);
            uint8_t chance = floor_roll(seed, FEATURE_TRAP_WEIGHT + FEATURE_INTERACTIVE_WEIGHT + weight);
            type = chance < FEATURE_TRAP_WEIGHT ? 2 + 2 * floor_roll(seed, 10) :
                chance < FEATURE_TRAP_WEIGHT + FEATURE_INTERACTIVE_WEIGHT ? 22 + 2 * floor_roll(seed, 9) : FEATURE_HOLY_SYMBOL;
        } else if(!slot && holy_selected) type = FEATURE_HOLY_SYMBOL;
        else {
            bool trap = depth < 4 ? floor_roll(seed, 100) < (depth < 2 ? OPENING_TRAP_PERCENT : MIDDLE_TRAP_PERCENT) :
                floor_roll(seed, FEATURE_TRAP_WEIGHT + FEATURE_INTERACTIVE_WEIGHT) < FEATURE_TRAP_WEIGHT;
            if(!trap) type = 22 + 2 * floor_roll(seed, 9);
            else if(roster_depth < 2) {
                uint8_t choice = floor_roll(seed, 3);
                type = choice == 2 ? FEATURE_CONFUSION_TRAP : 2 + 2 * choice;
            } else type = 2 + 2 * floor_roll(seed, roster_depth < 4 ? 7 : 10);
        }
        // Historical holy selection can still be paired with tuned eligibility.
        if(original_holy && feature_is_trap(type) && roster_depth < 4) {
            if(roster_depth < 2) { uint8_t choice = floor_roll(seed, 3); type = choice == 2 ? FEATURE_CONFUSION_TRAP : 2 + 2 * choice; }
            else type = 2 + 2 * floor_roll(seed, 7);
        }
        Position chosen = select_feature_tile(seed, type);
        if(chosen.x == NONE) continue;
        game.features[slot] = {chosen, static_cast<uint8_t>(type | (feature_is_trap(type) ? MAP_FEATURE_HIDDEN : 0))};
        if(type == FEATURE_HOLY_SYMBOL) holy_placed = true;
    }
}

} // namespace rogue::generation
