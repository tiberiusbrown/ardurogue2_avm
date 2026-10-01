#include <avm.h>
#include "game.hpp"
#include "ui.hpp"
#include <stdio.h>
#include <string.h>

namespace rogue {
Game game __attribute__((section(".saved"))) = {};
}

using namespace rogue;

namespace {

enum Mode : uint8_t {
    TITLE, PLAY, MENU, THROW_DIRECTION, FULL_MAP, END
};

struct Ui {
    uint8_t mode, selection, previous_buttons, held_direction;
    uint16_t next_repeat_ms;
    bool has_save, dirty, repeat_suppressed;
};
static Ui ui = {};
static uint8_t status_x = 67, status_y = 28;

int16_t text_width(const char* words)
{
    return avm_draw_text(128, 0, words).x - 128;
}

int16_t text_width(const char AVM_PROGMEM* words)
{
    return avm_draw_text_P(128, 0, words).x - 128;
}

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
    int16_t more_width = text_width(F("[more]"));
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

void status_formatted_number(uint8_t value, bool bonus, char punctuation)
{
    int16_t width = bonus
        ? avm_draw_textf_P(128, 0, F("+%u"), value).x - 128
        : avm_draw_textf_P(128, 0, F("%u"), value).x - 128;
    char mark[2] = {punctuation, 0};
    if(punctuation) width += text_width(mark);
    ui.repeat_suppressed = true;
    if(status_x != 67 && status_x + width > 128)
        status_next_line();
    status_x = static_cast<uint8_t>(bonus
        ? avm_draw_textf_P(status_x, status_y, F("+%u"), value).x
        : avm_draw_textf_P(status_x, status_y, F("%u"), value).x);
    if(punctuation)
        status_x = static_cast<uint8_t>(avm_draw_text(status_x, status_y,
                                                      mark).x);
    status_x = static_cast<uint8_t>(status_x + text_width(" "));
}

// Keep text scratch in its own frame on the 256-byte VM stack.

template<typename Pointer>
__attribute__((noinline)) void status_words(Pointer words, char suffix = 0)
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
            if(!c && suffix)
                rogue::status_word(word, suffix);
            else
                rogue::status_word(word);
            length = 0;
        }
        if(!c)
            return;
        if(c != ' ')
            word[length++] = c;
    }
}

__attribute__((noinline)) void status_words_P(
    const char AVM_PROGMEM* words, char suffix = 0)
{
    status_words(words, suffix);
}

const char AVM_PROGMEM* monster_name(uint8_t type)
{
    switch(type) {
    case BAT: return F("bat");
    case SNAKE: return F("snake");
    case RATTLESNAKE: return F("rattlesnake");
    case ZOMBIE: return F("zombie");
    case GOBLIN: return F("goblin");
    case PHANTOM: return F("phantom");
    case ORC: return F("orc");
    case TARANTULA: return F("tarantula");
    case HOBGOBLIN: return F("hobgoblin");
    case MIMIC: return F("mimic");
    case INCUBUS: return F("incubus");
    case TROLL: return F("troll");
    case GRIFFIN: return F("griffin");
    case DRAGON: return F("dragon");
    case ANGEL: return F("fallen angel");
    case LORD: return F("Lord of Darkness");
    default: return F("foe");
    }
}

