#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "world_gen_templates.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

using namespace rogue;
using namespace rogue::generation;
extern void require(bool, const char*);

namespace {

const char* names[] = {"CHAMBERS", "WARREN", "FORTRESS", "RUINS"};

void generate(unsigned seed, unsigned floor, bool ascent)
{
    game = {};
    game.run_seed = static_cast<uint16_t>(seed);
    game.floor = static_cast<uint8_t>(floor);
    game.has_amulet = ascent;
    game.random_state = 0x51ad;
    make_floor();
}

uint32_t terrain_hash()
{
    uint32_t hash = 2166136261u;
    for(uint8_t b : game.walls) hash = (hash ^ b) * 16777619u;
    return hash;
}

void print_map()
{
    std::printf("seed=0x%04x floor=%u %s %s coverage=%.1f%% rooms=%u corridors=%u loops=%u doors=%u attempts=%u hash=%08x\n",
        game.run_seed, game.floor, game.has_amulet ? "ascent" : "descent",
        names[archetype(floor_seed(LAYOUT))], diagnostics.floor_tiles * 100.0 / 2048,
        diagnostics.major_features, diagnostics.corridors, diagnostics.loops, game.door_count,
        diagnostics.attempts, terrain_hash());
    for(uint8_t y = 0; y < MAP_H; ++y) {
        for(uint8_t x = 0; x < MAP_W; ++x) {
            Position p = {x, y};
            char c = wall_at(x, y) ? '#' : '.';
            if(door_at(p) != NONE) c = '+';
            for(const GroundItem& item : game.ground)
                if(item.item.type && item.pos == p) c = '!';
            if(monster_at(p) != NONE) c = 'M';
            if(p == game.up) c = '<';
            if(p == game.down) c = '>';
            if(p == game.player) c = '@';
            std::putchar(c);
        }
        std::putchar('\n');
    }
    std::putchar('\n');
}

struct Metrics {
    unsigned floors = 0, tiles = 0, doors = 0, rooms = 0, loops = 0;
    unsigned dead = 0, junctions = 0, attempts = 0, redundant_edges = 0;
    unsigned minimum_tiles = 2048, maximum_tiles = 0, minimum_rooms = 255;
    unsigned minimum_separation = 99999, maximum_separation = 0;
    unsigned minimum_monster_distance = 99999, minimum_loops = 255;
    unsigned families[FAMILIES] = {};
    double separation = 0;
};

void validate(Metrics& m)
{
    require(game.random_state == 0x51ad, "generation consumed gameplay RNG");
    for(uint8_t b : game.explored) require(b == 0, "generation scratch survived");
    unsigned tiles = 0, edges = 0, dead = 0, junctions = 0;
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) {
            if(x < 2 || y < 2 || x >= MAP_W - 2 || y >= MAP_H - 2)
                require(wall_at(x, y), "outer wall buffer was carved");
            if(wall_at(x, y)) continue;
            ++tiles;
            edges += !wall_at(x + 1, y) + !wall_at(x, y + 1);
            uint8_t g = geometry({x, y});
            dead += (g & DEAD_END) != 0;
            junctions += (g & JUNCTION) && !(g & OPEN);
        }
    // The host oracle deliberately uses a queue, independently of the
    // production bitset propagation and attachment validity algorithms.
    std::array<uint16_t, 2048> queue = {};
    std::array<bool, 2048> seen = {};
    unsigned read = 0, write = 1;
    queue[0] = game.player.y * MAP_W + game.player.x;
    seen[queue[0]] = true;
    while(read < write) {
        unsigned index = queue[read++];
        int x = index & 63, y = index >> 6;
        const int nx[] = {x+1, x-1, x, x}, ny[] = {y, y, y+1, y-1};
        for(unsigned d = 0; d < 4; ++d) {
            if(wall_at(nx[d], ny[d])) continue;
            unsigned next = ny[d] * MAP_W + nx[d];
            if(!seen[next]) { seen[next] = true; queue[write++] = static_cast<uint16_t>(next); }
        }
    }
    require(write == tiles, "floor has inaccessible pockets");
    require(dead == 0, "floor has a corridor leading nowhere");
    require(tiles >= 300 && tiles <= 1050, "pathological floor coverage");
    require(diagnostics.major_features >= 5, "growth produced too few chambers");
    require(game.up != game.down && !wall_at(game.up.x, game.up.y) && !wall_at(game.down.x, game.down.y), "invalid stairs");
    require(door_at(game.up) == NONE && door_at(game.down) == NONE, "stairs overlap a door");
    require((geometry(game.up) & ROOMY) && (geometry(game.down) & ROOMY), "stairs are in a bottleneck");
    unsigned separation = distance_squared(game.up, game.down);
    require(separation >= 225, "stairs are too close");
    require(game.player == (game.has_amulet ? game.down : game.up), "wrong ascent/descent spawn");
    require(game.door_count <= DOORS, "door capacity exceeded");
    for(uint8_t i = 0; i < game.door_count; ++i) {
        Position p = door_position(i);
        require(p == game.doors[i].pos && !wall_at(p.x, p.y), "door has scratch flags or invalid position");
        require(geometry(p) & CORRIDOR, "door is not a narrow spatial boundary");
        for(uint8_t j = 0; j < i; ++j) require(p != door_position(j), "duplicate door");
    }
    for(uint8_t i = game.door_count; i < DOORS; ++i)
        require(game.doors[i].pos == Position{0, 0}, "unused door has scratch state");
    unsigned monsters = 0, lords = 0, nearest = 99999;
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Monster& mon = game.monsters[i];
        if(!mon.type) continue;
        ++monsters;
        require(!wall_at(mon.pos.x, mon.pos.y) && door_at(mon.pos) == NONE, "invalid monster placement");
        for(uint8_t j = 0; j < i; ++j)
            if(game.monsters[j].type) require(mon.pos != game.monsters[j].pos, "monsters overlap");
        require(mon.hp == monster_health(mon.type), "monster health distribution changed");
        if(mon.type == LORD) {
            ++lords;
            require(mon.pos == game.down && !game.has_amulet && game.floor == FLOORS - 1, "invalid final-floor Lord");
        } else {
            require(mon.pos != game.up && mon.pos != game.down, "ordinary monster on stairs");
            unsigned distance = distance_squared(game.player, mon.pos);
            require(distance >= (game.floor < 3 ? 36u : 16u), "unsafe initial monster spawn");
            nearest = std::min(nearest, distance);
        }
        if(mon.type == MIMIC)
            require((mon.state >> MIMIC_APPEARANCE_SHIFT) < MIMIC_APPEARANCE_COUNT &&
                (mon.state & ~MIMIC_APPEARANCE_MASK) == 0, "invalid mimic appearance");
    }
    require(monsters == MONSTERS, "population dropped an encounter");
    require(lords == unsigned(!game.has_amulet && game.floor == FLOORS - 1), "missing/extra Lord");
    unsigned items = 0;
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        const GroundItem& item = game.ground[i];
        if(!item.item.type) continue;
        ++items;
        require(!wall_at(item.pos.x, item.pos.y) && door_at(item.pos) == NONE &&
            item.pos != game.up && item.pos != game.down && monster_at(item.pos) == NONE, "invalid item placement");
        for(uint8_t j = 0; j < i; ++j)
            if(game.ground[j].item.type) require(item.pos != game.ground[j].pos, "generated items accidentally stack");
    }
    require(items == (game.has_amulet ? 0u : game.floor == FLOORS - 1 ? 15u : 16u), "wrong ascent/final-floor supply behavior");
    ++m.floors; m.tiles += tiles; m.doors += game.door_count;
    m.rooms += diagnostics.major_features; m.loops += diagnostics.loops;
    m.dead += dead; m.junctions += junctions; m.attempts += diagnostics.attempts;
    m.redundant_edges += edges - tiles + 1;
    m.minimum_tiles = std::min(m.minimum_tiles, tiles); m.maximum_tiles = std::max(m.maximum_tiles, tiles);
    m.minimum_rooms = std::min(m.minimum_rooms, unsigned(diagnostics.major_features));
    m.minimum_loops = std::min(m.minimum_loops, unsigned(diagnostics.loops));
    m.minimum_separation = std::min(m.minimum_separation, separation);
    m.maximum_separation = std::max(m.maximum_separation, separation);
    m.minimum_monster_distance = std::min(m.minimum_monster_distance, nearest);
    m.separation += std::sqrt(double(separation));
    for(unsigned i = 0; i < FAMILIES; ++i) m.families[i] += diagnostics.families[i];
}

