#include "world.hpp"
#include "game.hpp"
#include <string.h>

namespace rogue {

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

void explore(Position pos)
{
    uint16_t index = static_cast<uint16_t>(pos.y * MAP_W + pos.x);
    game.explored[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

bool explored(Position pos)
{
    uint16_t index = static_cast<uint16_t>(pos.y * MAP_W + pos.x);
    return (game.explored[index >> 3] & (1u << (index & 7))) != 0;
}

uint8_t door_at(Position pos)
{
    for(uint8_t i = 0; i < game.door_count; ++i)
        if(door_position(i) == pos)
            return i;
    return NONE;
}

bool door_open(uint8_t index)
{
    return (game.doors[index].pos.y & 0x80) != 0;
}

void open_door(uint8_t index)
{
    game.doors[index].pos.y |= 0x80;
}

Position door_position(uint8_t index)
{
    Position pos = game.doors[index].pos;
    pos.y &= 0x7f;
    return pos;
}

bool blocked(int16_t x, int16_t y)
{
    if(wall_at(x, y))
        return true;
    uint8_t door = door_at({static_cast<uint8_t>(x), static_cast<uint8_t>(y)});
    return door != NONE && !door_open(door);
}

RayResult scan_ray(Position origin, int8_t dx, int8_t dy, uint8_t range)
{
    RayResult result = {origin, origin, NONE, 0, false};
    int16_t x = origin.x, y = origin.y;
    for(uint8_t step = 0; step < range; ++step) {
        x += dx;
        y += dy;
        if(blocked(x, y)) {
            result.blocker = true;
            break;
        }
        result.before = result.end;
        result.end = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
        ++result.steps;
        result.monster = monster_at(result.end);
        if(result.monster != NONE) break;
    }
    return result;
}

bool can_see(Position pos)
{
    uint8_t tx = pos.x, ty = pos.y;
    int16_t x = game.player.x, y = game.player.y;
    int16_t dx = tx > x ? tx - x : x - tx;
    int16_t dy = ty > y ? ty - y : y - ty;
    if(!in_light_radius(dx, dy, clamp_light_radius(player_light_radius())))
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

struct LightMasks { uint16_t rows[MAX_LIGHT_RADIUS + 1][13]; };

constexpr LightMasks make_light_masks()
{
    LightMasks masks = {};
    for(uint8_t radius = 0; radius <= MAX_LIGHT_RADIUS; ++radius)
        for(uint8_t sy = 0; sy < 13; ++sy)
            for(uint8_t sx = 0; sx < 13; ++sx)
                if(in_light_radius(static_cast<int16_t>(sx) - 6,
                                   static_cast<int16_t>(sy) - 6, radius))
                    masks.rows[radius][sy] |= static_cast<uint16_t>(1u << sx);
    return masks;
}

static constexpr LightMasks PROGMEM light_masks = make_light_masks();

uint16_t light_mask(uint8_t radius, uint8_t sy)
{
    return sy < 13 ? light_masks.rows[clamp_light_radius(radius)][sy] : 0;
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

// Fixed shared-prefix branches; the generator preserves Bresenham tie rules.
#include "ray_sight_generated.hpp"

bool ray_visible(uint8_t tx, uint8_t ty, const uint16_t opaque[13])
{
    if(tx >= 13 || !(light_mask(player_light_radius(), ty) & (1u << tx)))
        return false;
    return ray_unblocked(tx, ty, opaque);
}

bool ray_unblocked(uint8_t tx, uint8_t ty, const uint16_t opaque[13])
{
    uint8_t ray = static_cast<uint8_t>((ty << 3) + (ty << 2) + ty + tx);
    uint16_t offset = static_cast<uint16_t>((static_cast<uint16_t>(ray) << 2) + ray);
    // Read only traversed flash bytes instead of copying a ray onto the stack.
    for(uint8_t i = 0; i < 5; ++i) {
        uint8_t tile = ray_paths.steps[offset + i];
        if(tile == 0xff)
            return true;
        if(opaque[tile >> 4] & (1u << (tile & 0x0f)))
            return false;
    }
    return true;
}

} // namespace rogue
