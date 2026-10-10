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
// Modal loops release their rendering temporaries before gameplay resumes.
__attribute__((noinline)) static uint8_t choose_item_modal(
    const char AVM_PROGMEM* prompt_text, ItemTypeFilter item_type_filter)
{
    InventoryView view(game, item_type_filter);
    const uint8_t total = view.count(); // Inventory stays unchanged until this modal returns.
    if(!total) {
        render_inventory(prompt_text, view, NONE, 0, total);
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
    uint8_t selected_row = 1; // First item follows the first nonempty group header.
    uint8_t top = 0;
    for(;;) {
        render_inventory(prompt_text, view, selection, top, total);
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
                    direction == AVM_BUTTON_U ? -1 : 1, selected_row, total);
                view.keep_row_visible(selected_row, total, top);
                break;
            }
        }
    }
}

__attribute__((noinline)) static bool yesno_modal(
    const char AVM_PROGMEM* prompt_text, const Item* item)
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

static void show_pickup_rejection(const char AVM_PROGMEM* message)
{
    render_play();
    status_clear();
    status(message);
    avm_display(false);
    for(;;) {
        avm_idle();
        uint8_t buttons = avm_buttons();
        uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
        ui.previous_buttons = buttons;
        directional_press(buttons, edges);
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) break;
    }
    status_clear();
}

__attribute__((noinline)) static void resolve_full_pack_pickup(uint8_t ground_slot)
{
    show_pickup_rejection(F("Your pack is full."));
    for(;;) {
        uint8_t slot = choose_item(F("Leave which item?"), nullptr);
        if(slot == NONE) { status_clear(); return; }
        Item item = game.inventory[slot];
        if(item.type == YENDOR_AMULET) {
            show_pickup_rejection(F("You cannot leave the amulet of Yendor."));
            continue;
        }
        if(!inventory_item_removable(slot)) {
            show_pickup_rejection(F("The cursed item cannot be removed."));
            continue;
        }
        render_play();
        if(!yesno(F("Leave"), item)) { status_clear(); continue; }
        status_clear();
        swap_ground_item(ground_slot, slot);
        return;
    }
}

