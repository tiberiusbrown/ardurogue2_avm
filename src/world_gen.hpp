#pragma once

#include "model.hpp"

#if defined(__AVM__)
namespace rogue { void update_generation_render(uint8_t percent); }
#endif

namespace rogue::generation {

enum Archetype : uint8_t { CHAMBERS, WARREN, FORTRESS, RUINS, ARCHETYPES };
enum Purpose : uint16_t {
    LAYOUT = 0x4a31, LOOPS = 0xb275, DOOR_SELECTION = 0x7d19,
    STAIRS = 0xe893, ENCOUNTERS = 0x19c7, SUPPLIES = 0x635b, AMMO_QUANTITY = 0x4d71,
    EQUIPMENT = 0xd421, ARTIFACTS = 0xac57, MAP_FEATURE_DOMAIN = 0x37e9
};

constexpr uint8_t ARTIFACT_SELECTION_DENOMINATOR = 128;
constexpr uint8_t ARTIFACT_FIRST_FLOOR = 4, ARTIFACT_LAST_FLOOR = 14;
constexpr uint8_t ARTIFACT_CURSE_DENOMINATOR = 8;
uint8_t artifact_type(uint8_t index);
// NONE means this type was not selected for the run. No gameplay RNG consumed.
uint8_t artifact_floor(uint16_t run_seed, uint8_t type);

constexpr uint8_t FOOD_WEIGHT = 15, AMMO_WEIGHT = 5;
constexpr uint8_t ARROW_BUNDLE_MIN = 2, ARROW_BUNDLE_MAX = 3;
static_assert(FOOD_WEIGHT + AMMO_WEIGHT == 20, "supply boundaries must remain fixed");
constexpr uint8_t weapon_type_for_roll(uint8_t chance, uint8_t floor)
{
    uint8_t heavy = floor < 4 ? 2 : floor < 8 ? floor : floor < 10 ? 10 : 20;
    return chance < 25 ? DAGGER : chance < 39 ? SPEAR : chance < 45 ? SHORT_BOW :
        chance < 71 ? LONG_SWORD : chance < 75 ? LONG_BOW :
        chance < 100 - heavy ? MACE : TWO_HANDED_SWORD;
}

uint16_t floor_seed(Purpose purpose);
Archetype archetype(uint16_t layout_seed);
void generate_layout(uint16_t seed);
void add_secondary_connections(uint16_t seed);
void trim_dangling_passages();
void finalize_doors(uint16_t seed);
void choose_stairs(uint16_t seed);
void populate_monsters(uint16_t seed);
void populate_features(uint16_t seed);
uint8_t encounter_type(uint8_t floor, uint16_t& seed);
Item useful_item(uint16_t& seed);
constexpr uint8_t FEATURE_TRAP_WEIGHT = 55, FEATURE_INTERACTIVE_WEIGHT = 40;
constexpr uint8_t OPENING_FEATURE_MIN = 2, MIDDLE_FEATURE_MIN = 3, DEEP_FEATURE_MIN = 4;
constexpr uint8_t OPENING_TRAP_PERCENT = 30, MIDDLE_TRAP_PERCENT = 40;
constexpr uint8_t HOLY_BASE_PERCENT = 4, HOLY_COUNT_PERCENT = 2, HOLY_MAX_PERCENT = 30;
constexpr uint8_t holy_symbol_chance(uint8_t count) {
    return HOLY_BASE_PERCENT + HOLY_COUNT_PERCENT * count < HOLY_MAX_PERCENT
        ? HOLY_BASE_PERCENT + HOLY_COUNT_PERCENT * count : HOLY_MAX_PERCENT;
}
// Conservative 5x5 proof, including normally openable closed doors.
bool teleport_tile_safe(Position pos);
constexpr uint8_t HOLY_BASE_WEIGHT = 5, HOLY_SPAWN_BONUS = 5;
constexpr uint8_t holy_symbol_weight(uint8_t count) { return HOLY_BASE_WEIGHT + count * HOLY_SPAWN_BONUS; }
void populate_items(uint16_t seed, uint16_t equipment_seed);
void populate_ordinary_items(uint16_t seed, uint16_t equipment_seed);

// Yield visual progress during bounded chunks of generation work. The host
// generator has no frontend, timer, or dependency on AVM drawing APIs.
inline void progress(uint8_t percent = 255)
{
#if defined(__AVM__)
    update_generation_render(percent);
#else
    (void)percent;
#endif
}

// Computed only during generation. No classifications survive in Game.
enum Geometry : uint8_t {
    DEAD_END = 1, CORRIDOR = 2, CORNER = 4, JUNCTION = 8,
    OPEN = 16, ROOMY = 32, NEAR_WALL = 64
};
uint8_t geometry(Position pos);
uint16_t distance_squared(Position a, Position b);

#if !defined(__AVM__)
// Host-only observations; neither state nor instrumentation enters the image.
struct Diagnostics {
    uint16_t attempts, floor_tiles;
    uint8_t major_features, corridors, loops, open_connections;
    uint8_t families[16];
};
extern Diagnostics diagnostics;
bool check_feature_masks();
#endif

} // namespace rogue::generation
