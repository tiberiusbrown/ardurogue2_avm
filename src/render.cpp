#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "status.hpp"
#include "world.hpp"

namespace rogue {

static bool play_render_pending = false;

void defer_play_render() { play_render_pending = true; }

void restore_play_render()
{
    if(play_render_pending) render_play();
}

void status_clear()
{
    avm_draw_filled_rect_black(65, 23, 63, 41);
    reset_status_position();
}

static const uint16_t PROGMEM monster_icons[] = {
    0x0000, // none
    0x0fa4, // bat
    0x0bd0, // snake
    0x0f5a, // rattlesnake
    0x9db9, // zombie
    0x0bf0, // goblin
    0x0f52, // phantom
    0x0f9f, // orc
    0x07a0, // tarantula
    0x0f2c, // hobgoblin
    0x0f2f, // mimic
    0x09f9, // incubus
    0x01f1, // troll
    0x069d, // griffin
    0xf996, // dragon
    0x0e5e, // fallen angel
    0x0f88, // Lord of Darkness
};
// Shared visual categories are independent of ItemType ordering and roster size.
enum ItemIconCategory : uint8_t {
    ICON_NONE, ICON_AMMO, ICON_FOOD, ICON_POTION, ICON_WEAPON, ICON_ARMOR,
    ICON_AMULET, ICON_RING, ICON_SCROLL, ICON_WAND, ITEM_ICON_CATEGORIES
};
static const uint16_t PROGMEM item_icons[] = {
    0x0000, 0x6f69, 0x9429, 0x0bb0, 0x04f4, 0x0f90,
    0x0606, 0x0aaa, 0x01b3, 0x1248
};
static_assert(sizeof(item_icons) / sizeof(item_icons[0]) == ITEM_ICON_CATEGORIES,
              "item icon categories changed");

uint16_t item_icon(uint8_t type)
{
    if(is_ammo(type)) return item_icons[ICON_AMMO];
    if(is_weapon(type)) return item_icons[ICON_WEAPON];
    if(is_armor(type)) return item_icons[ICON_ARMOR];
    if(is_potion(type)) return item_icons[ICON_POTION];
    if(is_scroll(type)) return item_icons[ICON_SCROLL];
    if(is_amulet(type) || type == YENDOR_AMULET) return item_icons[ICON_AMULET];
    if(is_ring(type)) return item_icons[ICON_RING];
    if(is_wand(type)) return item_icons[ICON_WAND];
    return item_icons[type == FOOD ? ICON_FOOD : ICON_NONE];
}

uint16_t mimic_icon(MimicAppearance appearance)
{
    switch(appearance) {
    case MIMIC_SCROLL: return item_icons[ICON_SCROLL];
    case MIMIC_POTION: return item_icons[ICON_POTION];
    case MIMIC_AMULET: return item_icons[ICON_AMULET];
    case MIMIC_RING: return item_icons[ICON_RING];
    case MIMIC_WAND: return item_icons[ICON_WAND];
    default: return item_icons[ICON_NONE];
    }
}
static constexpr uint16_t PLAYER_ICON = 0x6ff6;
static constexpr uint16_t DOWN_STAIRS_ICON = 0xfec8;
static constexpr uint16_t UP_STAIRS_ICON = 0x8cef;
static constexpr uint16_t CLOSED_DOOR_ICON = 0xefbe;
static constexpr uint16_t OPEN_DOOR_ICON = 0xe11e;

static void pixel(int16_t x, int16_t y)
{
    if(x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] |= static_cast<uint8_t>(1u << (y & 7));
}

// All map symbols fit on screen. Write a whole vertical nibble at once.
__attribute__((noinline)) static void column(uint8_t x, uint8_t y, uint8_t bits)
{
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    uint8_t shift = y & 7;
    __avm_framebuffer[offset] |= static_cast<uint8_t>(bits << shift);
    if(shift > 4 && y < 60)
        __avm_framebuffer[offset + 128] |= static_cast<uint8_t>(bits >> (8 - shift));
}

static void icon(uint16_t shape, uint8_t x, uint8_t y)
{
    avm_draw_filled_rect_black(x, y, 4, 4);
    for(uint8_t col = 0; col < 4; ++col) {
        column(static_cast<uint8_t>(x + col), y,
               static_cast<uint8_t>(shape >> 12));
        shape = static_cast<uint16_t>(shape << 4);
    }
}

static bool screen_tile(Position pos, uint8_t& sx, uint8_t& sy)
{
    int16_t dx = static_cast<int16_t>(pos.x) - game.player.x + 6;
    int16_t dy = static_cast<int16_t>(pos.y) - game.player.y + 6;
    if(dx < 0 || dx >= 13 || dy < 0 || dy >= 13)
        return false;
    sx = static_cast<uint8_t>(dx);
    sy = static_cast<uint8_t>(dy);
    return true;
}

static void animation_wait()
{
    uint16_t until = static_cast<uint16_t>(avm_millis() + 100);
    while(static_cast<int16_t>(avm_millis() - until) < 0)
        avm_idle();
}

static void effect_pixel(int16_t x, int16_t y, bool lit)
{
    if(x < 0 || x >= 64 || y < 0 || y >= 64) return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    uint8_t mask = static_cast<uint8_t>(1u << (y & 7));
    if(lit) __avm_framebuffer[offset] |= mask;
    else __avm_framebuffer[offset] &= static_cast<uint8_t>(~mask);
}

// ArduRogue clears the sprite's set pixels at all eight neighboring pixel
// positions before setting the four sprite columns at the current tile.
static void effect_sprite(uint8_t sx, uint8_t sy)
{
    constexpr uint16_t shape = 0x0eae;
    int16_t x = static_cast<int16_t>(sx) * 5;
    int16_t y = static_cast<int16_t>(sy) * 5;
    for(int8_t oy = -1; oy <= 1; ++oy)
        for(int8_t ox = -1; ox <= 1; ++ox) {
            if(!ox && !oy) continue;
            for(uint8_t col = 0; col < 4; ++col) {
                uint8_t bits = static_cast<uint8_t>(
                    (shape >> (12 - col * 4)) & 0x0f);
                for(uint8_t row = 0; row < 4; ++row)
                    if(bits & (1u << row))
                        effect_pixel(x + ox + col, y + oy + row, false);
            }
        }
    for(uint8_t col = 0; col < 4; ++col) {
        uint8_t bits = static_cast<uint8_t>(
            (shape >> (12 - col * 4)) & 0x0f);
        for(uint8_t row = 0; row < 4; ++row)
            if(bits & (1u << row))
                effect_pixel(x + col, y + row, true);
    }
}

static void draw_effect_tile(int16_t x, int16_t y)
{
    if(x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) {
        uint8_t sx, sy;
        if(screen_tile({static_cast<uint8_t>(x), static_cast<uint8_t>(y)}, sx, sy))
            effect_sprite(sx, sy);
    }
}

static void animation_tile(int16_t x, int16_t y)
{
    draw_effect_tile(x, y);
    avm_display(false);
    animation_wait();
}

__attribute__((noinline)) void animate_ray(Position origin, int8_t dx,
                                           int8_t dy, uint8_t steps)
{
    render_play();
    int16_t x = origin.x, y = origin.y;
    for(uint8_t step = 0; step < steps; ++step) {
        x += dx;
        y += dy;
        animation_tile(x, y);
    }
    render_play();
    avm_display(false);
}

// Each nibble is one vertical column of a 4x4 arrow. Flash-only sprites.
__attribute__((noinline)) void animate_arrow(Position origin, int8_t dx, int8_t dy, uint8_t steps)
{
#if defined(ARDUROGUE2_BENCH)
    // Benchmark contract excludes animation; retain production render setup.
    (void)origin; (void)dx; (void)dy; (void)steps;
    render_play(); avm_display(false);
#else
    static const uint16_t PROGMEM arrows[] = {0x2f20, 0x44e4, 0x4f40, 0x4e44};
    uint8_t direction = dy < 0 ? 0 : dx > 0 ? 1 : dy > 0 ? 2 : 3;
    Position pos = origin;
    for(uint8_t step = 0; step < steps; ++step) {
        render_play();
        pos.x = static_cast<uint8_t>(pos.x + dx);
        pos.y = static_cast<uint8_t>(pos.y + dy);
        uint8_t sx, sy;
        if(screen_tile(pos, sx, sy)) icon(arrows[direction], sx * 5, sy * 5);
        avm_display(false);
        uint16_t until = static_cast<uint16_t>(avm_millis() + 60);
        while(static_cast<int16_t>(avm_millis() - until) < 0) avm_idle();
    }
    render_play();
    avm_display(false);
#endif
}

__attribute__((noinline)) void animate_fire_burst(Position center)
{
    static const int8_t PROGMEM offsets[] = {
        0, 0, -1, -1, 0, -1, 1, -1, 1, 0,
        1, 1, 0, 1, -1, 1, -1, 0
    };
    render_play();
    for(uint8_t i = 0; i < 18; i += 2)
        animation_tile(static_cast<int16_t>(center.x) + offsets[i],
                       static_cast<int16_t>(center.y) + offsets[i + 1]);
    render_play();
    avm_display(false);
}

__attribute__((noinline)) static void draw_spreading_rays(
    Position origin, const uint8_t steps[4])
{
    static const int8_t PROGMEM directions[] = {0, -1, 1, 0, 0, 1, -1, 0};
    uint8_t maximum = 0;
    for(uint8_t i = 0; i < 4; ++i)
        if(steps[i] > maximum) maximum = steps[i];
    for(uint8_t step = 1; step <= maximum; ++step) {
        for(uint8_t i = 0; i < 4; ++i)
            if(step <= steps[i])
                draw_effect_tile(static_cast<int16_t>(origin.x) +
                                     directions[i * 2] * step,
                                 static_cast<int16_t>(origin.y) +
                                     directions[i * 2 + 1] * step);
        avm_display(false);
        animation_wait();
    }
}

__attribute__((noinline)) void animate_spreading_rays(
    Position origin, const uint8_t steps[4])
{
    render_play();
    draw_spreading_rays(origin, steps);
    render_play();
    avm_display(false);
}

__attribute__((noinline)) static void draw_fire_bursts(
    const Position* centers, uint8_t count, bool powerful)
{
    static const int8_t PROGMEM offsets[] = {
        0, 0, -1, -1, 0, -1, 1, -1, 1, 0,
        1, 1, 0, 1, -1, 1, -1, 0
    };
    for(uint8_t i = 0; i < 18; i += 2) {
        for(uint8_t j = 0; j < count; ++j)
            draw_effect_tile(static_cast<int16_t>(centers[j].x) + offsets[i],
                             static_cast<int16_t>(centers[j].y) + offsets[i + 1]);
        avm_display(false);
        animation_wait();
    }
    if(powerful) {
        for(uint8_t frame = 0; frame < 4; ++frame) {
            for(uint8_t j = 0; j < count; ++j) {
                for(int8_t n = -2; n <= 2; ++n) {
                    int8_t ox = frame == 0 || frame == 2 ? n :
                        frame == 1 ? 2 : -2;
                    int8_t oy = frame == 0 ? -2 : frame == 2 ? 2 : n;
                    if((frame == 1 || frame == 3) && (n == -2 || n == 2))
                        continue;
                    draw_effect_tile(static_cast<int16_t>(centers[j].x) + ox,
                                     static_cast<int16_t>(centers[j].y) + oy);
                }
            }
            avm_display(false);
            animation_wait();
        }
    }
}

__attribute__((noinline)) void animate_fire_bursts(
    const Position* centers, uint8_t count, bool powerful)
{
    render_play();
    draw_fire_bursts(centers, count, powerful);
    render_play();
    avm_display(false);
}

static bool in_sight(Position pos, const uint16_t sight[13],
              uint8_t& sx, uint8_t& sy)
{
    return screen_tile(pos, sx, sy) && (sight[sy] & (1u << sx));
}

// Keep viewport bounds separate from the visibility masks so the temporary
// ray blockers can be released before drawing.
struct DungeonView {
    int16_t left, top;
    uint8_t first_sx, end_sx, first_sy, end_sy;
};

static uint16_t map_row_bits(const uint8_t* map, uint8_t y, int16_t left,
                             uint8_t width)
{
    uint16_t opaque = static_cast<uint16_t>((1u << width) - 1);
    if(y >= MAP_H || left <= -static_cast<int16_t>(width) || left >= MAP_W)
        return opaque;
    uint8_t first = left < 0 ? static_cast<uint8_t>(-left) : 0;
    uint8_t end = left + width > MAP_W
        ? static_cast<uint8_t>(MAP_W - left) : width;
    uint8_t x = static_cast<uint8_t>(left + first);
    uint8_t shift = x & 7;
    uint8_t count = end - first;
    const uint8_t* source = map + static_cast<uint16_t>(y) * (MAP_W / 8) +
        (x >> 3);
    // The clipped run determines which bytes exist in this map row. Keep
    // the third byte separate so extraction needs only a 16-bit temporary.
    uint16_t bits = source[0];
    if(shift + count > 8)
        bits |= static_cast<uint16_t>(source[1]) << 8;
    bits >>= shift;
    if(shift + count > 16)
        bits |= static_cast<uint16_t>(source[2]) << (16 - shift);
    uint16_t run = static_cast<uint16_t>((1u << count) - 1);
    uint16_t valid = static_cast<uint16_t>(run << first);
    return static_cast<uint16_t>((opaque & ~valid) | ((bits & run) << first));
}

uint16_t wall_row_bits(uint8_t y, int16_t left)
{
    return map_row_bits(game.walls, y, left, 13);
}

__attribute__((noinline)) static void build_view_walls(
    const DungeonView& view, uint16_t walls[13])
{
    for(uint8_t sy = 0; sy < 13; ++sy)
        walls[sy] = 0x1fff;
    for(uint8_t sy = view.first_sy; sy < view.end_sy; ++sy)
        walls[sy] = wall_row_bits(static_cast<uint8_t>(view.top + sy), view.left);
    for(uint8_t i = 0; i < game.door_count; ++i) {
        Position door = door_position(i);
        uint8_t sx, sy;
        if(!door_open(i) && screen_tile(door, sx, sy))
            walls[sy] |= static_cast<uint16_t>(1u << sx);
    }
}

__attribute__((noinline)) static void reveal_view_floor(
    const DungeonView& view, uint16_t sight[13], uint8_t radius)
{
    uint16_t valid = static_cast<uint16_t>(((1u << view.end_sx) - 1) &
                                          ~((1u << view.first_sx) - 1));
    for(uint8_t sy = 0; sy < 13; ++sy) {
        if(sy < view.first_sy || sy >= view.end_sy) {
            sight[sy] = 0;
            continue;
        }
        sight[sy] &= static_cast<uint16_t>(valid & light_mask(radius, sy));
    }
}

__attribute__((noinline)) static void reveal_view_walls(
    const DungeonView& view, uint16_t sight[13], const uint16_t walls[13],
    uint8_t radius)
{
    // A ray to the center of a corridor wall can cross an earlier wall.
    // Reveal walls touching visible, non-opaque floor within the circular
    // light radius, without extending visibility through closed doors.
    uint16_t valid = static_cast<uint16_t>(((1u << view.end_sx) - 1) &
                                          ~((1u << view.first_sx) - 1));
    for(uint8_t sy = view.first_sy; sy < view.end_sy; ++sy) {
        uint16_t floor_sight = sight[sy] & ~walls[sy];
        uint16_t adjacent = static_cast<uint16_t>((floor_sight << 1) |
                                                   (floor_sight >> 1));
        if(sy > 0)
            adjacent |= sight[sy - 1] & ~walls[sy - 1];
        if(sy < 12)
            adjacent |= sight[sy + 1] & ~walls[sy + 1];
        sight[sy] |= static_cast<uint16_t>(adjacent & walls[sy] & valid &
                                          light_mask(radius, sy));
    }
}

__attribute__((noinline)) static void explore_view(
    const DungeonView& view, const uint16_t sight[13])
{
    uint8_t x = static_cast<uint8_t>(view.left + view.first_sx);
    uint8_t shift = x & 7;
    uint8_t count = view.end_sx - view.first_sx;
    uint8_t* row = game.explored + static_cast<uint16_t>(view.top + view.first_sy) *
                                      (MAP_W / 8);
    for(uint8_t sy = view.first_sy; sy < view.end_sy; ++sy, row += MAP_W / 8) {
        uint16_t bits = static_cast<uint16_t>(sight[sy] >> view.first_sx);
        uint8_t* output = row + (x >> 3);
        output[0] |= static_cast<uint8_t>(bits << shift);
        if(shift + count > 8)
            output[1] |= static_cast<uint8_t>(bits >> (8 - shift));
        if(shift + count > 16)
            output[2] |= static_cast<uint8_t>(bits >> (16 - shift));
    }
}

struct TerrainRow {
    uint16_t walls, shown_walls;
};

__attribute__((noinline)) static TerrainRow terrain_row(
    uint8_t y, int16_t left)
{
    // The halo includes neighbors beyond the viewport. Missing map rows and
    // columns are solid, matching wall_exposed's clipped neighborhoods.
    uint16_t center = map_row_bits(game.walls, y, left - 1, 15);
    uint16_t solid = center;
    if(y > 0) solid &= map_row_bits(game.walls, y - 1, left - 1, 15);
    if(y + 1 < MAP_H) solid &= map_row_bits(game.walls, y + 1, left - 1, 15);
    uint16_t exposed = static_cast<uint16_t>(center &
        ~(solid & (solid << 1) & (solid >> 1)));
    uint16_t walls = static_cast<uint16_t>((center >> 1) & 0x1fff);
    uint16_t shown = static_cast<uint16_t>((exposed >> 1) &
        map_row_bits(game.explored, y, left, 13));
    return {walls, shown};
}

__attribute__((always_inline)) static inline void draw_terrain_row(
    uint16_t floor, uint16_t walls, uint16_t below, uint8_t sy)
{
    uint8_t y = static_cast<uint8_t>(sy * 5);
    uint8_t shift = y & 7;
    uint8_t* output = __avm_framebuffer + static_cast<uint16_t>(y >> 3) * 128;
    for(uint8_t sx = 0; sx < 13; ++sx, output += 5,
        floor >>= 1, walls >>= 1, below >>= 1) {
        if(walls & 1) {
            uint8_t bits = below & 1 ? 0x1f : 0x0f;
            uint8_t first = static_cast<uint8_t>(bits << shift);
            for(uint8_t col = 0; col < 4; ++col) output[col] |= first;
            if(shift > 3 && y < 60) {
                uint8_t second = static_cast<uint8_t>(bits >> (8 - shift));
                for(uint8_t col = 0; col < 4; ++col) output[128 + col] |= second;
            }
            if(walls & 2) {
                output[4] |= static_cast<uint8_t>(0x0f << shift);
                if(shift > 4 && y < 60)
                    output[132] |= static_cast<uint8_t>(0x0f >> (8 - shift));
            }
        } else if(floor & 1) {
            if(shift < 6) output[2] |= static_cast<uint8_t>(1u << (shift + 2));
            else output[130] |= static_cast<uint8_t>(1u << (shift - 6));
        }
    }
}

__attribute__((noinline)) static void build_view_terrain(
    const DungeonView& view, TerrainRow terrain[13])
{
    uint16_t valid = static_cast<uint16_t>(((1u << view.end_sx) - 1) &
                                          ~((1u << view.first_sx) - 1));
    for(uint8_t sy = view.first_sy; sy < view.end_sy; ++sy) {
        terrain[sy] = terrain_row(static_cast<uint8_t>(view.top + sy), view.left);
        terrain[sy].shown_walls &= valid;
    }
}

__attribute__((noinline)) static void draw_view_terrain(
    const DungeonView& view, const uint16_t sight[13], const TerrainRow terrain[13])
{
    // All exploration and wall exposure are complete before joining tiles.
    for(uint8_t sy = view.first_sy; sy < view.end_sy; ++sy)
        draw_terrain_row(static_cast<uint16_t>(sight[sy] & ~terrain[sy].walls),
                         terrain[sy].shown_walls,
                         sy + 1 < view.end_sy ? terrain[sy + 1].shown_walls : 0, sy);
}

__attribute__((noinline)) static void draw_view_objects(const uint16_t sight[13])
{
    for(uint8_t i = 0; i < game.door_count; ++i) {
        Position door = door_position(i);
        uint8_t sx, sy;
        if(screen_tile(door, sx, sy) && explored(door))
            icon(door_open(i) ? OPEN_DOOR_ICON : CLOSED_DOOR_ICON,
                 static_cast<uint8_t>(sx * 5),
                 static_cast<uint8_t>(sy * 5));
    }
    uint8_t sx, sy;
    if(screen_tile(game.up, sx, sy) && explored(game.up))
        icon(UP_STAIRS_ICON, static_cast<uint8_t>(sx * 5),
             static_cast<uint8_t>(sy * 5));
    if(game.floor < FLOORS - 1 &&
       screen_tile(game.down, sx, sy) && explored(game.down))
        icon(DOWN_STAIRS_ICON, static_cast<uint8_t>(sx * 5),
             static_cast<uint8_t>(sy * 5));
    for(const GroundItem& ground : game.ground)
        if(ground.item.type && in_sight(ground.pos, sight, sx, sy))
            icon(item_icon(ground.item.type), static_cast<uint8_t>(sx * 5),
                 static_cast<uint8_t>(sy * 5));
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Monster& monster = game.monsters[i];
        if(player_can_see_monster(i) &&
           in_sight(monster.pos, sight, sx, sy))
            icon(monster.type == MIMIC && !(monster.state & MON_AGGRO)
                     ? mimic_icon(mimic_appearance(monster))
                     : monster_icons[monster.type],
                 static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    }
    icon(PLAYER_ICON, 30, 30);
}

__attribute__((noinline)) static void reveal_view(
    const DungeonView& view, uint16_t sight[13], uint16_t walls[13])
{
    uint8_t radius = player_light_radius();
    build_view_walls(view, walls);
    ray_sight(walls, sight);
    reveal_view_floor(view, sight, radius);
    reveal_view_walls(view, sight, walls, radius);
    explore_view(view, sight);
}

__attribute__((noinline)) static void render_dungeon_view()
{
    static DungeonView view;
    static uint16_t sight[13];
    // Rendering cannot reenter itself. Reuse the blockers' storage for terrain
    // rows after visibility, keeping pagination off the small VM stack.
    static union {
        uint16_t walls[13];
        TerrainRow terrain[13];
    } scratch;
    view.left = static_cast<int16_t>(game.player.x) - 6;
    view.top = static_cast<int16_t>(game.player.y) - 6;
    view.first_sx = view.left < 0 ? static_cast<uint8_t>(-view.left) : 0;
    view.end_sx = view.left + 13 > MAP_W
        ? static_cast<uint8_t>(MAP_W - view.left) : 13;
    view.first_sy = view.top < 0 ? static_cast<uint8_t>(-view.top) : 0;
    view.end_sy = view.top + 13 > MAP_H
        ? static_cast<uint8_t>(MAP_H - view.top) : 13;
    reveal_view(view, sight, scratch.walls);
    build_view_terrain(view, scratch.terrain);
    draw_view_terrain(view, sight, scratch.terrain);
    draw_view_objects(sight);
}

__attribute__((noinline)) static void render_stats()
{
    avm_draw_textf_P(67, 7, F("D%u LV%u"), game.floor + 1, game.level);
    avm_draw_textf_P(67, 15, F("HP%u/%u"), game.hp, player_max_hp());
}

__attribute__((noinline)) void render_play()
{
    play_render_pending = false;
    avm_draw_filled_rect_black(0, 0, 65, 64);
    avm_draw_filled_rect_black(65, 0, 63, 23);
    render_dungeon_view();
    for(uint8_t y = 0; y < 64; ++y)
        pixel(64, y);
    render_stats();
}

__attribute__((noinline)) void update_generation_render(uint8_t percent)
{
    // Input is paused during make_floor, so reuse its repeat timer and menu
    // fields instead of allocating permanent loading state or map scratch.
    if(percent <= 100) ui.selection = percent;
    uint16_t now = avm_millis();
    if(static_cast<int16_t>(now - ui.next_repeat_ms) < 0) return;
    ui.next_repeat_ms = static_cast<uint16_t>(now + 150);

    static const uint8_t PROGMEM orbit[] = {
        30, 6, 40, 10, 44, 20, 40, 30, 30, 34, 20, 30, 16, 20, 20, 10
    };
    // Keep the display buffer intact between displays. Only repaint the two
    // moving regions; labels, divider, stats and status retain their pixels.
    avm_draw_filled_rect_black(16, 6, 32, 32);
    icon(PLAYER_ICON, 30, 20);
    for(uint8_t trail = 0; trail < 3; ++trail) {
        uint8_t at = static_cast<uint8_t>(((ui.held_direction - trail) & 7) * 2);
        avm_draw_filled_rect_white(orbit[at], orbit[at + 1],
                                  static_cast<uint8_t>(4 - trail),
                                  static_cast<uint8_t>(4 - trail));
    }
    avm_draw_filled_rect_black(8, 53, 48, 4);
    avm_draw_filled_rect_white(8, 53, static_cast<uint8_t>(ui.selection * 48u / 100), 4);
    // A moving glint keeps the bar alive even within a long generation phase.
    uint8_t glint = static_cast<uint8_t>(ui.held_direction * 3u % 44);
    avm_draw_filled_rect_white(static_cast<int16_t>(8 + glint), 54, 4, 2);
    ++ui.held_direction;
    avm_display(false);
}

__attribute__((noinline)) void begin_generation_render()
{
    play_render_pending = false;
    avm_draw_filled_rect_black(0, 0, 128, 64);
    for(uint8_t y = 0; y < 64; ++y) pixel(64, y);
    render_stats();
    status_clear();
    status(F("Preparing a new floor."));
    avm_draw_text_P(6, 46, F("Generating..."));
    avm_draw_filled_rect_white(7, 52, 50, 6);
    ui.selection = 0;
    ui.held_direction = 0;
    ui.next_repeat_ms = avm_millis();
    update_generation_render(0);
}

void end_generation_render()
{
    ui.next_repeat_ms = avm_millis();
    update_generation_render(100);
    status_clear();
    ui.selection = 0;
    ui.held_direction = 0;
    ui.repeat_suppressed = true;
    ui.dirty = true;
}

// Avoid carrying all screen renderers' locals into render_play's frame.
__attribute__((noinline)) static void render_title()
{
    avm_draw_text_P(22, 14, F("ARDUROGUE 2"));
    avm_draw_text_P(14, 30, ui.has_save ? F("A: CONTINUE") : F("A: NEW GAME"));
    if(ui.has_save)
        avm_draw_text_P(14, 40, F("B: NEW GAME"));
    avm_draw_textf_P(14, 56, F("BEST %u"), game.best_score);
}

__attribute__((noinline)) static void render_menu()
{
    static const char AVM_PROGMEM* const AVM_PROGMEM names[] = {
        F("WAIT"), F("USE ITEM"), F("DROP ITEM"), F("Throw/Shoot"),
        F("FULL MAP"), F("SAVE & EXIT"), F("ABANDON")
    };
    avm_draw_text_P(10, 8, F("ACTION MENU"));
    for(uint8_t i = 0; i < 7; ++i) {
        if(i == ui.selection)
            avm_draw_text_P(4, static_cast<int16_t>(17 + 7 * i), F(">"));
        avm_draw_text_P(12, static_cast<int16_t>(17 + 7 * i), names[i]);
    }
}

// Keep the modal rendering boundary visible to complete-response profiling.
__attribute__((noinline)) void render_inventory(const char AVM_PROGMEM* prompt,
                      const InventoryView& view, uint8_t selection, uint8_t top,
                      uint8_t total)
{
    avm_draw_filled_rect_black(0, 0, 128, 64);
    avm_draw_text_P(1, 7, prompt);
    avm_draw_filled_rect_white(1, 9, 127, 1);
    if(!total) {
        avm_draw_text_P(8, 18, F("Empty"));
        return;
    }
    for(uint8_t row = 0; row < INVENTORY_VISIBLE_ROWS; ++row) {
        uint8_t index = static_cast<uint8_t>(top + row);
        if(index >= total) break;
        int16_t y = static_cast<int16_t>(18 + row * 7);
        uint8_t entry = view.entry_at(index);
        if(entry >= INVENTORY) {
            switch(entry - INVENTORY) {
            case WEAPONS: avm_draw_text_P(1, y, F("Weapons")); break;
            case AMMO: avm_draw_text_P(1, y, F("Ammo")); break;
            case ARMORS: avm_draw_text_P(1, y, F("Armor")); break;
            case RINGS: avm_draw_text_P(1, y, F("Rings")); break;
            case AMULETS: avm_draw_text_P(1, y, F("Amulets")); break;
            case WANDS: avm_draw_text_P(1, y, F("Wands")); break;
            case POTIONS: avm_draw_text_P(1, y, F("Potions")); break;
            case SCROLLS: avm_draw_text_P(1, y, F("Scrolls")); break;
            case FOODS: avm_draw_text_P(1, y, F("Food")); break;
            default: avm_draw_text_P(1, y, F("Quest")); break;
            }
            continue;
        }
        const Item& item = game.inventory[entry];
        if(entry == selection) {
            avm_draw_filled_rect_white(7, y - 6, 121, 7);
            avm_set_text_mode(AVM_TEXT_BLACK_TRANSPARENT);
        }
        draw_item_text(8, y, item);
        if(game.weapon_slot == entry || game.armor_slot == entry ||
           game.amulet_slot == entry || game.ring_slots[0] == entry ||
           game.ring_slots[1] == entry)
            avm_draw_text_P(116, y, F("*"));
        if(item_is_identified(item) && item_is_cursed(item))
            avm_draw_text_P(123, y, F("!"));
        avm_set_text_mode(AVM_TEXT_OVERWRITE);
    }
}

__attribute__((noinline)) static void render_throw_direction()
{
    avm_draw_text_P(8, 12, ui.mode == WAND_DIRECTION
        ? F("USE WAND") : F("Throw/Shoot"));
    draw_item_text(8, 27, game.inventory[ui.selection]);
    avm_draw_text_P(8, 43, F("D-PAD: DIRECTION"));
    avm_draw_text_P(8, 56, F("B: BACK"));
}

__attribute__((noinline)) static void render_full_map()
{
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) {
            if(!explored({x, y}))
                continue;
            if(wall_exposed(x, y)) {
                pixel(x * 2, y * 2);
                pixel(x * 2 + 1, y * 2);
                pixel(x * 2, y * 2 + 1);
                pixel(x * 2 + 1, y * 2 + 1);
            } else if(x + 6 >= game.player.x && x <= game.player.x + 6 &&
                      y + 6 >= game.player.y && y <= game.player.y + 6 &&
                      can_see({x, y})) {
                pixel(x * 2, y * 2);
            }
        }
    pixel(game.player.x * 2, game.player.y * 2);
    pixel(game.player.x * 2 + 1, game.player.y * 2 + 1);
}

