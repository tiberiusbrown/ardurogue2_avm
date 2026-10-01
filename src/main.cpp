#include <avm.h>
#include "game.hpp"
#include <string.h>

namespace rogue {
Game game __attribute__((section(".saved"))) = {};
}

using namespace rogue;

namespace {

enum Mode : uint8_t {
    TITLE, PLAY, MENU, INVENTORY_MENU, THROW_DIRECTION, FULL_MAP, END
};

struct Ui {
    uint8_t mode, selection, previous_buttons, held_direction;
    uint16_t next_repeat_ms;
    bool has_save, dirty, repeat_suppressed, throwing;
};
static Ui ui = {};
static uint8_t status_x = 67, status_y = 28;

void status_clear()
{
    avm_draw_filled_rect_black(65, 23, 63, 41);
    status_x = 67;
    status_y = 28;
}

void status_next_line()
{
    status_x = 67;
    status_y = static_cast<uint8_t>(status_y + 7);
    if(status_y <= 56)
        return;
    int16_t more_width = avm_draw_text_P(128, 0, F("[more]")).x - 128;
    avm_draw_text_P(static_cast<int16_t>(128 - more_width), 63, F("[more]"));
    avm_display(false);
    // A that initiated this turn must be released before it can advance a page.
    while(avm_buttons() & AVM_BUTTON_A)
        avm_idle();
    while(!(avm_buttons() & AVM_BUTTON_A))
        avm_idle();
    ui.previous_buttons = avm_buttons();
    ui.held_direction = 0;
    status_clear();
}

void status_words_P(const char AVM_PROGMEM* words)
{
    char word[32];
    uint8_t length = 0;
    for(;;) {
        char c = *words++;
        if(c != ' ' && c != 0 && length < sizeof(word) - 1) {
            word[length++] = c;
            continue;
        }
        if(length) {
            word[length] = 0;
            rogue::status_word(word);
            length = 0;
        }
        if(!c)
            return;
        if(c != ' ')
            word[length++] = c;
    }
}

void status_entity(uint8_t type)
{
    switch(type) {
    case BAT: status_words_P(F("bat")); break;
    case SNAKE: status_words_P(F("snake")); break;
    case RATTLESNAKE: status_words_P(F("rattlesnake")); break;
    case ZOMBIE: status_words_P(F("zombie")); break;
    case GOBLIN: status_words_P(F("goblin")); break;
    case PHANTOM: status_words_P(F("phantom")); break;
    case ORC: status_words_P(F("orc")); break;
    case TARANTULA: status_words_P(F("tarantula")); break;
    case HOBGOBLIN: status_words_P(F("hobgoblin")); break;
    case MIMIC: status_words_P(F("mimic")); break;
    case INCUBUS: status_words_P(F("incubus")); break;
    case TROLL: status_words_P(F("troll")); break;
    case GRIFFIN: status_words_P(F("griffin")); break;
    case DRAGON: status_words_P(F("dragon")); break;
    case ANGEL: status_words_P(F("fallen angel")); break;
    case LORD: status_words_P(F("Lord of Darkness")); break;
    default: status_words_P(F("foe")); break;
    }
}

static const char PROGMEM* const PROGMEM potion_effect_names[] = {
    F("healing"), F("confusion"), F("poison"), F("harming"),
    F("strength"), F("dexterity"), F("paralysis"), F("slowing"),
    F("experience"), F("invisibility")
};
static const char PROGMEM* const PROGMEM potion_color_names[] = {
    F("red"), F("clear"), F("orange"), F("green"), F("blue"),
    F("white"), F("yellow"), F("violet"), F("black"), F("pink")
};
static const char PROGMEM* const PROGMEM ring_names[] = {
    F("see invisible"), F("strength"), F("dexterity"), F("protection"),
    F("fire immunity"), F("attack"), F("sustenance"), F("invisibility")
};
static const char PROGMEM* const PROGMEM amulet_names[] = {
    F("speed"), F("clarity"), F("conservation"), F("regeneration"),
    F("the vampire"), F("ironblood"), F("vitality"), F("wisdom")
};

const char PROGMEM* potion_display_name(uint8_t type)
{
    return potion_identified(type)
        ? potion_effect_names[type - HEALING]
        : potion_color_names[potion_color(type)];
}

const char PROGMEM* ring_name(uint8_t type)
{
    return is_ring(type) ? ring_names[type - RING_SEE_INVISIBLE] : F("unknown");
}

const char PROGMEM* amulet_name(uint8_t type)
{
    return is_amulet(type) ? amulet_names[type - AMULET_SPEED] : F("unknown");
}

void status_item(Item item)
{
    if(is_potion(item.type)) {
        uint8_t quantity = item_value(item);
        if(quantity > 1)
            rogue::status_number(quantity);
        else
            status_words_P(F("a"));
        if(potion_identified(item.type)) {
            status_words_P(quantity > 1 ? F("potions of") : F("potion of"));
            status_words_P(potion_display_name(item.type));
        } else {
            status_words_P(potion_display_name(item.type));
            status_words_P(quantity > 1 ? F("potions") : F("potion"));
        }
        return;
    }
    if(is_ring(item.type)) {
        status_words_P(F("a ring of"));
        status_words_P(ring_name(item.type));
        return;
    }
    if(is_amulet(item.type)) {
        status_words_P(F("an amulet of"));
        status_words_P(amulet_name(item.type));
        return;
    }
    switch(item.type) {
    case FOOD:
        if(item_value(item) > 1) {
            rogue::status_number(item_value(item));
            status_words_P(F("food rations"));
        } else {
            status_words_P(F("some food"));
        }
        break;
    case SWORD: status_words_P(F("a sword")); break;
    case ARMOR: status_words_P(F("armor")); break;
    case YENDOR_AMULET: status_words_P(F("the amulet")); break;
    default: status_words_P(F("item")); break;
    }
    if((item.type == SWORD || item.type == ARMOR) && item_value(item)) {
        char modifier[5] = {'+'};
        uint8_t length = 1;
        if(item_value(item) >= 100)
            modifier[length++] = static_cast<char>('0' + item_value(item) / 100);
        if(item_value(item) >= 10)
            modifier[length++] = static_cast<char>('0' + (item_value(item) / 10) % 10);
        modifier[length++] = static_cast<char>('0' + item_value(item) % 10);
        modifier[length] = 0;
        rogue::status_word(modifier);
    }
}

// ArduRogue's four-column sprites (draw.cpp), with each column in one nibble.
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
    0x0660, 0x0660, 0x0660, 0x0660,
    0x0660, 0x0660, 0x0660, 0x0660, // ring variants
    0x0606, 0x0606, 0x0606, 0x0606,
    0x0606, 0x0606, 0x0606, 0x0606, // amulet variants
};
static constexpr uint16_t PLAYER_ICON = 0x6ff6;
static constexpr uint16_t DOWN_STAIRS_ICON = 0xfec8;
static constexpr uint16_t UP_STAIRS_ICON = 0x8cef;
static constexpr uint16_t CLOSED_DOOR_ICON = 0xefbe;
static constexpr uint16_t OPEN_DOOR_ICON = 0xe11e;

