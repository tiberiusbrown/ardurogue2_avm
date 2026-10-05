#include "world_gen.hpp"
#include "sim_hooks.hpp"
#include "world_gen_templates.hpp"
#include "world.hpp"
#include "game_internal.hpp"
#include <string.h>
#if defined(__AVM__)
#include "render.hpp"
#endif

namespace rogue::generation {

#if !defined(__AVM__)
Diagnostics diagnostics = {};
#endif

static uint16_t mix(uint16_t value)
{
    value ^= value >> 7;
    value = static_cast<uint16_t>(value * 0x9e37u);
    value ^= value >> 9;
    value = static_cast<uint16_t>(value * 0x85ebu);
    return static_cast<uint16_t>(value ^ (value >> 8));
}

uint16_t floor_seed(Purpose purpose)
{
    return mix(static_cast<uint16_t>(mix(game.run_seed) ^
        mix(static_cast<uint16_t>((game.floor + 1u) * 0x6d2bu)) ^
        (game.has_amulet ? 0xa5c3u : 0u) ^ purpose));
}

Archetype archetype(uint16_t seed)
{
    return static_cast<Archetype>(mix(seed ^ 0x37afu) & 3);
}

static uint8_t random(uint16_t& seed, uint8_t limit)
{
    return static_cast<uint8_t>(next_random(seed) % limit);
}

static Position tile(uint16_t index)
{
    return {static_cast<uint8_t>(index & 63), static_cast<uint8_t>(index >> 6)};
}

static int8_t dx(uint8_t dir) { return dir == 0 ? 1 : dir == 2 ? -1 : 0; }
static int8_t dy(uint8_t dir) { return dir == 1 ? 1 : dir == 3 ? -1 : 0; }

static uint8_t neighbors(Position pos)
{
    return static_cast<uint8_t>(!wall_at(pos.x + 1, pos.y) +
        !wall_at(pos.x - 1, pos.y) + !wall_at(pos.x, pos.y + 1) +
        !wall_at(pos.x, pos.y - 1));
}

static constexpr uint8_t PROGMEM bits3[8] = {0,1,1,2,1,2,2,3};

// Three neighboring wall bits, using at most two byte loads. Generation
// positions are interior tiles; the boundary guard lives in the caller.
static uint8_t local_row(Position pos, int8_t offset)
{
    uint8_t x = static_cast<uint8_t>(pos.x - 1);
    uint16_t index = static_cast<uint16_t>((pos.y + offset) * 8 + (x >> 3));
    uint16_t walls = game.walls[index];
    if((x & 7) > 5) walls |= static_cast<uint16_t>(game.walls[index + 1]) << 8;
    return static_cast<uint8_t>((~(walls >> (x & 7))) & 7);
}

static bool roomy(Position pos)
{
    return pos.x > 0 && pos.x < MAP_W - 1 && pos.y > 0 && pos.y < MAP_H - 1 &&
        local_row(pos, -1) == 7 && local_row(pos, 0) == 7 && local_row(pos, 1) == 7;
}

uint8_t geometry(Position pos)
{
    if(pos.x == 0 || pos.y == 0 || pos.x >= MAP_W - 1 || pos.y >= MAP_H - 1) return 0;
    uint8_t above = local_row(pos, -1), center = local_row(pos, 0), below = local_row(pos, 1);
    uint8_t count = static_cast<uint8_t>(bits3[center & 5] + ((above & 2) != 0) + ((below & 2) != 0));
    uint8_t area = static_cast<uint8_t>(bits3[above] + bits3[center] + bits3[below]);
    uint8_t result = count == 1 ? DEAD_END : count >= 3 && area <= 5 ? JUNCTION : 0;
    if(count == 2) result |= (center & 5) == 5 || ((above & below) & 2) ? CORRIDOR : CORNER;
    if(area >= 6) result |= OPEN;
    if(area == 9) result |= ROOMY;
    if(area != 9) result |= NEAR_WALL;
    return result;
}

uint16_t distance_squared(Position a, Position b)
{
    int16_t x = static_cast<int16_t>(a.x) - b.x;
    int16_t y = static_cast<int16_t>(a.y) - b.y;
    return static_cast<uint16_t>(x * x + y * y);
}

// One six-byte descriptor, only for the feature currently being attempted.
struct Feature { uint8_t family, w, h, transform; int8_t x, y; };

static uint16_t base_row(const Feature& f, int8_t y)
{
    if(y < 0 || y >= f.h) return 0;
    if(f.family >= L_CHAMBER) return templates[f.family - L_CHAMBER].carve[y];
    if(f.family == BENT_PASSAGE && y != f.h - 1) return 1;
    return static_cast<uint16_t>((1u << f.w) - 1);
}

static bool base_floor(const Feature& f, int8_t x, int8_t y)
{
    if(x < 0 || y < 0 || x >= f.w || y >= f.h) return false;
    if(f.family >= L_CHAMBER)
        return (templates[f.family - L_CHAMBER].carve[y] & (1u << x)) != 0;
    if(f.family == BENT_PASSAGE) return x == 0 || y == f.h - 1;
    return true;
}

static bool mask(const Feature& f, int8_t x, int8_t y, bool clearance, bool relaxed)
{
    int8_t bx = x, by = y;
    switch(f.transform & 3) {
    case 1: bx = y; by = static_cast<int8_t>(f.h - 1 - x); break;
    case 2: bx = static_cast<int8_t>(f.w - 1 - x); by = static_cast<int8_t>(f.h - 1 - y); break;
    case 3: bx = static_cast<int8_t>(f.w - 1 - y); by = x; break;
    }
    if(f.transform & 4) bx = static_cast<int8_t>(f.w - 1 - bx);
    if(!clearance) return base_floor(f, bx, by);
    if(bx < -1 || by < -1 || bx > f.w || by > f.h) return false;
    if(f.family >= L_CHAMBER && !relaxed)
        return (templates[f.family - L_CHAMBER].clearance[by + 1] & (1u << (bx + 1))) != 0;
    if(f.family < BENT_PASSAGE) {
        // Rectangles and straight passages have rectangular clearance. Ruins
        // omit just the four diagonal buffer corners for controlled contact.
        return !relaxed || !((bx < 0 || bx >= f.w) && (by < 0 || by >= f.h));
    }
    if(f.family == BENT_PASSAGE) {
        if(!relaxed) return bx <= 1 || by >= f.h - 2;
        return bx == 0 || (bx <= 1 && by >= 0 && by < f.h) ||
            by == f.h - 1 || (by >= f.h - 2 && bx >= 0 && bx < f.w);
    }
    uint16_t row = base_row(f, by);
    uint16_t clearance_row = static_cast<uint16_t>(row | (row << 1) | (row << 2) |
        (base_row(f, by - 1) << 1) | (base_row(f, by + 1) << 1));
    return (clearance_row & (1u << (bx + 1))) != 0;
}

#if !defined(__AVM__)
bool check_feature_masks()
{
    // Verify the actual inverse-transform reader against an independent
    // forward-transform oracle, including negative clearance coordinates.
    for(uint8_t family = SMALL; family < FAMILIES; ++family) {
        Feature f = {family, 7, 6, 0, 0, 0};
        if(family >= L_CHAMBER) {
            f.w = templates[family - L_CHAMBER].w;
            f.h = templates[family - L_CHAMBER].h;
        } else if(family == SHORT_PASSAGE || family == LONG_PASSAGE) f.w = 1;
        for(uint8_t transform = 0; transform < 8; ++transform) {
            f.transform = transform;
            for(int8_t by = -1; by <= f.h; ++by)
                for(int8_t bx = -1; bx <= f.w; ++bx) {
                    int8_t rx = transform & 4 ? static_cast<int8_t>(f.w - 1 - bx) : bx;
                    int8_t tx = rx, ty = by;
                    switch(transform & 3) {
                    case 1: tx = static_cast<int8_t>(f.h - 1 - by); ty = rx; break;
                    case 2: tx = static_cast<int8_t>(f.w - 1 - rx); ty = static_cast<int8_t>(f.h - 1 - by); break;
                    case 3: tx = by; ty = static_cast<int8_t>(f.w - 1 - rx); break;
                    }
                    if(mask(f, tx, ty, false, false) != base_floor(f, bx, by)) return false;
                    for(uint8_t relaxed = 0; relaxed < 2; ++relaxed) {
                        bool expected = false;
                        for(int8_t y = -1; y <= 1; ++y)
                            for(int8_t x = -1; x <= 1; ++x)
                                if((!relaxed || !x || !y) && base_floor(f, bx + x, by + y)) expected = true;
                        if(mask(f, tx, ty, true, relaxed != 0) != expected) return false;
                    }
                }
        }
    }
    return true;
}
#endif

static uint8_t width(const Feature& f) { return f.transform & 1 ? f.h : f.w; }
static uint8_t height(const Feature& f) { return f.transform & 1 ? f.w : f.h; }

static __attribute__((noinline)) void pick_feature(Feature& f, uint16_t& seed, Archetype style)
{
    uint8_t total = 0;
    for(uint8_t i = 0; i < FAMILIES; ++i) total += styles[style].weights[i];
    uint8_t choice = random(seed, total);
    f.family = 0;
    while(f.family + 1 < FAMILIES && choice >= styles[style].weights[f.family])
        choice -= styles[style].weights[f.family++];
    f.transform = random(seed, 8);
    if(f.family >= L_CHAMBER) {
        f.w = templates[f.family - L_CHAMBER].w;
        f.h = templates[f.family - L_CHAMBER].h;
        for(uint8_t tries = 0; tries < 8 && !(templates[f.family - L_CHAMBER].transforms & (1u << f.transform)); ++tries)
            f.transform = static_cast<uint8_t>((f.transform + 1) & 7);
    } else if(f.family <= LARGE) {
        uint8_t minimum = f.family == SMALL ? 4 : f.family == MEDIUM ? 6 : 8;
        uint8_t range = f.family == SMALL ? 4 : f.family == MEDIUM ? 5 : 7;
        f.w = static_cast<uint8_t>(minimum + random(seed, range));
        f.h = static_cast<uint8_t>(minimum + random(seed, range));
    } else {
        f.w = f.family == BENT_PASSAGE ? static_cast<uint8_t>(3 + random(seed, 6)) : 1;
        f.h = static_cast<uint8_t>((f.family == LONG_PASSAGE ? 8 : 3) +
            random(seed, f.family == LONG_PASSAGE ? 8 : 5));
    }
}

// Socket masks are derived a row at a time in base coordinates, then only
// the selected socket is transformed. They describe the same floor-backed
// boundary as the per-tile reader, without repeatedly transforming a bitmap.
static __attribute__((noinline)) bool socket(Feature& f, Position at, uint8_t dir, uint16_t& seed)
{
    uint8_t base_dir = static_cast<uint8_t>((dir + 4 - (f.transform & 3)) & 3);
    if(f.transform & 4) base_dir = static_cast<uint8_t>((2 - base_dir) & 3);
    uint8_t found = 0;
    int8_t sx = 0, sy = 0;
    for(int8_t y = 0; y < f.h; ++y) {
        if(!(y & 3)) progress();
        uint16_t row = base_row(f, y), sockets;
        if(base_dir == 0) sockets = static_cast<uint16_t>(row & (row >> 1) & ~(row << 1) & ~(row << 2));
        else if(base_dir == 2) sockets = static_cast<uint16_t>(row & (row << 1) & ~(row >> 1) & ~(row >> 2));
        else {
            int8_t forward = base_dir == 1 ? 1 : -1;
            sockets = static_cast<uint16_t>(row & base_row(f, y + forward) &
                ~base_row(f, y - forward) & ~base_row(f, y - 2 * forward));
        }
        for(int8_t x = 0; x < f.w && sockets; ++x, sockets >>= 1)
            if(sockets & 1)
                if(random(seed, ++found) == 0) { sx = x; sy = y; }
    }
    if(f.transform & 4) sx = static_cast<int8_t>(f.w - 1 - sx);
    int8_t tx = sx, ty = sy;
    switch(f.transform & 3) {
    case 1: tx = static_cast<int8_t>(f.h - 1 - sy); ty = sx; break;
    case 2: tx = static_cast<int8_t>(f.w - 1 - sx); ty = static_cast<int8_t>(f.h - 1 - sy); break;
    case 3: tx = sy; ty = static_cast<int8_t>(f.w - 1 - sx); break;
    }
    f.x = static_cast<int8_t>(at.x + dx(dir) - tx);
    f.y = static_cast<int8_t>(at.y + dy(dir) - ty);
    return found != 0;
}

static bool solid_run(uint8_t x, uint8_t y, uint8_t length)
{
    uint16_t index = static_cast<uint16_t>(y * 8 + (x >> 3));
    uint8_t shift = x & 7;
    while(length) {
        uint8_t count = static_cast<uint8_t>(8 - shift);
        if(count > length) count = length;
        uint8_t bits = static_cast<uint8_t>(((1u << count) - 1) << shift);
        if((game.walls[index++] & bits) != bits) return false;
        length -= count;
        shift = 0;
    }
    return true;
}

static __attribute__((noinline)) bool valid(const Feature& f, bool relaxed)
{
    if(f.x < 2 || f.y < 2 || f.x + width(f) > MAP_W - 2 ||
       f.y + height(f) > MAP_H - 2) return false;
    if(f.family < BENT_PASSAGE) {
        for(int8_t y = -1; y <= height(f); ++y) {
            bool corners = relaxed && (y == -1 || y == height(f));
            if(!solid_run(static_cast<uint8_t>(f.x - 1 + corners), static_cast<uint8_t>(f.y + y),
                          static_cast<uint8_t>(width(f) + 2 - 2 * corners))) return false;
        }
        return true;
    }
    for(int8_t y = -1; y <= height(f); ++y) {
        if(!(y & 3)) progress();
        for(int8_t x = -1; x <= width(f); ++x)
            if(mask(f, x, y, true, relaxed) && !wall_at(f.x + x, f.y + y)) return false;
    }
    return true;
}

static uint16_t stamp(const Feature& f)
{
    uint16_t count = 0;
    for(int8_t y = 0; y < height(f); ++y) {
        if(!(y & 3)) progress();
        for(int8_t x = 0; x < width(f); ++x)
            if(mask(f, x, y, false, false)) {
                carve(static_cast<uint8_t>(f.x + x), static_cast<uint8_t>(f.y + y));
                ++count;
            }
    }
#if !defined(__AVM__)
    ++diagnostics.families[f.family];
    if(f.family >= SHORT_PASSAGE && f.family <= BENT_PASSAGE) ++diagnostics.corridors;
    else ++diagnostics.major_features;
#endif
    return count;
}

static __attribute__((noinline)) bool attachment(Position& at, uint8_t& dir, uint16_t& seed)
{
    uint16_t index = next_random(seed) & 2047;
    uint16_t stride = next_random(seed) | 1;
    (void)random(seed, 4); // Preserve the layout stream's per-search advance.
    for(uint16_t n = 0; n < 2048; ++n, index = (index + stride) & 2047) {
        if(!(n & 63)) progress();
        at = tile(index);
        if(at.x <= 1 || at.y <= 1 || at.x >= MAP_W - 2 || at.y >= MAP_H - 2) continue;
        uint8_t row = local_row(at, 0);
        if(row & 2) continue;
        uint8_t cardinal = static_cast<uint8_t>((row & 5) |
            ((local_row(at, -1) & 2) ? 8 : 0) | ((local_row(at, 1) & 2) ? 16 : 0));
        // A frontier wall has exactly one floor neighbor. Its unique inward
        // side determines orientation; packed reads replace four full checks.
        if(!cardinal || (cardinal & (cardinal - 1))) continue;
        dir = cardinal == 1 ? 0 : cardinal == 4 ? 2 : cardinal == 8 ? 1 : 3;
        return true;
    }
    return false;
}

// During growth only, the unused high three y bits hold candidate priority.
// This bounded reservoir retains the strongest transitions in the existing
// eleven door slots, without room records or an additional scratch array.
static __attribute__((noinline)) void door_candidate(Position at, uint8_t quality, uint16_t seed)
{
    uint16_t rank = mix(static_cast<uint16_t>(seed ^ (at.y * MAP_W + at.x)));
    Archetype style = archetype(floor_seed(LAYOUT));
    if(rank % 100 >= styles[style].door_chance) return;
    uint8_t score = static_cast<uint8_t>(quality * 2 + ((rank >> 8) & 1));
    uint8_t slot = game.door_count;
    if(slot == styles[style].door_limit) {
        slot = 0;
        for(uint8_t i = 1; i < game.door_count; ++i)
            if((game.doors[i].pos.y >> 5) < (game.doors[slot].pos.y >> 5)) slot = i;
        if(score <= (game.doors[slot].pos.y >> 5)) return;
    } else ++game.door_count;
    game.doors[slot].pos = {at.x, static_cast<uint8_t>(at.y | (score << 5))};
}

__attribute__((noinline)) void generate_layout(uint16_t seed)
{
    Archetype style = archetype(seed);
    uint16_t target = static_cast<uint16_t>(styles[style].coverage + random(seed, 121) - 60);
    Feature f = {MEDIUM, static_cast<uint8_t>(7 + random(seed, 4)),
        static_cast<uint8_t>(6 + random(seed, 4)), 0, 0, 0};
    f.x = static_cast<int8_t>(19 + random(seed, 26) - f.w / 2);
    f.y = static_cast<int8_t>(10 + random(seed, 12) - f.h / 2);
    uint16_t coverage = stamp(f);
    uint16_t door_seed = floor_seed(DOOR_SELECTION);
    // Late attempts favor small attachable rooms instead of returning a tiny
    // floor after a run of unlucky large-room placements. No retries recurse.
    for(uint16_t attempt = 0; attempt < 1800 && coverage < target; ++attempt) {
        progress(static_cast<uint8_t>(coverage * 55u / target));
#if !defined(__AVM__)
        diagnostics.attempts = static_cast<uint16_t>(attempt + 1);
#endif
        Position at;
        uint8_t dir;
        if(!attachment(at, dir, seed)) break;
        pick_feature(f, seed, style);
        if(attempt > 1100 && (attempt & 1)) {
            f.family = SMALL; f.w = static_cast<uint8_t>(4 + random(seed, 2));
            f.h = static_cast<uint8_t>(4 + random(seed, 2));
        }
        if(!socket(f, at, dir, seed) || !valid(f, style == RUINS)) continue;
        Position behind = {static_cast<uint8_t>(at.x - dx(dir)), static_cast<uint8_t>(at.y - dy(dir))};
        bool old_room = (geometry(behind) & OPEN) != 0;
        bool new_room = f.family < SHORT_PASSAGE || f.family > BENT_PASSAGE;
        // Passage side branches are uncommon: usually extend an endpoint or
        // leave a chamber. This prevents late rejected rooms from turning
        // every remaining strip of rock into a forest of dangling corridors.
        if(!new_room && !old_room && neighbors(behind) > 1 && random(seed, style == WARREN ? 2 : 5)) continue;
        coverage += stamp(f);
        carve(at.x, at.y);
        ++coverage;
        bool wide = false;
        if(style == RUINS && old_room && new_room && random(seed, 3) == 0) {
            // Deliberate three-wide merge, only where both feature faces exist.
            if(!wall_at(at.x + dx(dir) + dy(dir), at.y + dy(dir) - dx(dir)) &&
               !wall_at(at.x - dx(dir) + dy(dir), at.y - dy(dir) - dx(dir)) &&
               !wall_at(at.x + dx(dir) - dy(dir), at.y + dy(dir) + dx(dir)) &&
               !wall_at(at.x - dx(dir) - dy(dir), at.y - dy(dir) + dx(dir))) {
                carve(static_cast<uint8_t>(at.x + dy(dir)), static_cast<uint8_t>(at.y - dx(dir)));
                carve(static_cast<uint8_t>(at.x - dy(dir)), static_cast<uint8_t>(at.y + dx(dir)));
                coverage += 2;
                wide = true;
#if !defined(__AVM__)
                ++diagnostics.open_connections;
#endif
            }
        }
        if(!wide && (old_room || new_room))
            door_candidate(at, old_room != new_room ? 3 : 2, door_seed);
    }
#if !defined(__AVM__)
    diagnostics.floor_tiles = coverage;
#endif
}

static uint32_t scratch_row(uint8_t plane, uint8_t y)
{
    uint32_t value;
    memcpy(&value, game.explored + plane * 92 + y * 4, 4);
    return value;
}

static void scratch_row(uint8_t plane, uint8_t y, uint32_t value)
{
    memcpy(game.explored + plane * 92 + y * 4, &value, 4);
}

static __attribute__((noinline)) uint32_t floor_row(uint8_t x, uint8_t y)
{
    const uint8_t* row = game.walls + y * 8 + (x >> 3);
    uint32_t bits = row[0] | (static_cast<uint32_t>(row[1]) << 8) |
        (static_cast<uint32_t>(row[2]) << 16);
    if((x & 7) > 1) bits |= static_cast<uint32_t>(row[3]) << 24;
    return (~(bits >> (x & 7))) & 0x7fffffu;
}

// Exact synchronous cardinal expansion for routes up to the style's detour
// threshold. A 23x23 window contains every such short route between these
// close endpoints. Two 92-byte bit planes live in explored, never on stack.
static __attribute__((noinline)) bool short_route(Position a, Position b, Position center, uint8_t limit)
{
    uint8_t x = center.x < 11 ? 0 : center.x > 52 ? 41 : center.x - 11;
    uint8_t y = center.y < 11 ? 0 : center.y > 20 ? 9 : center.y - 11;
    memset(game.explored, 0, sizeof game.explored);
    scratch_row(0, static_cast<uint8_t>(a.y - y), 1ul << (a.x - x));
    for(uint8_t step = 0; step < limit; ++step) {
        progress();
        for(uint8_t row = 0; row < 23; ++row) {
            if(!(row & 3)) progress();
            uint32_t reached = scratch_row(0, row);
            uint32_t next = reached | (reached << 1) | (reached >> 1);
            if(row) next |= scratch_row(0, row - 1);
            if(row != 22) next |= scratch_row(0, row + 1);
            scratch_row(1, row, next & floor_row(x, static_cast<uint8_t>(y + row)));
        }
        memcpy(game.explored, game.explored + 92, 92);
        if(scratch_row(0, static_cast<uint8_t>(b.y - y)) & (1ul << (b.x - x))) return true;
    }
    return false;
}

__attribute__((noinline)) void add_secondary_connections(uint16_t seed)
{
    Archetype style = archetype(floor_seed(LAYOUT));
    uint8_t goal = static_cast<uint8_t>(styles[style].loop_min + random(seed, styles[style].loop_range));
    uint8_t made = 0;
    uint16_t index = next_random(seed) & 2047;
    uint16_t stride = next_random(seed) | 1;
    // A complete permutation considers every tile once. Each opportunity has
    // two orientations and a maximum connector length of three tiles.
    for(uint16_t n = 0; n < 2048 && made < goal; ++n, index = (index + stride) & 2047) {
        if(!(n & 31)) progress();
        Position at = tile(index);
        if(at.x < 2 || at.y < 2 || at.x > MAP_W - 3 || at.y > MAP_H - 3 || !wall_at(at.x, at.y)) continue;
        for(uint8_t dir = 0; dir < 4 && made < goal; ++dir) {
            Position a = {static_cast<uint8_t>(at.x - dx(dir)), static_cast<uint8_t>(at.y - dy(dir))};
            if(wall_at(a.x, a.y)) continue;
            uint8_t length = 0;
            int16_t bx = at.x, by = at.y;
            while(length < 3 && wall_at(bx, by) && bx > 1 && by > 1 && bx < MAP_W - 2 && by < MAP_H - 2 &&
                  wall_at(bx + dy(dir), by - dx(dir)) && wall_at(bx - dy(dir), by + dx(dir))) {
                ++length; bx += dx(dir); by += dy(dir);
            }
            if(!length || wall_at(bx, by)) continue;
            Position b = {static_cast<uint8_t>(bx), static_cast<uint8_t>(by)};
            if(short_route(a, b, at, styles[style].detour)) continue;
            for(uint8_t i = 0; i < length; ++i)
                carve(static_cast<uint8_t>(at.x + i * dx(dir)), static_cast<uint8_t>(at.y + i * dy(dir)));
            ++made;
#if !defined(__AVM__)
            diagnostics.floor_tiles += length;
#endif
            // A one-cell connector between rooms is a real new boundary too.
            if(length == 1 && neighbors(a) >= 3 && neighbors(b) >= 3)
                door_candidate(at, 2, floor_seed(DOOR_SELECTION));
        }
    }
#if !defined(__AVM__)
    diagnostics.loops = made;
#endif
}

__attribute__((noinline)) void trim_dangling_passages()
{
    // Peel corridor leaves back to a room or surviving junction. Removing a
    // leaf cannot disconnect the remaining floor, and chamber interiors and
    // completed loops have at least two neighbors and survive this pass.
    // Each newly exposed leaf is followed immediately, so one scan suffices
    // even for bent passages and branches whose roots precede their tips.
    for(uint16_t index = 0; index < MAP_W * MAP_H; ++index) {
        if(!(index & 31)) progress();
        Position pos = tile(index);
        while(!wall_at(pos.x, pos.y) && neighbors(pos) <= 1) {
            Position next = pos;
            for(uint8_t dir = 0; dir < 4; ++dir) {
                Position candidate = {static_cast<uint8_t>(pos.x + dx(dir)),
                                      static_cast<uint8_t>(pos.y + dy(dir))};
                if(!wall_at(candidate.x, candidate.y)) { next = candidate; break; }
            }
            uint16_t bit = static_cast<uint16_t>(pos.y * MAP_W + pos.x);
            game.walls[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
#if !defined(__AVM__)
            --diagnostics.floor_tiles;
#endif
            pos = next;
        }
    }
}

static bool throat(Position pos)
{
    return !wall_at(pos.x, pos.y) &&
        ((!wall_at(pos.x - 1, pos.y) && !wall_at(pos.x + 1, pos.y) &&
          wall_at(pos.x, pos.y - 1) && wall_at(pos.x, pos.y + 1)) ||
         (!wall_at(pos.x, pos.y - 1) && !wall_at(pos.x, pos.y + 1) &&
          wall_at(pos.x - 1, pos.y) && wall_at(pos.x + 1, pos.y)));
}

__attribute__((noinline)) void finalize_doors(uint16_t seed)
{
    // Priority already used the independent door seed. Revalidate after loops
    // and erase every temporary high bit and unused slot before gameplay.
    (void)seed;
    uint8_t count = 0;
    for(uint8_t i = 0; i < game.door_count; ++i) {
        Position pos = game.doors[i].pos;
        pos.y &= 31;
        bool duplicate = false;
        for(uint8_t j = 0; j < count; ++j)
            if(game.doors[j].pos == pos || distance_squared(game.doors[j].pos, pos) <= 2) duplicate = true;
        if(!duplicate && throat(pos)) game.doors[count++].pos = pos;
    }
    game.door_count = count;
    for(uint8_t i = count; i < DOORS; ++i) game.doors[i] = {};
}

static Position farthest(Position from, uint16_t& seed, bool first)
{
    uint16_t index = next_random(seed) & 2047;
    uint16_t stride = next_random(seed) | 1;
    Position best = from;
    uint16_t distance = 0;
    for(uint16_t n = 0; n < 2048; ++n, index = (index + stride) & 2047) {
        if(!(n & 63)) progress();
        Position pos = tile(index);
        if(wall_at(pos.x, pos.y) || !roomy(pos) || door_at(pos) != NONE) continue;
        uint16_t d = distance_squared(from, pos);
        if(first || d > distance) { best = pos; distance = d; }
        if(first) break;
    }
    return best;
}

__attribute__((noinline)) void choose_stairs(uint16_t seed)
{
    Position a = farthest({0, 0}, seed, true);
    Position b = farthest(a, seed, false);
    a = farthest(b, seed, false);
    b = farthest(a, seed, false);
    // Stream-derived assignment avoids a constant west-to-east progression.
    if(random(seed, 2)) { game.up = a; game.down = b; }
    else { game.up = b; game.down = a; }
}

} // namespace rogue::generation

namespace rogue {

void make_floor()
{
#if defined(__AVM__)
    begin_generation_render();
#endif
    memset(game.walls, 0xff, sizeof game.walls);
    memset(game.explored, 0, sizeof game.explored);
    memset(game.doors, 0, sizeof game.doors);
    memset(game.monsters, 0, sizeof game.monsters);
    memset(game.ground, 0, sizeof game.ground);
    game.door_count = 0;
#if !defined(__AVM__)
    generation::diagnostics = {};
#endif
    using namespace generation;
    generate_layout(floor_seed(LAYOUT));
    progress(55);
    add_secondary_connections(floor_seed(LOOPS));
    progress(75);
    trim_dangling_passages();
    progress(80);
    finalize_doors(floor_seed(DOOR_SELECTION));
    progress(82);
    choose_stairs(floor_seed(STAIRS));
    progress(87);
    populate_monsters(floor_seed(ENCOUNTERS));
    progress(94);
    populate_items(floor_seed(SUPPLIES), floor_seed(EQUIPMENT));
    game.player = game.has_amulet ? game.down : game.up;
    memset(game.explored, 0, sizeof game.explored);
    SIM_EVENT(sim::EventKind::FloorEntered);
#if defined(__AVM__)
    end_generation_render();
#endif
}

} // namespace rogue