__attribute__((noinline)) static void render_end()
{
    avm_draw_text_P(16, 15, session.result == ESCAPED ? F("YOU ESCAPED!") :
        session.result == ABANDONED ? F("ABANDONED") : F("YOU DIED"));
    avm_draw_textf_P(16, 31, F("SCORE %u"), game.score);
    avm_draw_textf_P(16, 41, F("BEST %u"), game.best_score);
    avm_draw_text_P(16, 57, F("A: TITLE"));
}

__attribute__((noinline)) void render()
{
    play_render_pending = false;
    if(ui.mode != PLAY)
        avm_draw_filled_rect_black(0, 0, 128, 64);
    switch(ui.mode) {
    case TITLE: render_title(); break;
    case PLAY: render_play(); break;
    case MENU: render_menu(); break;
    case PROJECTILE_DIRECTION: render_throw_direction(); break;
    case WAND_DIRECTION: render_throw_direction(); break;
    case FULL_MAP: render_full_map(); break;
    case END: render_end(); break;
    }
    avm_display(false);
    ui.dirty = false;
}

static const uint8_t AVM_PROGMEM yesno_buttons[] = {
    7, 7,
    0x1c, 0x3e, 0x43, 0x75, 0x43, 0x3e, 0x1c, // A
    0x1c, 0x3e, 0x41, 0x55, 0x6b, 0x3e, 0x1c, // B
};

__attribute__((noinline)) void render_yesno_prompt(
    const char AVM_PROGMEM* prompt_text, const Item* item)
{
    restore_play_render();
    status(prompt_text);
    if(item) status(*item, '?');
    uint8_t buttons_y = static_cast<uint8_t>(status_baseline() + 3);
    if(buttons_y > 56) buttons_y = 56;
    avm_draw_sprite_overwrite(72, buttons_y, yesno_buttons, 0);
    avm_draw_sprite_overwrite(104, buttons_y, yesno_buttons, 1);
    avm_draw_text_P(83, buttons_y + 6, F("Yes"));
    avm_draw_text_P(115, buttons_y + 6, F("No"));
    avm_display(false);
}
} // namespace rogue