void pixel(int16_t x, int16_t y)
{
    if(x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] |= static_cast<uint8_t>(1u << (y & 7));
}

// All map symbols fit on screen. Write a whole vertical nibble at once.
void column(uint8_t x, uint8_t y, uint8_t bits)
{
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    uint8_t shift = y & 7;
    __avm_framebuffer[offset] |= static_cast<uint8_t>(bits << shift);
    if(shift > 4 && y < 60)
        __avm_framebuffer[offset + 128] |= static_cast<uint8_t>(bits >> (8 - shift));
}

void icon(uint16_t shape, uint8_t x, uint8_t y)
{
    avm_draw_filled_rect_black(x, y, 4, 4);
    for(uint8_t col = 0; col < 4; ++col) {
        column(static_cast<uint8_t>(x + col), y,
               static_cast<uint8_t>(shape >> 12));
        shape = static_cast<uint16_t>(shape << 4);
    }
}

bool screen_tile(uint8_t x, uint8_t y, uint8_t& sx, uint8_t& sy)
{
    int16_t dx = static_cast<int16_t>(x) - game.px + 6;
    int16_t dy = static_cast<int16_t>(y) - game.py + 6;
    if(dx < 0 || dx >= 13 || dy < 0 || dy >= 13)
        return false;
    sx = static_cast<uint8_t>(dx);
    sy = static_cast<uint8_t>(dy);
    return true;
}

bool in_sight(uint8_t x, uint8_t y, const uint16_t sight[13],
              uint8_t& sx, uint8_t& sy)
{
    return screen_tile(x, y, sx, sy) && (sight[sy] & (1u << sx));
}

