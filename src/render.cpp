#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "status.hpp"
#include "world.hpp"

namespace rogue {

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
static const uint16_t PROGMEM item_icons[] = {
    0x0000, // none
    0x9429, // food
    0x0bb0, // potions
    0x0bb0, 0x0bb0, 0x0bb0, 0x0bb0, 0x0bb0,
    0x0bb0, 0x0bb0, 0x0bb0, 0x0bb0,
    0x04f4, // sword
    0x0f90, // armor
    0x0606, // Yendor amulet
    0x0aaa, 0x0aaa, 0x0aaa, 0x0aaa,
    0x0aaa, 0x0aaa, 0x0aaa, 0x0aaa, // ring variants
    0x0606, 0x0606, 0x0606, 0x0606,
    0x0606, 0x0606, 0x0606, 0x0606, // amulet variants
    0x01b3, 0x01b3, 0x01b3, 0x01b3, 0x01b3,
    0x01b3, 0x01b3, 0x01b3, 0x01b3, // scroll variants
};
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

static void clear_pixel(uint8_t x, uint8_t y)
{
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] &= static_cast<uint8_t>(~(1u << (y & 7)));
}

// All map symbols fit on screen. Write a whole vertical nibble at once.
static void column(uint8_t x, uint8_t y, uint8_t bits)
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

static bool screen_tile(uint8_t x, uint8_t y, uint8_t& sx, uint8_t& sy)
{
    int16_t dx = static_cast<int16_t>(x) - game.px + 6;
    int16_t dy = static_cast<int16_t>(y) - game.py + 6;
    if(dx < 0 || dx >= 13 || dy < 0 || dy >= 13)
        return false;
    sx = static_cast<uint8_t>(dx);
    sy = static_cast<uint8_t>(dy);
    return true;
}

static bool in_sight(uint8_t x, uint8_t y, const uint16_t sight[13],
              uint8_t& sx, uint8_t& sy)
{
    return screen_tile(x, y, sx, sy) && (sight[sy] & (1u << sx));
}

