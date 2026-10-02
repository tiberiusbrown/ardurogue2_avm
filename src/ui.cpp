#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "inventory_view.hpp"
#include "persistence.hpp"
#include "render.hpp"
#include "status.hpp"
#include "ui.hpp"

namespace rogue {

Ui ui = {};

static uint8_t directional_press(uint8_t buttons, uint8_t edges)
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
static uint8_t choose_item_modal(const char AVM_PROGMEM* prompt_text,
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

static bool yesno_modal(const char AVM_PROGMEM* prompt_text, const Item* item)
{
    status_clear();
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
    render_play();
    bool confirmed = yesno(question);
    status_clear();
    if(confirmed) take_stairs();
}

// The last drawn ground slot is on top, as in ArduRogue.
__attribute__((noinline)) void prompt_ground_items()
{
    uint8_t before = GROUND_ITEMS;
    while(!session.ended && !game.paralyzed) {
        uint8_t slot = ground_item_before(game.px, game.py, before);
        if(slot == NONE) break;
        before = slot;
        Item item = ground_item_info(slot);
        render_play();
        if(yesno(F("Pick up"), item)) {
            status_clear();
            take_item(slot);
        } else {
            status_clear();
        }
    }
}

static void begin_new_game()
{
    if(ui.has_save) {
        invalidate_saved_game();
    }
    start_new(avm_generate_random_seed());
    ui.has_save = false;
    ui.mode = PLAY;
    status_clear();
    status(F("Welcome to the dungeon."));
}

// Return after movement so the main loop can prompt with this frame unwound.
__attribute__((noinline)) bool handle_input(uint8_t buttons)
{
    bool moved = false;
    uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
    ui.previous_buttons = buttons;
    uint8_t direction = directional_press(buttons, edges);
    if(ui.mode == TITLE) {
        if(edges & AVM_BUTTON_A) {
            if(ui.has_save) {
                resume_saved_game();
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
        return false;
    }
    if(ui.mode == END) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = TITLE;
            ui.dirty = true;
        }
        return false;
    }
    if(ui.mode == FULL_MAP) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        }
        return false;
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
        return false;
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
                save_resumable_game();
                ui.has_save = true;
                ui.mode = TITLE;
                break;
            case 6:
                render_play();
                if(yesno(F("Abandon this game?")))
                    finish(ABANDONED);
                break;
            }
            ui.dirty = true;
        }
        return false;
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
        moved = !session.ended &&
            (game.px != old_x || game.py != old_y);
    } else if(edges & AVM_BUTTON_A) {
        status_clear();
        action();
    } else {
        return false;
    }
    ui.dirty = true;
    return moved;
}

uint8_t choose_item(const char AVM_PROGMEM* prompt_text,
                           ItemTypeFilter item_type_filter)
{
    return choose_item_modal(prompt_text, item_type_filter);
}

bool yesno(const char AVM_PROGMEM* prompt_text)
{
    return yesno_modal(prompt_text, nullptr);
}

bool yesno(const char AVM_PROGMEM* prompt_text, Item item)
{
    return yesno_modal(prompt_text, &item);
}
} // namespace rogue
