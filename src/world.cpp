#include "game.hpp"
#include <string.h>

namespace rogue {

static const uint8_t PROGMEM monster_health[] = {0, 3, 5, 8, 12, 17, 48};

uint16_t next_random(uint16_t& state)
{
    uint16_t x = state ? state : 0xace1;
    x ^= static_cast<uint16_t>(x << 7);
    x ^= x >> 9;
    x ^= static_cast<uint16_t>(x << 8);
    state = x;
    return x;
}

uint8_t floor_roll(uint16_t& seed, uint8_t limit)
{
    return static_cast<uint8_t>(next_random(seed) % limit);
}

bool marked(const FloorMarks& marks, FloorMark group, uint8_t index)
{
    uint8_t bit = static_cast<uint8_t>(group + index);
    return (marks.bits[bit >> 3] & (1u << (bit & 7))) != 0;
}

void mark(FloorMarks& marks, FloorMark group, uint8_t index)
{
    uint8_t bit = static_cast<uint8_t>(group + index);
    marks.bits[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
}

bool wall_at(int16_t x, int16_t y)
{
    if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H)
        return true;
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    return (game.walls[index >> 3] & (1u << (index & 7))) != 0;
}

bool wall_exposed(uint8_t x, uint8_t y)
{
    if(x >= MAP_W || y >= MAP_H)
        return false;
    const uint16_t index = static_cast<uint16_t>(y * (MAP_W / 8) + (x >> 3));
    const uint8_t bit = static_cast<uint8_t>(x & 7);
    const uint8_t center = static_cast<uint8_t>(1u << bit);
    if(!(game.walls[index] & center))
        return false;
    // A zero bit in any of the three rows means an adjacent floor tile.
    uint8_t solid = game.walls[index];
    if(y > 0)
        solid &= game.walls[index - MAP_W / 8];
    if(y + 1 < MAP_H)
        solid &= game.walls[index + MAP_W / 8];
    uint8_t neighbors = static_cast<uint8_t>(center | (center << 1) |
                                             (center >> 1));
    if((solid & neighbors) != neighbors)
        return true;
    if(bit == 0 && x > 0) {
        solid = game.walls[index - 1];
        if(y > 0)
            solid &= game.walls[index - MAP_W / 8 - 1];
        if(y + 1 < MAP_H)
            solid &= game.walls[index + MAP_W / 8 - 1];
        return !(solid & 0x80);
    }
    if(bit == 7 && x + 1 < MAP_W) {
        solid = game.walls[index + 1];
        if(y > 0)
            solid &= game.walls[index - MAP_W / 8 + 1];
        if(y + 1 < MAP_H)
            solid &= game.walls[index + MAP_W / 8 + 1];
        return !(solid & 1);
    }
    return false;
}

void carve(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    game.walls[index >> 3] &= static_cast<uint8_t>(~(1u << (index & 7)));
}

void explore(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    game.explored[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

bool explored(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    return (game.explored[index >> 3] & (1u << (index & 7))) != 0;
}

uint8_t door_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < game.door_count; ++i)
        if(game.doors[i].x == x && game.doors[i].y == y)
            return i;
    return NONE;
}

bool door_open(uint8_t index)
{
    return marked(game.marks[game.floor], OPENED_DOORS, index);
}

bool blocked(int16_t x, int16_t y)
{
    if(wall_at(x, y))
        return true;
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    return door != NONE && !door_open(door);
}

bool in_room(uint8_t x, uint8_t y, const Room& room)
{
    return x >= room.x && x < room.x + room.w &&
           y >= room.y && y < room.y + room.h;
}

bool in_any_room(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < ROOMS; ++i)
        if(in_room(x, y, game.rooms[i]))
            return true;
    return false;
}

void visit_room()
{
    for(uint8_t i = 0; i < ROOMS; ++i)
        if(in_room(game.px, game.py, game.rooms[i])) {
            mark(game.marks[game.floor], VISITED_ROOMS, i);
            break;
        }
}