void check_stream_isolation()
{
    for(unsigned seed : {0u, 1u, 0x1234u, 0xffffu}) {
        generate(seed, 7, false);
        Game original = game;
        uint16_t encounter = floor_seed(ENCOUNTERS), supplies = floor_seed(SUPPLIES), gear = floor_seed(EQUIPMENT);
        std::memset(game.walls, 0xff, sizeof game.walls);
        std::memset(game.doors, 0, sizeof game.doors);
        std::memset(game.monsters, 0, sizeof game.monsters);
        std::memset(game.ground, 0, sizeof game.ground);
        game.door_count = 0;
        generate_layout(floor_seed(LAYOUT) ^ 0x7231);
        finalize_doors(floor_seed(DOOR_SELECTION));
        choose_stairs(floor_seed(STAIRS));
        populate_monsters(encounter); populate_items(supplies, gear);
        require(std::memcmp(original.walls, game.walls, sizeof game.walls) != 0, "RNG isolation fixture has identical layouts");
        for(unsigned i = 0; i < MONSTERS; ++i)
            require(original.monsters[i].type == game.monsters[i].type &&
                original.monsters[i].state == game.monsters[i].state, "layout changed encounter random sequence");
        for(unsigned i = 0; i < GROUND_ITEMS; ++i)
            require(std::memcmp(&original.ground[i].item, &game.ground[i].item, sizeof(Item)) == 0,
                "layout changed supply/equipment random sequence");
    }
}