__attribute__((noinline)) void prompt_stairs()
{
    if(game.paralyzed || session.ended) return;
    const char AVM_PROGMEM* question = nullptr;
    if(game.player == game.up) {
        if(!game.has_amulet) {
            status(F("The way up is closed until you find"));
            status(Item{YENDOR_AMULET, 1}, '.');
            return;
        }
        question = game.floor ? F("Go upstairs?") :
                                F("Leave the dungeon?");
    } else if(game.floor < FLOORS - 1 &&
              game.player == game.down) {
        if(game.has_amulet) {
            status_capitalize(); status(Item{YENDOR_AMULET, 1});
            status(F("calls you toward the surface."));
            return;
        }
        question = F("Go downstairs?");
    }
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
        uint8_t slot = ground_item_before(game.player, before);
        if(slot == NONE) break;
        before = slot;
        Item item = ground_item_info(slot);
        render_play();
        if(yesno(F("Pick up"), item)) {
            status_clear();
            if(take_item(slot) == PICKUP_NEEDS_SWAP)
                resolve_full_pack_pickup(slot);
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

// Defer wand/projectile activation to the main loop so this UI frame unwinds first.
__attribute__((noinline)) InputAction handle_input(uint8_t buttons)
{
    bool moved = false;
    InputAction pending = INPUT_NONE;
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
        return INPUT_NONE;
    }
    if(ui.mode == END) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = TITLE;
            ui.dirty = true;
        }
        return INPUT_NONE;
    }
    if(ui.mode == FULL_MAP) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        }
        return INPUT_NONE;
    }
    if(ui.mode == PROJECTILE_DIRECTION || ui.mode == WAND_DIRECTION) {
        if(edges & AVM_BUTTON_B) {
            ui.mode = PLAY;
            status_clear();
            ui.dirty = true;
        } else if(direction) {
            bool wand_direction = ui.mode == WAND_DIRECTION;
            ui.mode = PLAY;
            status_clear();
            defer_play_render();
            if(wand_direction) {
                ui.dirty = true;
                return direction == AVM_BUTTON_U ? INPUT_WAND_UP :
                       direction == AVM_BUTTON_R ? INPUT_WAND_RIGHT :
                       direction == AVM_BUTTON_D ? INPUT_WAND_DOWN :
                                                   INPUT_WAND_LEFT;
            } else {
                ui.dirty = true;
                return direction == AVM_BUTTON_U ? INPUT_PROJECTILE_UP :
                       direction == AVM_BUTTON_R ? INPUT_PROJECTILE_RIGHT :
                       direction == AVM_BUTTON_D ? INPUT_PROJECTILE_DOWN : INPUT_PROJECTILE_LEFT;
            }
        }
        return INPUT_NONE;
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
            case 1: {
                ui.mode = PLAY;
                uint8_t slot = choose_item(F("Use which item?"), nullptr);
                if(slot != NONE) {
                    uint8_t type = game.inventory[slot].type;
                    uint8_t target = NONE;
                    if(is_wand(type)) {
                        status_clear();
                        defer_play_render();
                        ui.selection = slot;
                        if(wand_needs_direction(game.inventory[slot]))
                            ui.mode = WAND_DIRECTION;
                        else
                            pending = INPUT_WAND_IMMEDIATE;
                    } else if(type == SCROLL_IDENTIFY)
                        target = choose_item(F("Identify which item?"), nullptr);
                    else if(type == SCROLL_ENCHANT)
                        target = choose_item(F("Enchant which item?"), nullptr);
                    else if(type == SCROLL_REMOVE_CURSE)
                        target = choose_item(F("Uncurse which item?"), nullptr);
                    if(!is_wand(type) && ui.mode != WAND_DIRECTION) {
                        status_clear();
                        // Pagination restores the background if needed; the
                        // main loop otherwise draws only the completed turn.
                        defer_play_render();
                        use_inventory(slot, target);
                    }
                } else status_clear();
                break;
            }
            case 2: {
                ui.mode = PLAY;
                uint8_t slot = choose_item(F("Drop which item?"), nullptr);
                if(slot != NONE) {
                    status_clear();
                    defer_play_render();
                    DropDisposition disposition = drop_disposition(slot);
                    if(disposition == DROP_DISCARD_ALL ||
                       disposition == DROP_DISCARD_REST) {
                        bool confirmed = disposition == DROP_DISCARD_REST
                            ? yesno(F("Discard the rest?"))
                            : (is_stackable(game.inventory[slot].type)
                                ? yesno(F("Discard this item?"))
                                : yesno(F("Discard"), game.inventory[slot]));
                        status_clear();
                        if(confirmed) drop_inventory(slot, true);
                    } else {
                        drop_inventory(slot);
                    }
                } else status_clear();
                break;
            }
            case 3: {
                ui.mode = PLAY;
                uint8_t slot = choose_item(F("Throw/Shoot what?"), is_throwable_or_shootable);
                if(slot != NONE) {
                    ui.selection = slot;
                    ui.mode = PROJECTILE_DIRECTION;
                } else {
                    status_clear();
                    InventoryView projectiles(game, is_throwable_or_shootable);
                    if(!projectiles.count()) {
                        status(F("You have nothing to throw or shoot."));
                    }
                }
                break;
            }
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
        return pending;
    }
    if(edges & AVM_BUTTON_B) {
        ui.mode = MENU;
        ui.selection = 0;
    } else if(direction) {
        status_clear();
        Position old = game.player;
        int8_t dx = direction == AVM_BUTTON_L ? -1 :
                    direction == AVM_BUTTON_R ? 1 : 0;
        int8_t dy = direction == AVM_BUTTON_U ? -1 :
                    direction == AVM_BUTTON_D ? 1 : 0;
        move_player(dx, dy);
        moved = !session.ended && game.player != old;
    } else if(edges & AVM_BUTTON_A) {
        status_clear();
        action();
    } else {
        return INPUT_NONE;
    }
    ui.dirty = true;
    return moved ? INPUT_MOVED : INPUT_NONE;
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

__attribute__((noinline)) bool yesno(const char AVM_PROGMEM* prompt_text, Item item)
{
    return yesno_modal(prompt_text, &item);
}
} // namespace rogue