void render_play()
{
    avm_draw_filled_rect_black(0, 0, 65, 64);
    avm_draw_filled_rect_black(65, 0, 63, 23);
    uint16_t sight[13] = {};
    uint16_t walls[13];
    uint16_t opaque[13];
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
    for(uint8_t sy = 0; sy < 13; ++sy)
        opaque[sy] = walls[sy];
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(!door_open(i) && screen_tile(door.x, door.y, sx, sy))
            opaque[sy] |= static_cast<uint16_t>(1u << sx);
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
                ray_visible(sx, sy, opaque);
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
        uint16_t floor_sight = sight[sy] & ~opaque[sy];
        uint16_t adjacent = static_cast<uint16_t>((floor_sight << 1) |
                                                   (floor_sight >> 1));
        if(sy > 0)
            adjacent |= sight[sy - 1] & ~opaque[sy - 1];
        if(sy < 12)
            adjacent |= sight[sy + 1] & ~opaque[sy + 1];
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

void render_title()
{
    avm_draw_text_P(22, 14, F("ARDUROGUE 2"));
    avm_draw_text_P(14, 30, ui.has_save ? F("A: CONTINUE") : F("A: NEW GAME"));
    if(ui.has_save)
        avm_draw_text_P(14, 40, F("B: NEW GAME"));
    avm_draw_textf_P(14, 56, F("BEST %u"), game.best_score);
}

void render_menu()
{
    static const char AVM_PROGMEM* const AVM_PROGMEM names[] = {
        F("WAIT"), F("INVENTORY"), F("THROW POTION"), F("FULL MAP"),
        F("SAVE & EXIT"), F("ABANDON")
    };
    avm_draw_text_P(10, 8, F("ACTION MENU"));
    for(uint8_t i = 0; i < 6; ++i) {
        if(i == ui.selection)
            avm_draw_text_P(4, static_cast<int16_t>(17 + 8 * i), F(">"));
        avm_draw_text_P(12, static_cast<int16_t>(17 + 8 * i), names[i]);
    }
}

void render_inventory()
{
    avm_draw_text_P(2, 7, ui.throwing ? F("A PICK POTION B BACK") :
                                   F("A USE > DROP B BACK"));
    uint8_t start = ui.selection < 8 ? 0 : 8;
    for(uint8_t row = 0; row < 8; ++row) {
        uint8_t i = start + row;
        int16_t y = static_cast<int16_t>(14 + row * 7);
        if(i == ui.selection)
            avm_draw_text_P(1, y, F(">"));
        const Item& item = game.inventory[i];
        if(!item.type)
            continue;
        if(is_potion(item.type)) {
            avm_draw_text_P(10, y, F("P:"));
            avm_draw_text_P(21, y, potion_display_name(item.type));
            avm_draw_textf_P(95, y, F("x%u"), item_value(item));
            continue;
        }
        switch(item.type) {
        case FOOD: avm_draw_textf_P(10, y, F("FOOD x%u"), item_value(item)); break;
        case SWORD: avm_draw_textf_P(10, y, F("SWORD +%u"), item_value(item)); break;
        case ARMOR: avm_draw_textf_P(10, y, F("ARMOR +%u"), item_value(item)); break;
        default:
            if(is_ring(item.type)) {
                avm_draw_text_P(10, y, F("RING OF"));
                avm_draw_text_P(48, y, ring_name(item.type));
            } else if(is_amulet(item.type)) {
                avm_draw_text_P(10, y, F("AMULET OF"));
                avm_draw_text_P(61, y, amulet_name(item.type));
            }
            break;
        }
        if(game.weapon_slot == i || game.armor_slot == i ||
           game.amulet_slot == i || game.ring_slots[0] == i ||
           game.ring_slots[1] == i)
            avm_draw_text_P(116, y, F("*"));
        if(item_is_identified(item) && item_is_cursed(item))
            avm_draw_text_P(123, y, F("!"));
    }
}

void render_throw_direction()
{
    avm_draw_text_P(8, 12, F("THROW POTION"));
    avm_draw_text_P(8, 27,
        potion_display_name(game.inventory[ui.selection].type));
    avm_draw_text_P(8, 43, F("D-PAD: DIRECTION"));
    avm_draw_text_P(8, 56, F("B: BACK"));
}

void render_full_map()
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

void render_end()
{
    avm_draw_text_P(16, 15, session.result == 1 ? F("YOU ESCAPED!") :
        session.result == 2 ? F("RETURNED EMPTY") : F("YOU DIED"));
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
    case INVENTORY_MENU: render_inventory(); break;
    case THROW_DIRECTION: render_throw_direction(); break;
    case FULL_MAP: render_full_map(); break;
    case END: render_end(); break;
    }
    avm_display(false);
    ui.dirty = false;
}