void tunnel(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    int16_t x = ax;
    int16_t y = ay;
    while(x != bx) {
        carve(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        x += x < bx ? 1 : -1;
    }
    while(y != by) {
        carve(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        y += y < by ? 1 : -1;
    }
    carve(bx, by);
}

void make_floor()
{
    memset(game.walls, 0xff, sizeof(game.walls));
    memset(game.explored, 0, sizeof(game.explored));
    memset(game.doors, 0, sizeof(game.doors));
    memset(game.monsters, 0, sizeof(game.monsters));
    memset(game.ground, 0, sizeof(game.ground));
    game.door_count = 0;

    uint16_t seed = static_cast<uint16_t>(game.run_seed ^
        static_cast<uint16_t>((game.floor + 1u) * 0x9e37u));
    FloorMarks& marks = game.marks[game.floor];

    for(uint8_t i = 0; i < ROOMS; ++i) {
        Room& room = game.rooms[i];
        room.w = static_cast<uint8_t>(5 + floor_roll(seed, 6));
        room.h = static_cast<uint8_t>(4 + floor_roll(seed, 4));
        room.x = static_cast<uint8_t>((i % 4) * 16 + 2 +
            floor_roll(seed, static_cast<uint8_t>(13 - room.w)));
        room.y = static_cast<uint8_t>((i / 4) * 10 + 1 +
            floor_roll(seed, static_cast<uint8_t>(9 - room.h)));
        for(uint8_t y = room.y; y < room.y + room.h; ++y)
            for(uint8_t x = room.x; x < room.x + room.w; ++x)
                carve(x, y);
    }

    for(uint8_t i = 1; i < ROOMS; ++i) {
        uint8_t parent = i >= 4 && floor_roll(seed, 2) ? i - 4 : i - 1;
        const Room& a = game.rooms[parent];
        const Room& b = game.rooms[i];
        uint8_t ax = static_cast<uint8_t>(a.x + a.w / 2);
        uint8_t ay = static_cast<uint8_t>(a.y + a.h / 2);
        uint8_t bx = static_cast<uint8_t>(b.x + b.w / 2);
        uint8_t by = static_cast<uint8_t>(b.y + b.h / 2);
        tunnel(ax, ay, bx, by);

        uint8_t dx = static_cast<uint8_t>((static_cast<uint16_t>(ax) + bx) / 2);
        if(game.door_count < DOORS && !in_any_room(dx, ay) &&
           door_at(dx, ay) == NONE && floor_roll(seed, 3) != 0) {
            uint8_t id = game.door_count++;
            game.doors[id] = {dx, ay};
        }
    }

    const Room& first = game.rooms[0];
    const Room& last = game.rooms[ROOMS - 1];
    game.up_x = static_cast<uint8_t>(first.x + first.w / 2);
    game.up_y = static_cast<uint8_t>(first.y + first.h / 2);
    game.down_x = static_cast<uint8_t>(last.x + last.w / 2);
    game.down_y = static_cast<uint8_t>(last.y + last.h / 2);

    for(uint8_t i = 0; i < ROOMS; ++i) {
        if(!marked(marks, VISITED_ROOMS, i))
            continue;
        const Room& room = game.rooms[i];
        for(uint8_t y = room.y; y < room.y + room.h; ++y)
            for(uint8_t x = room.x; x < room.x + room.w; ++x)
                explore(x, y);
    }

    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Room& room = game.rooms[i];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t type = game.floor == FLOORS - 1 && i == MONSTERS - 1
            ? LORD : static_cast<uint8_t>(1 + floor_roll(seed,
                static_cast<uint8_t>(1 + (game.floor < 10 ? game.floor / 2 : 5))));
        if(type > TROLL)
            type = TROLL;
        if(type == LORD) {
            x = game.down_x;
            y = game.down_y;
        } else if((x == game.up_x && y == game.up_y) ||
                  (x == game.down_x && y == game.down_y)) {
            x = static_cast<uint8_t>(room.x + 1);
        }
        if(marked(marks, KILLED_MONSTERS, i))
            continue;
        game.monsters[i] = {x, y, type,
            static_cast<uint8_t>(monster_health[type] + game.floor / 2),
            0, {0, 0}};
    }

    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        const Room& room = game.rooms[(i * 7u + 3u) % ROOMS];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t chance = floor_roll(seed, 12);
        uint8_t type = chance < 4 ? FOOD : chance < 8
            ? static_cast<uint8_t>(HEALING + floor_roll(seed, POTION_COUNT)) :
              chance < 10 ? SWORD : ARMOR;
        uint8_t amount = type == SWORD || type == ARMOR
            ? static_cast<uint8_t>(1 + game.floor / 4) : 1;
        if(marked(marks, TAKEN_ITEMS, i) || (game.floor == FLOORS - 1 && i == 15))
            continue;
        game.ground[i] = {x, y, type, amount};
    }
    if(game.floor == FLOORS - 1 &&
       marked(marks, KILLED_MONSTERS, MONSTERS - 1) &&
       !marked(marks, TAKEN_ITEMS, 15))
        game.ground[15] = {game.down_x, game.down_y, AMULET, 1};
}