void status_entity(uint8_t type, char punctuation = 0)
{
    status_words_P(monster_name(type), punctuation);
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
static const char PROGMEM* const PROGMEM scroll_names[] = {
    F("identify"), F("enchanting"), F("remove curse"),
    F("teleportation"), F("magic mapping"), F("fear"),
    F("torment"), F("mass confusion"), F("mass poison")
};
static const char PROGMEM* const PROGMEM scroll_descriptors[] = {
    F("faded"), F("yellowed"), F("tattered"), F("glowing"),
    F("shimmering"), F("humming"), F("dark"), F("bright"),
    F("brilliant")
};
static const char PROGMEM* const PROGMEM jewel_descriptors[] = {
    F("diamond"), F("ruby"), F("emerald"), F("topaz"),
    F("gold"), F("silver"), F("platinum"), F("iron")
};

const char PROGMEM* ring_name(uint8_t type)
{
    return is_ring(type) ? ring_names[type - RING_SEE_INVISIBLE] : F("unknown");
}

const char PROGMEM* amulet_name(uint8_t type)
{
    return is_amulet(type) ? amulet_names[type - AMULET_SPEED] : F("unknown");
}

constexpr uint8_t ITEM_TEXT_CAPACITY = 36;

struct BufferedItemText {
    char* out;
    uint8_t length = 0;

    void append(const char AVM_PROGMEM* words)
    {
        while(*words && length < ITEM_TEXT_CAPACITY - 1)
            out[length++] = *words++;
        out[length] = 0;
    }

    void word(const char AVM_PROGMEM* words)
    {
        if(length) append(F(" "));
        append(words);
    }

    void number(uint8_t value)
    {
        if(length) append(F(" "));
        snprintf_P(out + length, ITEM_TEXT_CAPACITY - length, F("%u"), value);
        length = static_cast<uint8_t>(strlen(out));
    }

    void bonus(uint8_t value)
    {
        if(length) append(F(" "));
        snprintf_P(out + length, ITEM_TEXT_CAPACITY - length,
                   F("+%u"), value);
        length = static_cast<uint8_t>(strlen(out));
    }

    void finish(char = 0) {}
};

enum ItemTextStyle : uint8_t { STATUS_ITEM, INVENTORY_ITEM, PROMPT_ITEM };

struct StatusItemText {
    const char AVM_PROGMEM* pending_word = nullptr;
    uint8_t pending_value = 0;
    uint8_t kind = 0; // Text, quantity, or bonus.

    void flush(char suffix = 0)
    {
        if(kind == 1) status(pending_word, suffix);
        else if(kind == 2) status_number(pending_value, suffix);
        else if(kind) status_formatted_number(pending_value, true, suffix);
        kind = 0;
    }

    void word(const char AVM_PROGMEM* words)
    {
        flush();
        pending_word = words;
        kind = 1;
    }

    void number(uint8_t value)
    {
        flush();
        pending_value = value;
        kind = 2;
    }

    void bonus(uint8_t value)
    {
        flush();
        pending_value = value;
        kind = 3;
    }

    void finish(char suffix = 0) { flush(suffix); }
};

template<typename Output>
void emit_item(Item item, ItemTextStyle style, Output& text)
{
    bool known = item_type_identified(item.type);
    if(is_potion(item.type) || is_scroll(item.type)) {
        uint8_t quantity = item_value(item);
        bool plural = quantity > 1 && style != PROMPT_ITEM;
        if(plural) text.number(quantity);
        else if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM)
            text.word(!known && is_potion(item.type) &&
                item_appearance(item.type) == 2 ? F("an") : F("a"));
        if(known) {
            text.word(is_scroll(item.type)
                ? (plural ? F("scrolls") : F("scroll"))
                : (plural ? F("potions") : F("potion")));
            text.word(F("of"));
            text.word(is_scroll(item.type)
                ? scroll_names[item.type - SCROLL_IDENTIFY]
                : potion_effect_names[item.type - HEALING]);
        } else {
            text.word(is_scroll(item.type)
                ? scroll_descriptors[item_appearance(item.type)]
                : potion_color_names[item_appearance(item.type)]);
            text.word(is_scroll(item.type)
                ? (plural ? F("scrolls") : F("scroll"))
                : (plural ? F("potions") : F("potion")));
        }
        return;
    }
    if(is_ring(item.type)) {
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM) {
            uint8_t descriptor = item_appearance(item.type);
            text.word(!known && (descriptor == 2 || descriptor == 7)
                ? F("an") : F("a"));
        }
        if(known) {
            text.word(F("ring"));
            text.word(F("of"));
            text.word(ring_name(item.type));
        } else {
            text.word(jewel_descriptors[item_appearance(item.type)]);
            text.word(F("ring"));
        }
        return;
    }
    if(is_amulet(item.type)) {
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM) {
            uint8_t descriptor = item_appearance(item.type);
            text.word(known || descriptor == 2 || descriptor == 7
                ? F("an") : F("a"));
        }
        if(known) {
            text.word(F("amulet"));
            text.word(F("of"));
            text.word(amulet_name(item.type));
        } else {
            text.word(jewel_descriptors[item_appearance(item.type)]);
            text.word(F("amulet"));
        }
        return;
    }
    switch(item.type) {
    case FOOD:
        if(item_value(item) > 1) {
            if(style == PROMPT_ITEM) text.word(F("the"));
            text.number(item_value(item));
            text.word(F("food rations"));
        } else {
            if(style == PROMPT_ITEM) text.word(F("the"));
            else if(style == STATUS_ITEM) text.word(F("some"));
            text.word(F("food"));
        }
        break;
    case SWORD:
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM) text.word(F("a"));
        text.word(F("sword"));
        break;
    case ARMOR:
        if(style == PROMPT_ITEM) text.word(F("the"));
        text.word(F("armor"));
        break;
    case YENDOR_AMULET:
        if(style != INVENTORY_ITEM) text.word(F("the"));
        text.word(F("amulet"));
        break;
    default:
        if(style == PROMPT_ITEM) text.word(F("the"));
        text.word(F("item"));
        break;
    }
    if((item.type == SWORD || item.type == ARMOR) &&
       item_is_identified(item) && item_value(item))
        text.bonus(item_value(item));
}