uint8_t directional_press(uint8_t buttons, uint8_t edges)
{
    uint8_t direction = buttons & static_cast<uint8_t>(
        AVM_BUTTON_U | AVM_BUTTON_D | AVM_BUTTON_L | AVM_BUTTON_R);
    if(!direction) {
        ui.held_direction = 0;
        ui.repeat_suppressed = false;
        return 0;
    }
    uint16_t now = avm_millis();
    if(direction != ui.held_direction) {
        ui.held_direction = direction;
        ui.next_repeat_ms = static_cast<uint16_t>(now + 300);
        if(!(edges & direction))
            return 0;
        ui.repeat_suppressed = false;
    } else {
        if(ui.repeat_suppressed ||
           static_cast<int16_t>(now - ui.next_repeat_ms) < 0)
            return 0;
        ui.next_repeat_ms = static_cast<uint16_t>(now + 100);
    }
    if(direction & AVM_BUTTON_U) return AVM_BUTTON_U;
    if(direction & AVM_BUTTON_D) return AVM_BUTTON_D;
    if(direction & AVM_BUTTON_L) return AVM_BUTTON_L;
    if(direction & AVM_BUTTON_R) return AVM_BUTTON_R;
    return 0;
}

void begin_new_game()
{
    if(ui.has_save) {
        game.valid = 0;
        avm_save();
    }
    start_new(avm_generate_random_seed());
    ui.has_save = false;
    ui.mode = PLAY;
    status_clear();
    status(F("Welcome to the dungeon."));
}

uint8_t next_potion_slot(int16_t start, int8_t step)
{
    for(int16_t slot = start + step;
        slot >= 0 && slot < INVENTORY; slot += step)
        if(is_potion(game.inventory[slot].type))
            return static_cast<uint8_t>(slot);
    return NONE;
}

void handle_input(uint8_t buttons)
{
    uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
    ui.previous_buttons = buttons;
    uint8_t direction = directional_press(buttons, edges);
    if(ui.mode == TITLE) {
        if(edges & AVM_BUTTON_A) {
            if(ui.has_save) {
                game.valid = 0;
                avm_save(); // One-use save: a crash cannot reload the run.
                game.valid = 1;
                ui.mode = PLAY;
                ui.has_save = false;
                status_clear();
                status(F("Welcome back to the dungeon."));
            } else {
                begin_new_game();
            }
            ui.dirty = true;
        } else if(ui.has_save && (edges & AVM_BUTTON_B)) {
            begin_new_game();
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == END) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = TITLE;
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == FULL_MAP) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == THROW_DIRECTION) {
        if(edges & AVM_BUTTON_B) {
            ui.mode = INVENTORY_MENU;
            ui.dirty = true;
        } else if(direction) {
            int8_t dx = direction == AVM_BUTTON_L ? -1 :
                        direction == AVM_BUTTON_R ? 1 : 0;
            int8_t dy = direction == AVM_BUTTON_U ? -1 :
                        direction == AVM_BUTTON_D ? 1 : 0;
            status_clear();
            if(throw_potion(ui.selection, dx, dy)) {
                ui.throwing = false;
                ui.mode = PLAY;
            }
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == MENU) {
        if(direction == AVM_BUTTON_U && ui.selection) {
            --ui.selection;
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_D && ui.selection < 5) {
            ++ui.selection;
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_B) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_A) {
            switch(ui.selection) {
            case 0:
                ui.mode = PLAY;
                status_clear();
                status(F("You wait."));
                end_turn();
                break;
            case 1:
                ui.throwing = false;
                ui.mode = INVENTORY_MENU;
                ui.selection = 0;
                break;
            case 2:
                ui.throwing = true;
                ui.selection = next_potion_slot(-1, 1);
                if(ui.selection != NONE)
                    ui.mode = INVENTORY_MENU;
                else {
                    ui.throwing = false;
                    ui.mode = PLAY;
                    status_clear();
                    status(F("You have no potions."));
                }
                break;
            case 3: ui.mode = FULL_MAP; break;
            case 4:
                game.valid = 1;
                avm_save();
                ui.has_save = true;
                ui.mode = TITLE;
                break;
            case 5: finish(2); break;
            }
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == INVENTORY_MENU) {
        if(direction == AVM_BUTTON_U || direction == AVM_BUTTON_D) {
            int8_t step = direction == AVM_BUTTON_U ? -1 : 1;
            if(ui.throwing) {
                uint8_t next = next_potion_slot(ui.selection, step);
                if(next != NONE) ui.selection = next;
            } else if((step < 0 && ui.selection > 0) ||
                      (step > 0 && ui.selection < INVENTORY - 1)) {
                ui.selection = static_cast<uint8_t>(ui.selection + step);
            }
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_R && !ui.throwing) {
            status_clear();
            if(drop_inventory(ui.selection)) {
                ui.mode = PLAY;
            }
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_B) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_A) {
            status_clear();
            if(ui.throwing) {
                if(is_potion(game.inventory[ui.selection].type))
                    ui.mode = THROW_DIRECTION;
            } else if(use_inventory(ui.selection)) {
                ui.mode = PLAY;
            }
            ui.dirty = true;
        }
        return;
    }
    if(edges & AVM_BUTTON_B) {
        ui.mode = MENU;
        ui.selection = 0;
    } else if(direction) {
        status_clear();
        int8_t dx = direction == AVM_BUTTON_L ? -1 :
                    direction == AVM_BUTTON_R ? 1 : 0;
        int8_t dy = direction == AVM_BUTTON_U ? -1 :
                    direction == AVM_BUTTON_D ? 1 : 0;
        move_player(dx, dy);
    } else if(edges & AVM_BUTTON_A) {
        status_clear();
        action();
    } else {
        return;
    }
    ui.dirty = true;
}

} // namespace