unsigned cycle_rank()
{
    unsigned cells = 0, edges = 0;
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x)
            if(!wall_at(x, y)) {
                ++cells;
                edges += !wall_at(x + 1, y) + !wall_at(x, y + 1);
            }
    return edges - cells + 1;
}

void check_loop_topology()
{
    for(unsigned seed = 0; seed < 64; ++seed) {
        game = {};
        game.run_seed = static_cast<uint16_t>(seed);
        game.floor = static_cast<uint8_t>(seed % FLOORS);
        std::memset(game.walls, 0xff, sizeof game.walls);
        diagnostics = {};
        generate_layout(floor_seed(LAYOUT));
        unsigned before = cycle_rank();
        add_secondary_connections(floor_seed(LOOPS));
        require(cycle_rank() == before + diagnostics.loops,
            "loop pass opened decorative holes instead of independent routes");
        trim_dangling_passages();
        require(cycle_rank() == before + diagnostics.loops,
            "corridor pruning removed a completed loop");
    }
}

void check_passage_trimming()
{
    game = {};
    std::memset(game.walls, 0xff, sizeof game.walls);
    for(uint8_t y = 8; y <= 12; ++y)
        for(uint8_t x = 8; x <= 32; ++x)
            if(x <= 12 || x >= 28 || y == 10) carve(x, y);
    // A terminal chamber and a small attached loop must both remain intact.
    for(uint8_t y = 10; y <= 11; ++y)
        for(uint8_t x = 5; x <= 6; ++x) carve(x, y);
    carve(7, 10);
    Game expected = game;
    for(uint8_t y = 5; y < 10; ++y) carve(18, y);
    for(uint8_t x = 15; x <= 21; ++x) carve(x, 5);
    for(uint8_t y = 13; y <= 22; ++y) carve(10, y);
    diagnostics.floor_tiles = 0;
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x)
            diagnostics.floor_tiles += !wall_at(x, y);
    trim_dangling_passages();
    require(!std::memcmp(&expected, &game, sizeof game),
        "branch pruning removed a chamber/loop/through passage or left a dangling branch");
    unsigned cells = 0;
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) cells += !wall_at(x, y);
    require(cells == diagnostics.floor_tiles, "trimmed coverage diagnostics are inaccurate");
    trim_dangling_passages();
    require(!std::memcmp(&expected, &game, sizeof game), "passage pruning is not idempotent");
}

void bulk(unsigned seeds)
{
    check_passage_trimming();
    require(check_feature_masks(), "carve/clearance transformation mismatch");
    require(seeds >= 1 && seeds <= 65536, "seed count must be 1..65536");
    Metrics metrics[ARCHETYPES];
    std::set<uint32_t> hashes;
    std::set<uint16_t> layout_seeds;
    for(unsigned seed = 0; seed < seeds; ++seed)
        for(unsigned floor = 0; floor < FLOORS; ++floor)
            for(unsigned ascent = 0; ascent < 2; ++ascent) {
                generate(seed, floor, ascent != 0);
                Archetype style = archetype(floor_seed(LAYOUT));
                validate(metrics[style]);
                hashes.insert(terrain_hash());
                layout_seeds.insert(floor_seed(LAYOUT));
                if(seed < 4 || (seed % 32 == 0 && floor % 4 == 0)) {
                    Game before = game;
                    std::memset(game.explored, 0xff, sizeof game.explored);
                    make_floor();
                    require(std::memcmp(&before, &game, sizeof game) == 0, "same floor identity regenerated differently");
                }
            }
    check_stream_isolation();
    check_loop_topology();
    unsigned total = seeds * FLOORS * 2;
    require(hashes.size() >= layout_seeds.size() * 99 / 100, "too many repeated terrain silhouettes");
    std::printf("%u floors, %u unique layout seeds, %u unique terrain hashes; all generation properties passed\n",
        total, unsigned(layout_seeds.size()), unsigned(hashes.size()));
    std::puts("style,count,coverage_mean,tiles_min,tiles_max,rooms_mean,rooms_min,doors_mean,loops_mean,loops_min,dead_ends_mean,junctions_mean,grid_cycle_rank_mean,stairs_mean,stairs_min,monster_distance_min,attempts_mean");
    for(unsigned a = 0; a < ARCHETYPES; ++a) {
        const Metrics& m = metrics[a];
        require(m.floors != 0, "missing archetype");
        std::printf("%s,%u,%.2f,%u,%u,%.2f,%u,%.2f,%.2f,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f\n",
            names[a], m.floors, m.tiles * 100.0 / (m.floors * 2048), m.minimum_tiles, m.maximum_tiles,
            m.rooms / double(m.floors), m.minimum_rooms, m.doors / double(m.floors), m.loops / double(m.floors),
            m.minimum_loops, m.dead / double(m.floors), m.junctions / double(m.floors),
            m.redundant_edges / double(m.floors), m.separation / m.floors, std::sqrt(double(m.minimum_separation)),
            std::sqrt(double(m.minimum_monster_distance)), m.attempts / double(m.floors));
        std::printf("%s family_counts:", names[a]);
        for(unsigned i = 0; i < FAMILIES; ++i) std::printf(" %u", m.families[i]);
        std::putchar('\n');
    }
}