void format_item(Item item, char (&buffer)[ITEM_TEXT_CAPACITY])
{
    buffer[0] = 0;
    BufferedItemText text{buffer};
    emit_item(item, INVENTORY_ITEM, text);
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

void pixel(int16_t x, int16_t y)
{
    if(x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] |= static_cast<uint8_t>(1u << (y & 7));
}

void clear_pixel(uint8_t x, uint8_t y)
{
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] &= static_cast<uint8_t>(~(1u << (y & 7)));
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
__attribute__((noinline)) void render_title()
{
    avm_draw_text_P(22, 14, F("ARDUROGUE 2"));
    avm_draw_text_P(14, 30, ui.has_save ? F("A: CONTINUE") : F("A: NEW GAME"));
    if(ui.has_save)
        avm_draw_text_P(14, 40, F("B: NEW GAME"));
    avm_draw_textf_P(14, 56, F("BEST %u"), game.best_score);
}

__attribute__((noinline)) void render_menu()
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

__attribute__((noinline)) void render_throw_direction()
{
    avm_draw_text_P(8, 12, F("THROW POTION"));
    char label[ITEM_TEXT_CAPACITY];
    format_item(game.inventory[ui.selection], label);
    avm_draw_text(8, 27, label);
    avm_draw_text_P(8, 43, F("D-PAD: DIRECTION"));
    avm_draw_text_P(8, 56, F("B: BACK"));
}

__attribute__((noinline)) void render_full_map()
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

__attribute__((noinline)) void render_end()
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

// Modal choice: owns its rendering and button loop, returning a real slot only.
uint8_t choose_item_modal(const char AVM_PROGMEM* prompt_text,
                          ItemTypeFilter item_type_filter)
{
    InventoryView view(game, item_type_filter);
    if(!view.count) {
        render_inventory(prompt_text, view, NONE, 0);
        avm_display(false);
        for(;;) {
            avm_idle();
            uint8_t buttons = avm_buttons();
            uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
            ui.previous_buttons = buttons;
            directional_press(buttons, edges);
            if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) return NONE;
        }
    }
    uint8_t selection = view.first_slot();
    uint8_t top = 0;
    for(;;) {
        render_inventory(prompt_text, view, selection, top);
        avm_display(false);
        for(;;) {
            avm_idle();
            uint8_t buttons = avm_buttons();
            uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
            ui.previous_buttons = buttons;
            uint8_t direction = directional_press(buttons, edges);
            if(edges & AVM_BUTTON_B) return NONE;
            if(edges & AVM_BUTTON_A) return selection;
            if(direction == AVM_BUTTON_U || direction == AVM_BUTTON_D) {
                selection = view.move(selection,
                    direction == AVM_BUTTON_U ? -1 : 1);
                view.keep_visible(selection, top);
                break;
            }
        }
    }
}

