#include <avm.h>
#include "game.hpp"

namespace rogue {
Game game __attribute__((section(".saved"))) = {};
}

using namespace rogue;

namespace {

enum Mode : uint8_t {
    TITLE, PLAY, MENU, INVENTORY_MENU, FULL_MAP, END
};

struct Ui {
    uint8_t mode, selection, previous_buttons, held_direction;
    uint16_t next_repeat_ms;
    bool has_save, dirty;
};
static Ui ui = {};

// Four vertical columns per symbol. The fifth pixel is left blank between tiles.
static const uint8_t AVM_PROGMEM icons[][4] = {
    {0x06, 0x09, 0x0b, 0x06}, // player
    {0x06, 0x0f, 0x09, 0x06}, // ordinary monster
    {0x09, 0x06, 0x0f, 0x09}, // Lord of Darkness
    {0x04, 0x0e, 0x0e, 0x04}, // item
    {0x04, 0x04, 0x0f, 0x06}, // down stairs
    {0x06, 0x0f, 0x04, 0x04}, // up stairs
    {0x0f, 0x09, 0x09, 0x0f}, // door
};

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

void icon(uint8_t kind, uint8_t x, uint8_t y)
{
    for(uint8_t col = 0; col < 4; ++col)
        column(static_cast<uint8_t>(x + col), y, icons[kind][col]);
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

void render_message()
{
    switch(session.message) {
    case WELCOME: avm_draw_text_P(66, 55, F("WELCOME")); break;
    case WALL: avm_draw_text_P(66, 55, F("A WALL")); break;
    case OPENED: avm_draw_text_P(66, 55, F("DOOR OPEN")); break;
    case HIT: avm_draw_text_P(66, 55, F("YOU HIT")); break;
    case MISSED: avm_draw_text_P(66, 55, F("YOU MISS")); break;
    case HURT: avm_draw_text_P(66, 55, F("YOU HURT")); break;
    case KILLED: avm_draw_text_P(66, 55, F("FOE DOWN")); break;
    case FOUND: avm_draw_text_P(66, 55, F("A: PICK UP")); break;
    case PICKED_UP: avm_draw_text_P(66, 55, F("PICKED UP")); break;
    case FULL: avm_draw_text_P(66, 55, F("PACK FULL")); break;
    case HEALED: avm_draw_text_P(66, 55, F("HEALED")); break;
    case FED: avm_draw_text_P(66, 55, F("ATE FOOD")); break;
    case EQUIPPED: avm_draw_text_P(66, 55, F("EQUIPPED")); break;
    case STAIRS: avm_draw_text_P(66, 55, F("A: STAIRS")); break;
    case AMULET_FOUND: avm_draw_text_P(66, 55, F("AMULET!")); break;
    case HUNGRY: avm_draw_text_P(66, 55, F("STARVING")); break;
    case DROPPED: avm_draw_text_P(66, 55, F("DROPPED")); break;
    default: avm_draw_text_P(66, 55, F("A: WAIT")); break;
    }
}

void render_play()
{
    uint16_t sight[13] = {};
    uint16_t walls[13] = {};
    uint16_t opaque[13] = {};
    for(uint8_t sy = 0; sy < 13; ++sy)
        for(uint8_t sx = 0; sx < 13; ++sx) {
            int16_t x = static_cast<int16_t>(game.px) + sx - 6;
            int16_t y = static_cast<int16_t>(game.py) + sy - 6;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H ||
               (game.walls[static_cast<uint16_t>(y * (MAP_W / 8) +
                   (x >> 3))] & (1u << (x & 7))))
                walls[sy] |= static_cast<uint16_t>(1u << sx);
        }
    for(uint8_t sy = 0; sy < 13; ++sy)
        opaque[sy] = walls[sy];
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(!door.open && screen_tile(door.x, door.y, sx, sy))
            opaque[sy] |= static_cast<uint16_t>(1u << sx);
    }
    const Room* player_room = nullptr;
    for(const Room& room : game.rooms)
        if(game.px >= room.x && game.px < room.x + room.w &&
           game.py >= room.y && game.py < room.y + room.h) {
            player_room = &room;
            break;
        }
    for(uint8_t sy = 0; sy < 13; ++sy)
        for(uint8_t sx = 0; sx < 13; ++sx) {
            int16_t x = static_cast<int16_t>(game.px) + sx - 6;
            int16_t y = static_cast<int16_t>(game.py) + sy - 6;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H)
                continue;
            uint8_t tx = static_cast<uint8_t>(x);
            uint8_t ty = static_cast<uint8_t>(y);
            bool visible = (player_room &&
                tx >= player_room->x && tx < player_room->x + player_room->w &&
                ty >= player_room->y && ty < player_room->y + player_room->h) ||
                ray_visible(sx, sy, opaque);
            if(visible) {
                sight[sy] |= static_cast<uint16_t>(1u << sx);
                explore(tx, ty);
            }
            if(!visible && !explored(tx, ty))
                continue;
            uint8_t px = static_cast<uint8_t>(sx * 5);
            uint8_t py = static_cast<uint8_t>(sy * 5);
            if(walls[sy] & (1u << sx)) {
                for(uint8_t col = 0; col < 4; ++col)
                    column(static_cast<uint8_t>(px + col), py, 0x0f);
                if(sx < 12 && tx + 1 < MAP_W &&
                   (walls[sy] & (1u << (sx + 1))) && explored(tx + 1, ty))
                    column(static_cast<uint8_t>(px + 4), py, 0x0f);
                if(sy < 12 && ty + 1 < MAP_H &&
                   (walls[sy + 1] & (1u << sx)) && explored(tx, ty + 1))
                    for(uint8_t col = 0; col < 4; ++col)
                        column(static_cast<uint8_t>(px + col),
                               static_cast<uint8_t>(py + 4), 1);
            } else if(visible) {
                pixel(px + 2, py + 2);
            }
        }
    for(uint8_t i = 0; i < game.door_count; ++i) {
        const Door& door = game.doors[i];
        uint8_t sx, sy;
        if(!door.open && screen_tile(door.x, door.y, sx, sy) &&
           explored(door.x, door.y))
            icon(6, static_cast<uint8_t>(sx * 5),
                    static_cast<uint8_t>(sy * 5));
    }
    uint8_t sx, sy;
    if(screen_tile(game.up_x, game.up_y, sx, sy) &&
       explored(game.up_x, game.up_y))
        icon(5, static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    if(game.floor < FLOORS - 1 &&
       screen_tile(game.down_x, game.down_y, sx, sy) &&
       explored(game.down_x, game.down_y))
        icon(4, static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    for(const GroundItem& item : game.ground)
        if(item.type && in_sight(item.x, item.y, sight, sx, sy))
            icon(3, static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    for(const DroppedItem& item : game.dropped)
        if(item.type && item.floor == game.floor &&
           in_sight(item.x, item.y, sight, sx, sy))
            icon(3, static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    for(const Monster& monster : game.monsters)
        if(monster.type && in_sight(monster.x, monster.y, sight, sx, sy))
            icon(monster.type == LORD ? 2 : 1,
                 static_cast<uint8_t>(sx * 5), static_cast<uint8_t>(sy * 5));
    icon(0, 30, 30);
    for(uint8_t y = 0; y < 64; ++y)
        pixel(64, y);
    avm_draw_textf_P(67, 7, F("D%u LV%u"), game.floor + 1, game.level);
    avm_draw_textf_P(67, 15, F("HP%u/%u"), game.hp, game.max_hp);
    avm_draw_textf_P(67, 23, F("AT%u DF%u"), game.attack +
        (game.weapon_slot != NONE ? game.inventory[game.weapon_slot].amount : 0),
        game.defense);
    avm_draw_textf_P(67, 31, F("XP%u"), game.xp);
    avm_draw_textf_P(67, 39, F("FOOD%u"), game.hunger);
    avm_draw_textf_P(67, 47, F("SCORE%u"), game.score);
    render_message();
    avm_draw_text_P(67, 63, F("B:MENU"));
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
        F("WAIT"), F("INVENTORY"), F("FULL MAP"),
        F("SAVE & EXIT"), F("ABANDON")
    };
    avm_draw_text_P(10, 8, F("ACTION MENU"));
    for(uint8_t i = 0; i < 5; ++i) {
        if(i == ui.selection)
            avm_draw_text_P(4, static_cast<int16_t>(20 + 9 * i), F(">"));
        avm_draw_text_P(12, static_cast<int16_t>(20 + 9 * i), names[i]);
    }
}

void render_inventory()
{
    avm_draw_text_P(2, 7, F("A USE > DROP B BACK"));
    uint8_t start = ui.selection < 8 ? 0 : 8;
    for(uint8_t row = 0; row < 8; ++row) {
        uint8_t i = start + row;
        int16_t y = static_cast<int16_t>(14 + row * 7);
        if(i == ui.selection)
            avm_draw_text_P(1, y, F(">"));
        const Item& item = game.inventory[i];
        if(!item.type)
            continue;
        switch(item.type) {
        case FOOD: avm_draw_textf_P(10, y, F("FOOD x%u"), item.amount); break;
        case HEALING: avm_draw_textf_P(10, y, F("HEAL x%u"), item.amount); break;
        case SWORD: avm_draw_textf_P(10, y, F("SWORD +%u"), item.amount); break;
        case ARMOR: avm_draw_textf_P(10, y, F("ARMOR +%u"), item.amount); break;
        default: break;
        }
        if(game.weapon_slot == i || game.armor_slot == i)
            avm_draw_text_P(75, y, F("*"));
    }
}

void render_full_map()
{
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) {
            if(!explored(x, y))
                continue;
            if(wall_at(x, y)) {
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
    switch(ui.mode) {
    case TITLE: render_title(); break;
    case PLAY: render_play(); break;
    case MENU: render_menu(); break;
    case INVENTORY_MENU: render_inventory(); break;
    case FULL_MAP: render_full_map(); break;
    case END: render_end(); break;
    }
    avm_display(AVM_CLEAR_BUFFER);
    ui.dirty = false;
}

uint8_t directional_press(uint8_t buttons, uint8_t edges)
{
    uint8_t direction = buttons & static_cast<uint8_t>(
        AVM_BUTTON_U | AVM_BUTTON_D | AVM_BUTTON_L | AVM_BUTTON_R);
    uint16_t now = avm_millis();
    if(direction != ui.held_direction) {
        ui.held_direction = direction;
        ui.next_repeat_ms = static_cast<uint16_t>(now + 300);
        if(!(edges & direction))
            return 0;
    } else {
        if(!direction || static_cast<int16_t>(now - ui.next_repeat_ms) < 0)
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
                session.message = WELCOME;
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
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == MENU) {
        if(direction == AVM_BUTTON_U && ui.selection) {
            --ui.selection;
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_D && ui.selection < 4) {
            ++ui.selection;
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_B) {
            ui.mode = PLAY;
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_A) {
            switch(ui.selection) {
            case 0: ui.mode = PLAY; session.message = EMPTY; end_turn(); break;
            case 1: ui.mode = INVENTORY_MENU; ui.selection = 0; break;
            case 2: ui.mode = FULL_MAP; break;
            case 3:
                game.valid = 1;
                avm_save();
                ui.has_save = true;
                ui.mode = TITLE;
                break;
            case 4: finish(2); break;
            }
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == INVENTORY_MENU) {
        if(direction == AVM_BUTTON_U && ui.selection) {
            --ui.selection;
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_D && ui.selection < INVENTORY - 1) {
            ++ui.selection;
            ui.dirty = true;
        } else if(direction == AVM_BUTTON_R) {
            if(drop_inventory(ui.selection)) {
                ui.mode = PLAY;
                ui.dirty = true;
            }
        } else if(edges & AVM_BUTTON_B) {
            ui.mode = PLAY;
            ui.dirty = true;
        } else if(edges & AVM_BUTTON_A) {
            if(use_inventory(ui.selection)) {
                ui.mode = PLAY;
                ui.dirty = true;
            }
        }
        return;
    }
    if(edges & AVM_BUTTON_B) {
        ui.mode = MENU;
        ui.selection = 0;
    } else if(direction) {
        int8_t dx = direction == AVM_BUTTON_L ? -1 :
                    direction == AVM_BUTTON_R ? 1 : 0;
        int8_t dy = direction == AVM_BUTTON_U ? -1 :
                    direction == AVM_BUTTON_D ? 1 : 0;
        move_player(dx, dy);
    } else if(edges & AVM_BUTTON_A) {
        action();
    } else {
        return;
    }
    ui.dirty = true;
}

} // namespace

extern "C" int main()
{
    avm_set_text_font(AVM_FONT_5X7);
    if(avm_save_exists() && avm_load() &&
       game.magic == 0xa7 && game.version == 2 && game.valid)
        ui.has_save = true;
    ui.mode = TITLE;
    session.repeat_slot = NONE;
    ui.dirty = true;
    render();

    for(;;) {
        // Sleep until an interrupt, then process button edges or a held repeat.
        avm_idle();
        uint8_t buttons = avm_buttons();
        if(buttons != ui.previous_buttons ||
           (ui.held_direction && static_cast<int16_t>(avm_millis() -
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
