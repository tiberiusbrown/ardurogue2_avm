#include "game.hpp"
#include <string.h>

namespace rogue {

static const uint8_t ROGUE_ROM_DATA monster_health[] = {0, 3, 5, 8, 12, 17, 48};

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

bool marked(uint16_t bits, uint8_t index)
{
    return (bits & static_cast<uint16_t>(1u << index)) != 0;
}

void mark(uint16_t& bits, uint8_t index)
{
    bits |= static_cast<uint16_t>(1u << index);
}

bool wall_at(int16_t x, int16_t y)
{
    if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H)
        return true;
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    return (game.walls[index >> 3] & (1u << (index & 7))) != 0;
}

void carve(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    game.walls[index >> 3] &= static_cast<uint8_t>(~(1u << (index & 7)));
}

void explore(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>((y >> 1) * (MAP_W / 2) + (x >> 1));
    game.explored[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

bool explored(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>((y >> 1) * (MAP_W / 2) + (x >> 1));
    return (game.explored[index >> 3] & (1u << (index & 7))) != 0;
}

uint8_t door_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < game.door_count; ++i)
        if(game.doors[i].x == x && game.doors[i].y == y)
            return i;
    return NONE;
}

bool blocked(int16_t x, int16_t y)
{
    if(wall_at(x, y))
        return true;
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    return door != NONE && !game.doors[door].open;
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
            mark(game.marks[game.floor].visited_rooms, i);
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
            game.doors[id] = {dx, ay,
                static_cast<uint8_t>(marked(marks.opened_doors, id))};
        }
    }

    const Room& first = game.rooms[0];
    const Room& last = game.rooms[ROOMS - 1];
    game.up_x = static_cast<uint8_t>(first.x + first.w / 2);
    game.up_y = static_cast<uint8_t>(first.y + first.h / 2);
    game.down_x = static_cast<uint8_t>(last.x + last.w / 2);
    game.down_y = static_cast<uint8_t>(last.y + last.h / 2);

    for(uint8_t i = 0; i < ROOMS; ++i) {
        if(!marked(marks.visited_rooms, i))
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
        if(marked(marks.killed_monsters, i))
            continue;
        game.monsters[i] = {x, y, type,
            static_cast<uint8_t>(monster_health[type] + game.floor / 2), 0, i};
    }

    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        const Room& room = game.rooms[(i * 7u + 3u) % ROOMS];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t chance = floor_roll(seed, 12);
        uint8_t type = chance < 4 ? FOOD : chance < 8 ? HEALING :
                       chance < 10 ? SWORD : ARMOR;
        uint8_t amount = type == SWORD || type == ARMOR
            ? static_cast<uint8_t>(1 + game.floor / 4) : 1;
        if(marked(marks.taken_items, i) || (game.floor == FLOORS - 1 && i == 15))
            continue;
        game.ground[i] = {x, y, type, amount};
    }
    if(game.floor == FLOORS - 1 &&
       marked(marks.killed_monsters, MONSTERS - 1) &&
       !marked(marks.taken_items, 15))
        game.ground[15] = {game.down_x, game.down_y, AMULET, 1};
}

bool can_see(uint8_t tx, uint8_t ty)
{
    int16_t x = game.px, y = game.py;
    int16_t dx = tx > x ? tx - x : x - tx;
    int16_t dy = ty > y ? ty - y : y - ty;
    if(dx > 6 || dy > 6)
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

// Same Bresenham ray as can_see, using a precomputed local opacity map.
bool ray_visible(uint8_t tx, uint8_t ty, const uint16_t opaque[13])
{
    int8_t x = 6, y = 6;
    int8_t dx = tx > 6 ? tx - 6 : 6 - tx;
    int8_t dy = ty > 6 ? ty - 6 : 6 - ty;
    int8_t sx = tx > 6 ? 1 : -1;
    int8_t sy = ty > 6 ? 1 : -1;
    int8_t error = dx - dy;
    for(;;) {
        if(x == tx && y == ty)
            return true;
        int8_t twice = static_cast<int8_t>(2 * error);
        if(twice > -dy) { error -= dy; x += sx; }
        if(twice < dx) { error += dx; y += sy; }
        if(x == tx && y == ty)
            return true;
        if(opaque[y] & (1u << x))
            return false;
    }
}

} // namespace rogue