void yesno_button(uint8_t x, uint8_t y, bool affirmative)
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
    uint8_t buttons_y = static_cast<uint8_t>(status_y + 3);
    if(buttons_y > 56) buttons_y = 56;
    yesno_button(72, buttons_y, true);
    yesno_button(104, buttons_y, false);
    avm_draw_text_P(83, buttons_y + 6, F("Yes"));
    avm_draw_text_P(115, buttons_y + 6, F("No"));
    avm_display(false);
}

bool yesno_modal(const char AVM_PROGMEM* prompt_text, const Item* item)
{
    status_clear();
    render_play();
    render_yesno_prompt(prompt_text, item);
    for(;;) {
        avm_idle();
        uint8_t buttons = avm_buttons();
        uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
        ui.previous_buttons = buttons;
        directional_press(buttons, edges);
        if(edges & AVM_BUTTON_A) return true;
        if(edges & AVM_BUTTON_B) return false;
    }
}

__attribute__((noinline)) void prompt_stairs()
{
    if(game.paralyzed || session.ended) return;
    const char AVM_PROGMEM* question = nullptr;
    if(game.px == game.up_x && game.py == game.up_y)
        question = game.floor ? F("Go upstairs?") :
                                F("Leave the dungeon?");
    else if(game.floor < FLOORS - 1 &&
            game.px == game.down_x && game.py == game.down_y)
        question = F("Go downstairs?");
    if(!question) return;
    bool confirmed = yesno(question);
    status_clear();
    if(confirmed) take_stairs();
}

// The last drawn ground slot is on top, as in ArduRogue.
__attribute__((noinline)) void prompt_ground_items()
{
    uint8_t before = GROUND_ITEMS;
    while(!session.ended) {
        uint8_t slot = ground_item_before(game.px, game.py, before);
        if(slot == NONE) break;
        before = slot;
        Item item = ground_item_info(slot);
        if(yesno(F("Pick up"), item)) {
            status_clear();
            take_item(slot);
        } else {
            status_clear();
        }
    }
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

// Modal prompts and dungeon rendering must not inherit the main loop's frame.
__attribute__((noinline)) void handle_input(uint8_t buttons)
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
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        } else if(direction) {
            int8_t dx = direction == AVM_BUTTON_L ? -1 :
                        direction == AVM_BUTTON_R ? 1 : 0;
            int8_t dy = direction == AVM_BUTTON_U ? -1 :
                        direction == AVM_BUTTON_D ? 1 : 0;
            ui.mode = PLAY;
            status_clear();
            render();
            if(!throw_potion(ui.selection, dx, dy))
                ui.mode = THROW_DIRECTION;
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == MENU) {
        if(direction == AVM_BUTTON_U && ui.selection) {
            --ui.selection;
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_D && ui.selection < 6) {
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
                ui.mode = PLAY;
                if(uint8_t slot = choose_item(F("Use which item?"), nullptr);
                   slot != NONE) {
                    status_clear();
                    render();
                    uint8_t type = game.inventory[slot].type;
                    uint8_t target = NONE;
                    if(type == SCROLL_IDENTIFY)
                        target = choose_item(F("Identify which item?"), nullptr);
                    else if(type == SCROLL_ENCHANT)
                        target = choose_item(F("Enchant which item?"), nullptr);
                    else if(type == SCROLL_REMOVE_CURSE)
                        target = choose_item(F("Uncurse which item?"), nullptr);
                    status_clear();
                    render();
                    use_inventory(slot, target);
                } else status_clear();
                break;
            case 2:
                ui.mode = PLAY;
                if(uint8_t slot = choose_item(F("Drop which item?"), nullptr);
                   slot != NONE) {
                    status_clear();
                    render();
                    drop_inventory(slot);
                } else status_clear();
                break;
            case 3:
                ui.mode = PLAY;
                if(uint8_t slot = choose_item(F("Throw what?"), is_potion);
                   slot != NONE) {
                    ui.selection = slot;
                    ui.mode = THROW_DIRECTION;
                } else {
                    status_clear();
                    InventoryView potions(game, is_potion);
                    if(!potions.count) {
                        status(F("You have no potions."));
                    }
                }
                break;
            case 4: ui.mode = FULL_MAP; break;
            case 5:
                game.valid = 1;
                avm_save();
                ui.has_save = true;
                ui.mode = TITLE;
                break;
            case 6:
                if(yesno(F("Abandon this game?")))
                    finish(2);
                break;
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
        uint8_t old_x = game.px, old_y = game.py;
        int8_t dx = direction == AVM_BUTTON_L ? -1 :
                    direction == AVM_BUTTON_R ? 1 : 0;
        int8_t dy = direction == AVM_BUTTON_U ? -1 :
                    direction == AVM_BUTTON_D ? 1 : 0;
        move_player(dx, dy);
        if(!session.ended && (game.px != old_x || game.py != old_y)) {
            prompt_ground_items();
            prompt_stairs();
        }
    } else if(edges & AVM_BUTTON_A) {
        status_clear();
        action();
    } else {
        return;
    }
    ui.dirty = true;
}

} // namespace