void rogue::status_word(const char* word)
{
    ui.repeat_suppressed = true;
    const int16_t space_width = avm_draw_text(128, 0, " ").x - 128;
    if((*word == '.' || *word == '!' || *word == ',' || *word == ':') &&
       status_x > 67)
        status_x = static_cast<uint8_t>(status_x - space_width);
    while(*word) {
        char part[32];
        uint8_t count = 0;
        int16_t width = 0;
        while(word[count] && count < sizeof(part) - 1) {
            part[count] = word[count];
            part[count + 1] = 0;
            int16_t candidate = avm_draw_text(128, 0, part).x - 128;
            if(count && candidate > 60)
                break;
            width = candidate;
            ++count;
        }
        if(status_x != 67 && status_x + width > 128)
            status_next_line();
        part[count] = 0;
        status_x = static_cast<uint8_t>(avm_draw_text(status_x, status_y, part).x);
        word += count;
        if(*word)
            status_next_line();
    }
    status_x = static_cast<uint8_t>(status_x + space_width);
}

void rogue::status(const char PROGMEM* words)
{
    status_words_P(words);
}

void rogue::status(Item item)
{
    status_item(item);
}

void rogue::status(MonsterType monster)
{
    status_entity(monster);
}

void rogue::status_number(uint8_t value)
{
    char digits[4];
    uint8_t length = 0;
    if(value >= 100)
        digits[length++] = static_cast<char>('0' + value / 100);
    if(value >= 10)
        digits[length++] = static_cast<char>('0' + (value / 10) % 10);
    digits[length++] = static_cast<char>('0' + value % 10);
    digits[length] = 0;
    status_word(digits);
}

extern "C" int main()
{
    avm_set_text_font(AVM_FONT_BR5D);
    if(avm_save_exists() && avm_load() &&
       game.magic == 0xa7 && game.version == SAVE_VERSION && game.valid)
        ui.has_save = true;
    else
        memset(&game, 0, sizeof(game));
    ui.mode = TITLE;
    session.repeat_slot = NONE;
    ui.dirty = true;
    render();

    for(;;) {
        // Sleep until an interrupt, then process button edges or a held repeat.
        avm_idle();
        uint8_t buttons = avm_buttons();
        if(buttons != ui.previous_buttons ||
           (ui.held_direction && !ui.repeat_suppressed &&
            static_cast<int16_t>(avm_millis() -
               ui.next_repeat_ms) >= 0))
            handle_input(buttons);
        if(session.ended) {
            avm_save();
            ui.has_save = false;
            ui.mode = END;
            session.ended = false;
            ui.dirty = true;
        }
        if(ui.dirty)
            render();
    }
}