bool can_see(uint8_t tx, uint8_t ty)
{
    int16_t x = game.px, y = game.py;
    int16_t dx = tx > x ? tx - x : x - tx;
    int16_t dy = ty > y ? ty - y : y - ty;
    if(!in_light_radius(dx, dy))
        return false;
    int16_t sx = x < tx ? 1 : -1;
    int16_t sy = y < ty ? 1 : -1;
    int16_t error = dx - dy;
    for(;;) {
        if(x == tx && y == ty)
            return true;
        int16_t twice = 2 * error;
        if(twice > -dy) { error -= dy; x += sx; }
        if(twice < dx) { error += dx; y += sy; }
        if(x == tx && y == ty)
            return true;
        if(blocked(x, y))
            return false;
    }
}

// Each local ray crosses at most five tiles before its target. Encode an
// intermediate tile as (row << 4) | column, leaving 0xff as the end marker.
// Generating the paths at compile time preserves can_see's Bresenham tie rules
// without repeating its coordinate arithmetic for every tile on every frame.
struct RayPaths { uint8_t steps[13 * 13 * 5]; };

constexpr RayPaths make_ray_paths()
{
    RayPaths paths = {};
    for(int ty = 0; ty < 13; ++ty)
        for(int tx = 0; tx < 13; ++tx) {
            uint8_t* steps = &paths.steps[(ty * 13 + tx) * 5];
            for(int i = 0; i < 5; ++i)
                steps[i] = 0xff;
            int x = 6, y = 6;
            int dx = tx > 6 ? tx - 6 : 6 - tx;
            int dy = ty > 6 ? ty - 6 : 6 - ty;
            int sx = tx > 6 ? 1 : -1;
            int sy = ty > 6 ? 1 : -1;
            int error = dx - dy;
            int count = 0;
            while(x != tx || y != ty) {
                int twice = 2 * error;
                if(twice > -dy) { error -= dy; x += sx; }
                if(twice < dx) { error += dx; y += sy; }
                if(x != tx || y != ty)
                    steps[count++] = static_cast<uint8_t>((y << 4) | x);
            }
        }
    return paths;
}

static constexpr RayPaths PROGMEM ray_paths = make_ray_paths();

bool ray_visible(uint8_t tx, uint8_t ty, const uint16_t opaque[13])
{
    if(!in_light_radius(static_cast<int16_t>(tx) - LIGHT_RADIUS,
                        static_cast<int16_t>(ty) - LIGHT_RADIUS))
        return false;
    uint8_t ray = static_cast<uint8_t>((ty << 3) + (ty << 2) + ty + tx);
    uint16_t offset = static_cast<uint16_t>((static_cast<uint16_t>(ray) << 2) + ray);
    uint8_t steps[5];
    memcpy_P(steps, ray_paths.steps + offset, sizeof steps);
    for(uint8_t i = 0; i < 5; ++i) {
        uint8_t tile = steps[i];
        if(tile == 0xff)
            return true;
        if(opaque[tile >> 4] & (1u << (tile & 0x0f)))
            return false;
    }
    return true;
}

} // namespace rogue