uint8_t rogue::choose_item(const char AVM_PROGMEM* prompt_text,
                           ItemTypeFilter item_type_filter)
{
    return choose_item_modal(prompt_text, item_type_filter);
}

bool rogue::yesno(const char AVM_PROGMEM* prompt_text)
{
    return yesno_modal(prompt_text, nullptr);
}

bool rogue::yesno(const char AVM_PROGMEM* prompt_text, Item item)
{
    return yesno_modal(prompt_text, &item);
}

void rogue::status_word(const char* word)
{
    ui.repeat_suppressed = true;
    const int16_t space_width = text_width(" ");
    if((*word == '.' || *word == '!' || *word == '?' ||
        *word == ',' || *word == ':') &&
       status_x > 67)
        status_x = static_cast<uint8_t>(status_x - space_width);
    while(*word) {
        char part[16];
        uint8_t count = 0;
        int16_t width = 0;
        while(word[count] && count < sizeof(part) - 1) {
            part[count] = word[count];
            part[count + 1] = 0;
            int16_t candidate = text_width(part);
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

void rogue::status_word(const char* word, char punctuation)
{
    if(!punctuation) {
        status_word(word);
        return;
    }
    char mark[2] = {punctuation, 0};
    int16_t width = text_width(word) + text_width(mark);
    if(width > 60) {
        status_word(word);
        status_word(mark);
        return;
    }
    ui.repeat_suppressed = true;
    if(status_x != 67 && status_x + width > 128)
        status_next_line();
    status_x = static_cast<uint8_t>(avm_draw_text(status_x, status_y, word).x);
    status_x = static_cast<uint8_t>(avm_draw_text(status_x, status_y, mark).x +
                                    text_width(" "));
}

void rogue::status(const char PROGMEM* words)
{
    status_words_P(words);
}

void rogue::status(const char PROGMEM* words, char punctuation)
{
    status_words_P(words, punctuation);
}

void rogue::status(Item item)
{
    status(item, 0);
}

void rogue::status(Item item, char punctuation)
{
    StatusItemText text;
    emit_item(item, punctuation == '?' ? PROMPT_ITEM : STATUS_ITEM, text);
    text.finish(punctuation);
}

void rogue::status(MonsterType monster)
{
    status_entity(monster);
}

void rogue::status(MonsterType monster, char punctuation)
{
    status_entity(monster, punctuation);
}

void rogue::status_number(uint8_t value)
{
    status_number(value, 0);
}

void rogue::status_number(uint8_t value, char punctuation)
{
    status_formatted_number(value, false, punctuation);
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
