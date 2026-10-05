#pragma once

#include "model.hpp"

#if defined(__AVM__)
namespace rogue { void update_generation_render(uint8_t percent); }
#endif

namespace rogue::generation {

enum Archetype : uint8_t { CHAMBERS, WARREN, FORTRESS, RUINS, ARCHETYPES };
enum Purpose : uint16_t {
    LAYOUT = 0x4a31, LOOPS = 0xb275, DOOR_SELECTION = 0x7d19,
    STAIRS = 0xe893, ENCOUNTERS = 0x19c7, SUPPLIES = 0x635b,
    EQUIPMENT = 0xd421
};

uint16_t floor_seed(Purpose purpose);
Archetype archetype(uint16_t layout_seed);
void generate_layout(uint16_t seed);
void add_secondary_connections(uint16_t seed);
void trim_dangling_passages();
void finalize_doors(uint16_t seed);
void choose_stairs(uint16_t seed);
void populate_monsters(uint16_t seed);
void populate_items(uint16_t seed, uint16_t equipment_seed);

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
