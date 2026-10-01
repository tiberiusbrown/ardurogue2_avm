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
    uint8_t mode, selection, previous_buttons, held_direction, held_frames;
    bool has_save, dirty;
};
static Ui ui = {};

// Five rows of five pixels for each icon, kept entirely in program memory.
static const uint8_t AVM_PROGMEM icons[][5] = {
    {0x0e, 0x11, 0x17, 0x10, 0x0e}, // player
    {0x00, 0x0e, 0x0a, 0x0e, 0x00}, // ordinary monster
    {0x15, 0x0e, 0x1f, 0x0e, 0x15}, // Lord of Darkness
    {0x04, 0x0e, 0x0e, 0x04, 0x00}, // item
    {0x04, 0x0e, 0x1f, 0x04, 0x04}, // down stairs
    {0x04, 0x04, 0x1f, 0x0e, 0x04}, // up stairs
    {0x11, 0x0a, 0x04, 0x0a, 0x11}, // door
};

void pixel(int16_t x, int16_t y)
{
    if(x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] |= static_cast<uint8_t>(1u << (y & 7));
}

void icon(uint8_t kind, int16_t x, int16_t y)
{
    for(uint8_t row = 0; row < 5; ++row) {
        uint8_t bits = icons[kind][row];
        for(uint8_t col = 0; col < 5; ++col)
            if(bits & (1u << col))
                pixel(x + col, y + row);
    }
}

void map_tile(uint8_t x, uint8_t y, uint8_t sx, uint8_t sy)
{
    int16_t px = sx * 5;
    int16_t py = sy * 5;
    bool visible = can_see(x, y);
    if(visible)
        explore(x, y);
    if(!visible && !explored(x, y))
        return;
    if(wall_at(x, y)) {
        for(uint8_t k = 0; k < 5; ++k) {
            pixel(px + k, py);
            pixel(px, py + k);
        }
    } else if(visible) {
        pixel(px + 2, py + 2);
    }
    uint8_t door = door_at(x, y);
    if(door != NONE && !game.doors[door].open)
        icon(6, px, py);
    if(x == game.up_x && y == game.up_y)
        icon(5, px, py);
    else if(game.floor < FLOORS - 1 &&
            x == game.down_x && y == game.down_y)
        icon(4, px, py);
    if(!visible)
        return;
    if(item_at(x, y) != NONE)
        icon(3, px, py);
    uint8_t monster = monster_at(x, y);
    if(monster != NONE)
        icon(game.monsters[monster].type == LORD ? 2 : 1, px, py);
    if(x == game.px && y == game.py)
        icon(0, px, py);
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
    for(uint8_t sy = 0; sy < 13; ++sy)
        for(uint8_t sx = 0; sx < 13; ++sx) {
            int16_t x = static_cast<int16_t>(game.px) + sx - 6;
            int16_t y = static_cast<int16_t>(game.py) + sy - 6;
            if(x >= 0 && x < MAP_W && y >= 0 && y < MAP_H)
                map_tile(static_cast<uint8_t>(x), static_cast<uint8_t>(y), sx, sy);
        }
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
            } else if(can_see(x, y)) {
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
    if(direction != ui.held_direction) {
        ui.held_direction = direction;
        ui.held_frames = 0;
    } else if(direction && ui.held_frames < 250) {
        ++ui.held_frames;
    }
    if(direction && ((edges & direction) ||
        (ui.held_frames >= 9 && ui.held_frames % 3 == 0))) {
        if(direction & AVM_BUTTON_U) return AVM_BUTTON_U;
        if(direction & AVM_BUTTON_D) return AVM_BUTTON_D;
        if(direction & AVM_BUTTON_L) return AVM_BUTTON_L;
        if(direction & AVM_BUTTON_R) return AVM_BUTTON_R;
    }
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
        if(direction == AVM_BUTTON_U && ui.selection)
            --ui.selection;
        else if(direction == AVM_BUTTON_D && ui.selection < 4)
            ++ui.selection;
        else if(edges & AVM_BUTTON_B)
            ui.mode = PLAY;
        else if(edges & AVM_BUTTON_A) {
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
        }
        ui.dirty = true;
        return;
    }
    if(ui.mode == INVENTORY_MENU) {
        if(direction == AVM_BUTTON_U && ui.selection)
            --ui.selection;
        else if(direction == AVM_BUTTON_D && ui.selection < INVENTORY - 1)
            ++ui.selection;
        else if(direction == AVM_BUTTON_R) {
            if(drop_inventory(ui.selection))
                ui.mode = PLAY;
        }
        else if(edges & AVM_BUTTON_B)
            ui.mode = PLAY;
        else if(edges & AVM_BUTTON_A) {
            if(use_inventory(ui.selection))
                ui.mode = PLAY;
        }
        ui.dirty = true;
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
    avm_set_frame_rate(30);
    avm_set_text_font(AVM_FONT_5X7);
    if(avm_save_exists() && avm_load() &&
       game.magic == 0xa7 && game.version == 2 && game.valid)
        ui.has_save = true;
    ui.mode = TITLE;
    session.repeat_slot = NONE;
    ui.dirty = true;

    for(;;) {
        if(!avm_next_frame())
            continue;
        handle_input(avm_buttons());
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