void render_play()
{
    avm_draw_filled_rect_black(0, 0, 65, 64);
    avm_draw_filled_rect_black(65, 0, 63, 23);
    uint16_t sight[13] = {};
    // Reuse this array for ray blockers, then restore door tiles before drawing.
    // Keeping a third 26-byte row array here crowds the nested modal stack.
    uint16_t walls[13];
    const int16_t left = static_cast<int16_t>(game.px) - 6;
    const int16_t top = static_cast<int16_t>(game.py) - 6;
    const uint8_t first_sx = left < 0 ? static_cast<uint8_t>(-left) : 0;
    const uint8_t end_sx = left + 13 > MAP_W
        ? static_cast<uint8_t>(MAP_W - left) : 13;
    const uint8_t first_sy = top < 0 ? static_cast<uint8_t>(-top) : 0;
    const uint8_t end_sy = top + 13 > MAP_H
        ? static_cast<uint8_t>(MAP_H - top) : 13;
    const uint16_t valid_x = static_cast<uint16_t>(
        ((1u << end_sx) - 1) & ~((1u << first_sx) - 1));
    for(uint8_t sy = 0; sy < 13; ++sy)
        walls[sy] = 0x1fff;
    for(uint8_t sy = first_sy; sy < end_sy; ++sy) {
        walls[sy] = static_cast<uint16_t>(0x1fff & ~valid_x);
        uint16_t row = static_cast<uint16_t>((top + sy) * (MAP_W / 8));
        uint8_t tx = static_cast<uint8_t>(left + first_sx);
        for(uint8_t sx = first_sx; sx < end_sx; ++sx, ++tx) {
            if(game.walls[row + (tx >> 3)] & (1u << (tx & 7)))
                walls[sy] |= static_cast<uint16_t>(1u << sx);
        }
    }
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(!door_open(i) && screen_tile(door.x, door.y, sx, sy))
            walls[sy] |= static_cast<uint16_t>(1u << sx);
    }
    const Room* player_room = nullptr;
    for(const Room& room : game.rooms)
        if(game.px >= room.x && game.px < room.x + room.w &&
           game.py >= room.y && game.py < room.y + room.h) {
            player_room = &room;
            break;
        }
    for(uint8_t sy = first_sy; sy < end_sy; ++sy) {
        uint8_t ty = static_cast<uint8_t>(top + sy);
        uint8_t tx = static_cast<uint8_t>(left + first_sx);
        for(uint8_t sx = first_sx; sx < end_sx; ++sx, ++tx) {
            if(!in_light_radius(static_cast<int16_t>(sx) - LIGHT_RADIUS,
                                static_cast<int16_t>(sy) - LIGHT_RADIUS))
                continue;
            bool visible = (player_room &&
                tx >= player_room->x && tx < player_room->x + player_room->w &&
                ty >= player_room->y && ty < player_room->y + player_room->h) ||
                ray_visible(sx, sy, walls);
            if(visible) {
                sight[sy] |= static_cast<uint16_t>(1u << sx);
                explore(tx, ty);
            }
        }
    }
    // A ray to the center of a corridor wall can cross an earlier wall.
    // Reveal walls touching visible, non-opaque floor within the circular
    // light radius, without extending visibility through closed doors.
    for(uint8_t sy = first_sy; sy < end_sy; ++sy) {
        uint8_t ty = static_cast<uint8_t>(top + sy);
        int16_t dy = static_cast<int16_t>(sy) - LIGHT_RADIUS;
        uint16_t floor_sight = sight[sy] & ~walls[sy];
        uint16_t adjacent = static_cast<uint16_t>((floor_sight << 1) |
                                                   (floor_sight >> 1));
        if(sy > 0)
            adjacent |= sight[sy - 1] & ~walls[sy - 1];
        if(sy < 12)
            adjacent |= sight[sy + 1] & ~walls[sy + 1];
        uint16_t nearby_walls = adjacent & walls[sy];
        for(uint8_t sx = first_sx; sx < end_sx; ++sx) {
            uint16_t bit = static_cast<uint16_t>(1u << sx);
            if(!(nearby_walls & bit))
                continue;
            int16_t dx = static_cast<int16_t>(sx) - LIGHT_RADIUS;
            if(!in_light_radius(dx, dy))
                continue;
            sight[sy] |= bit;
            explore(static_cast<uint8_t>(left + sx), ty);
        }
    }
    // Closed doors blocked the rays above, but their tiles are floor when drawn.
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(!door_open(i) && screen_tile(door.x, door.y, sx, sy))
            walls[sy] &= static_cast<uint16_t>(~(1u << sx));
    }
    // Finish exploration before drawing: wall joins inspect the tile to the
    // right and below, which may be later in screen traversal order.
    for(uint8_t sy = first_sy; sy < end_sy; ++sy) {
        uint8_t ty = static_cast<uint8_t>(top + sy);
        uint8_t py = static_cast<uint8_t>(sy * 5);
        uint8_t tx = static_cast<uint8_t>(left + first_sx);
        for(uint8_t sx = first_sx; sx < end_sx; ++sx, ++tx) {
            bool visible = (sight[sy] & (1u << sx)) != 0;
            if(!visible && !explored(tx, ty))
                continue;
            uint8_t px = static_cast<uint8_t>(sx * 5);
            if(walls[sy] & (1u << sx)) {
                if(!wall_exposed(tx, ty))
                    continue;
                for(uint8_t col = 0; col < 4; ++col)
                    column(static_cast<uint8_t>(px + col), py, 0x0f);
                if(sx < 12 && tx + 1 < MAP_W &&
                   (walls[sy] & (1u << (sx + 1))) &&
                   wall_exposed(tx + 1, ty) && explored(tx + 1, ty))
                    column(static_cast<uint8_t>(px + 4), py, 0x0f);
                if(sy < 12 && ty + 1 < MAP_H &&
                   (walls[sy + 1] & (1u << sx)) &&
                   wall_exposed(tx, ty + 1) && explored(tx, ty + 1))
                    for(uint8_t col = 0; col < 4; ++col)
                        column(static_cast<uint8_t>(px + col),
                               static_cast<uint8_t>(py + 4), 1);
            } else if(visible) {
                pixel(px + 2, py + 2);
            }
        }
    }
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(screen_tile(door.x, door.y, sx, sy) &&
           explored(door.x, door.y))
            icon(door_open(i) ? OPEN_DOOR_ICON : CLOSED_DOOR_ICON,
                 static_cast<uint8_t>(sx * 5),
                 static_cast<uint8_t>(sy * 5));
    }
    uint8_t sx, sy;
    if(screen_tile(game.up_x, game.up_y, sx, sy) &&
       explored(game.up_x, game.up_y))
        icon(UP_STAIRS_ICON, static_cast<uint8_t>(sx * 5),
             static_cast<uint8_t>(sy * 5));
    if(game.floor < FLOORS - 1 &&
       screen_tile(game.down_x, game.down_y, sx, sy) &&
       explored(game.down_x, game.down_y))
        icon(DOWN_STAIRS_ICON, static_cast<uint8_t>(sx * 5),
             static_cast<uint8_t>(sy * 5));
    for(const GroundItem& ground : game.ground)
        if(ground.item.type && in_sight(ground.x, ground.y, sight, sx, sy))
            icon(item_icons[ground.item.type], static_cast<uint8_t>(sx * 5),
                 static_cast<uint8_t>(sy * 5));
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Monster& monster = game.monsters[i];
        if(player_can_see_monster(i) &&
           in_sight(monster.x, monster.y, sight, sx, sy))
            icon(monster.type == MIMIC && !(monster.state & MON_AGGRO)
                     ? item_icons[monster.state >> 1]
                     : monster_icons[monster.type],
                 static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    }
    icon(PLAYER_ICON, 30, 30);
    for(uint8_t y = 0; y < 64; ++y)
        pixel(64, y);
    avm_draw_textf_P(67, 7, F("D%u LV%u"), game.floor + 1, game.level);
    avm_draw_textf_P(67, 15, F("HP%u/%u"), game.hp, player_max_hp());
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
        F("WAIT"), F("USE ITEM"), F("DROP ITEM"), F("THROW POTION"),
        F("FULL MAP"), F("SAVE & EXIT"), F("ABANDON")
    };
    avm_draw_text_P(10, 8, F("ACTION MENU"));
    for(uint8_t i = 0; i < 7; ++i) {
        if(i == ui.selection)
            avm_draw_text_P(4, static_cast<int16_t>(17 + 7 * i), F(">"));
        avm_draw_text_P(12, static_cast<int16_t>(17 + 7 * i), names[i]);
    }
}

