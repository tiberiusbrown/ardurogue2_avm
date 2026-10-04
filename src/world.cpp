#include "world.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include <string.h>

namespace rogue {

// Weighted encounter lists from ArduRogue's MAP_GEN_INFOS. Zero entries are
// retried, as in the original generator.
static const uint8_t PROGMEM floor_monsters[FLOORS][6] = {
    {BAT, SNAKE, SNAKE, 0, 0, 0},
    {SNAKE, SNAKE, SNAKE, SNAKE, RATTLESNAKE, RATTLESNAKE},
    {ZOMBIE, ZOMBIE, ZOMBIE, GOBLIN, GOBLIN, PHANTOM},
    {ZOMBIE, GOBLIN, GOBLIN, PHANTOM, ORC, 0},
    {PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM, PHANTOM},
    {GOBLIN, GOBLIN, GOBLIN, ORC, HOBGOBLIN, 0},
    {ORC, ORC, HOBGOBLIN, TARANTULA, MIMIC, 0},
    {ORC, HOBGOBLIN, TARANTULA, TARANTULA, TARANTULA, MIMIC},
    {HOBGOBLIN, HOBGOBLIN, HOBGOBLIN, TARANTULA, MIMIC, INCUBUS},
    {MIMIC, MIMIC, MIMIC, MIMIC, TARANTULA, HOBGOBLIN},
    {TARANTULA, HOBGOBLIN, MIMIC, INCUBUS, INCUBUS, TROLL},
    {HOBGOBLIN, MIMIC, INCUBUS, TROLL, TROLL, GRIFFIN},
    {MIMIC, INCUBUS, TROLL, GRIFFIN, GRIFFIN, DRAGON},
    {INCUBUS, TROLL, GRIFFIN, DRAGON, DRAGON, DRAGON},
    {INCUBUS, ANGEL, ANGEL, DRAGON, DRAGON, DRAGON},
    {INCUBUS, INCUBUS, ANGEL, ANGEL, ANGEL, ANGEL}
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
    // Cumulative integer weights: 25, 20, 30, 15, 10.
    uint8_t chance = floor_equipment_roll(seed, 100);
    return chance < 25 ? DAGGER : chance < 45 ? SPEAR :
        chance < 75 ? LONG_SWORD : chance < 90 ? MACE : TWO_HANDED_SWORD;
}

static uint8_t floor_armor_type(uint16_t& seed)
{
    // Cumulative integer weights: 25, 20, 20, 15, 12, 8.
    uint8_t chance = floor_equipment_roll(seed, 100);
    return chance < 25 ? LEATHER_ARMOR : chance < 45 ? RING_MAIL :
        chance < 65 ? SCALE_MAIL : chance < 80 ? CHAIN_MAIL :
        chance < 92 ? SPLINT_MAIL : PLATE_MAIL;
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
    memset(game.rooms, 0, sizeof(game.rooms));
    memset(game.doors, 0, sizeof(game.doors));
    memset(game.monsters, 0, sizeof(game.monsters));
    memset(game.ground, 0, sizeof(game.ground));
    game.door_count = 0;

    uint16_t seed = static_cast<uint16_t>(game.run_seed ^
        static_cast<uint16_t>((game.floor + 1u) * 0x9e37u) ^
        (game.has_amulet ? 0xa5c3u : 0u));

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
           door_at({dx, ay}) == NONE && floor_roll(seed, 3) != 0) {
            uint8_t id = game.door_count++;
            game.doors[id] = {{dx, ay}};
        }
    }

    const Room& first = game.rooms[0];
    const Room& last = game.rooms[ROOMS - 1];
    game.up = {static_cast<uint8_t>(first.x + first.w / 2),
               static_cast<uint8_t>(first.y + first.h / 2)};
    game.down = {static_cast<uint8_t>(last.x + last.w / 2),
                 static_cast<uint8_t>(last.y + last.h / 2)};
    game.player = game.has_amulet ? game.down : game.up;

    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Room& room = game.rooms[i];
        Position pos = {
            static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2)),
            static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2))};
        uint8_t type = 0;
        if(!game.has_amulet && game.floor == FLOORS - 1 && i == MONSTERS - 1)
            type = LORD;
        else
            while(!type)
                type = floor_monsters[game.floor][floor_roll(seed, 6)];
        if(type == LORD) {
            pos = game.down;
        } else if(pos == game.up || pos == game.down) {
            pos.x = static_cast<uint8_t>(room.x + 1);
        }
        game.monsters[i] = {pos, type, monster_info(type).health,
            0, {0, 0}, 0};
        if(type == MIMIC)
            set_mimic_appearance(game.monsters[i], static_cast<MimicAppearance>(
                floor_roll(seed, MIMIC_APPEARANCE_COUNT)));
    }

    // The ascent has fresh threats but no replenishing ordinary supplies.
    if(game.has_amulet) return;
    // Keep equipment rolls out of terrain/encounter retry loops: those loops
    // merge RNG sequences and can bias rare combinations in a small state space.
    uint16_t equipment_seed = static_cast<uint16_t>(game.run_seed ^
        static_cast<uint16_t>((game.floor + 1u) * 0x85ebu) ^ 0x51edu);
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        const Room& room = game.rooms[(i * 7u + 3u) % ROOMS];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t chance = floor_roll(seed, 72);
        uint8_t type = chance < 20 ? FOOD : chance < 40
            ? static_cast<uint8_t>(HEALING + floor_roll(seed, POTION_COUNT)) :
              chance < 48 ? static_cast<uint8_t>(SCROLL_IDENTIFY +
                                                floor_roll(seed, SCROLL_COUNT)) :
              chance < 56 ? floor_weapon_type(equipment_seed) :
              chance < 64 ? floor_armor_type(equipment_seed) :
              chance < 68 ? static_cast<uint8_t>(WAND_FORCE +
                                                 floor_roll(seed, WAND_COUNT)) :
              chance < 70
                ? static_cast<uint8_t>(RING_SEE_INVISIBLE +
                                       floor_roll(seed, RING_COUNT))
                : static_cast<uint8_t>(AMULET_SPEED +
                                       floor_roll(seed, AMULET_COUNT));
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
        game.ground[i] = {{x, y}, {type, info}};
    }
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