unsigned number(const char* value)
{
    char* end = nullptr;
    unsigned long result = std::strtoul(value, &end, 0);
    require(end && *end == 0 && result <= 65536, "invalid numeric generator argument");
    return static_cast<unsigned>(result);
}

} // namespace

bool generation_command(int argc, char** argv)
{
    if(argc >= 2 && std::strcmp(argv[1], "--floor-corpus") == 0) {
        require(argc == 4, "usage: --floor-corpus SEED_COUNT OUTPUT");
        unsigned seeds = number(argv[2]);
        require(seeds >= 1 && seeds <= 65536, "seed count must be 1..65536");
        FILE* out = std::fopen(argv[3], "wb");
        require(out != nullptr, "could not create floor corpus");
        // Same field order as --floor-state, plus gameplay RNG. Compare this
        // corpus across builds to catch terrain, population and stream drift.
        for(unsigned seed = 0; seed < seeds; ++seed)
            for(unsigned floor = 0; floor < FLOORS; ++floor)
                for(unsigned ascent = 0; ascent < 2; ++ascent) {
                    generate(seed, floor, ascent != 0);
                    require(std::fwrite(game.walls, 1, 694, out) == 694 &&
                        std::fwrite(&game.player, 1, 6, out) == 6 &&
                        std::fwrite(&game.door_count, 1, 1, out) == 1 &&
                        std::fwrite(&game.random_state, 1, 2, out) == 2,
                        "could not write floor corpus");
                }
        require(std::fclose(out) == 0, "could not close floor corpus");
        return true;
    }
    if(argc >= 2 && std::strcmp(argv[1], "--floor-state") == 0) {
        require(argc == 6, "usage: --floor-state RUN_SEED FLOOR ascent|descent OUTPUT");
        unsigned seed = number(argv[2]), floor = number(argv[3]);
        require(seed <= 65535 && floor < FLOORS, "seed/floor out of range");
        require(std::strcmp(argv[4], "ascent") == 0 || std::strcmp(argv[4], "descent") == 0, "invalid ascent/descent state");
        generate(seed, floor, std::strcmp(argv[4], "ascent") == 0);
        FILE* out = std::fopen(argv[5], "wb");
        require(out != nullptr, "could not create generated state file");
        bool ok = std::fwrite(game.walls, 1, 694, out) == 694 &&
            std::fwrite(&game.player, 1, 6, out) == 6 &&
            std::fwrite(&game.door_count, 1, 1, out) == 1;
        require(std::fclose(out) == 0 && ok, "could not write generated state file");
        return true;
    }
    if(argc >= 2 && std::strcmp(argv[1], "--floor-map") == 0) {
        require(argc == 4 || argc == 5, "usage: --floor-map RUN_SEED FLOOR [ascent|descent]");
        unsigned seed = number(argv[2]), floor = number(argv[3]);
        require(seed <= 65535 && floor < FLOORS, "seed/floor out of range");
        require(argc != 5 || std::strcmp(argv[4], "ascent") == 0 || std::strcmp(argv[4], "descent") == 0, "invalid ascent/descent state");
        generate(seed, floor, argc == 5 && std::strcmp(argv[4], "ascent") == 0); print_map(); return true;
    }
    if(argc >= 2 && std::strcmp(argv[1], "--floor-batch") == 0) {
        require(argc == 3, "usage: --floor-batch COUNT");
        unsigned count = number(argv[2]);
        for(unsigned seed = 0; seed < count; ++seed) { generate(seed, seed % FLOORS, false); print_map(); }
        return true;
    }
    if(argc >= 2 && std::strcmp(argv[1], "--floor-bulk") == 0) {
        require(argc == 3, "usage: --floor-bulk SEED_COUNT");
        bulk(number(argv[2])); return true;
    }
    return false;
}