void render_inventory(const char AVM_PROGMEM* prompt,
                      const InventoryView& view, uint8_t selection, uint8_t top)
{
    avm_draw_filled_rect_black(0, 0, 128, 64);
    avm_draw_text_P(1, 7, prompt);
    avm_draw_filled_rect_white(1, 9, 127, 1);
    if(!view.count) {
        avm_draw_text_P(8, 18, F("Empty"));
        return;
    }
    for(uint8_t row = 0; row < INVENTORY_VISIBLE_ROWS; ++row) {
        uint8_t index = static_cast<uint8_t>(top + row);
        if(index >= view.count) break;
        int16_t y = static_cast<int16_t>(18 + row * 7);
        uint8_t entry = view.rows[index];
        if(entry >= INVENTORY) {
            switch(entry - INVENTORY) {
            case WEAPONS: avm_draw_text_P(1, y, F("Weapons")); break;
            case ARMORS: avm_draw_text_P(1, y, F("Armor")); break;
            case RINGS: avm_draw_text_P(1, y, F("Rings")); break;
            case AMULETS: avm_draw_text_P(1, y, F("Amulets")); break;
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
        char label[ITEM_TEXT_CAPACITY];
        format_item(item, label);
        avm_draw_text(8, y, label);
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
    avm_draw_text_P(8, 12, F("THROW POTION"));
    char label[ITEM_TEXT_CAPACITY];
    format_item(game.inventory[ui.selection], label);
    avm_draw_text(8, 27, label);
    avm_draw_text_P(8, 43, F("D-PAD: DIRECTION"));
    avm_draw_text_P(8, 56, F("B: BACK"));
}

__attribute__((noinline)) static void render_full_map()
{
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) {
            if(!explored(x, y))
                continue;
            if(wall_exposed(x, y)) {
                pixel(x * 2, y * 2);
                pixel(x * 2 + 1, y * 2);
                pixel(x * 2, y * 2 + 1);
                pixel(x * 2 + 1, y * 2 + 1);
            } else if(x + 6 >= game.px && x <= game.px + 6 &&
                      y + 6 >= game.py && y <= game.py + 6 &&
                      can_see(x, y)) {
                pixel(x * 2, y * 2);
            }
        }
    pixel(game.px * 2, game.py * 2);
    pixel(game.px * 2 + 1, game.py * 2 + 1);
}

__attribute__((noinline)) static void render_end()
{
    avm_draw_text_P(16, 15, session.result == ESCAPED ? F("YOU ESCAPED!") :
        session.result == RETURNED_EMPTY ? F("RETURNED EMPTY") :
        session.result == ABANDONED ? F("ABANDONED") : F("YOU DIED"));
    avm_draw_textf_P(16, 31, F("SCORE %u"), game.score);
    avm_draw_textf_P(16, 41, F("BEST %u"), game.best_score);
    avm_draw_text_P(16, 57, F("A: TITLE"));
}

void render()
{
    if(ui.mode != PLAY)
        avm_draw_filled_rect_black(0, 0, 128, 64);
    switch(ui.mode) {
    case TITLE: render_title(); break;
    case PLAY: render_play(); break;
    case MENU: render_menu(); break;
    case THROW_DIRECTION: render_throw_direction(); break;
    case FULL_MAP: render_full_map(); break;
    case END: render_end(); break;
    }
    avm_display(false);
    ui.dirty = false;
}

static void yesno_button(uint8_t x, uint8_t y, bool affirmative)
{
    // ArduRogue's seven-pixel circular button sprite and three-pixel letter.
    static constexpr uint8_t circle[7] = {
        0x1c, 0x3e, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c
    };
    static constexpr uint8_t letter_a[3] = {0x1e, 0x05, 0x1e};
    static constexpr uint8_t letter_b[3] = {0x1f, 0x15, 0x0a};
    for(uint8_t col = 0; col < 7; ++col)
        for(uint8_t row = 0; row < 7; ++row)
            if(circle[col] & (1u << row)) pixel(x + col, y + row);
    const uint8_t* letter = affirmative ? letter_a : letter_b;
    for(uint8_t col = 0; col < 3; ++col)
        for(uint8_t row = 0; row < 5; ++row)
            if(letter[col] & (1u << row))
                clear_pixel(static_cast<uint8_t>(x + 2 + col),
                            static_cast<uint8_t>(y + 1 + row));
}

__attribute__((noinline)) void render_yesno_prompt(
    const char AVM_PROGMEM* prompt_text, const Item* item)
{
    status(prompt_text);
    if(item) status(*item, '?');
    uint8_t buttons_y = static_cast<uint8_t>(status_baseline() + 3);
    if(buttons_y > 56) buttons_y = 56;
    yesno_button(72, buttons_y, true);
    yesno_button(104, buttons_y, false);
    avm_draw_text_P(83, buttons_y + 6, F("Yes"));
    avm_draw_text_P(115, buttons_y + 6, F("No"));
    avm_display(false);
}
} // namespace rogue
