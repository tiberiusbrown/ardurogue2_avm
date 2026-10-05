#include "app_state.hpp"
#include "game.hpp"
#include "inventory_view.hpp"
#include "render.hpp"
#include "ui.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0;
uint8_t avm_test_button_index = 0;

static unsigned play_renders = 0;
static unsigned full_renders = 0;
static unsigned inventory_renders = 0;
static unsigned deferred_renders = 0;
static bool play_render_pending = false;
static uint8_t last_inventory_total = 0;
static void require(bool condition, const char* message);

namespace rogue {
Game game = {};
const char* status_word(const char*) { return nullptr; }
void status_words(const char*) {}
void status_suffix(char) {}
void status_capitalize() {}
void status(const char*) {}
void status(const char*, char) {}
void status(Item) {}
void status(Item, char) {}
void status(MonsterType) {}
void status(MonsterType, char) {}
void status_number(uint8_t) {}
void status_number(uint8_t, char) {}
void status_clear() {}
void defer_play_render() { ++deferred_renders; play_render_pending = true; }
void render_play() { ++play_renders; play_render_pending = false; }
void restore_play_render() { if(play_render_pending) render_play(); }
void render_inventory(const char*, const InventoryView& view, uint8_t selection,
                      uint8_t top, uint8_t total)
{
    require(total == view.count(), "modal reused a stale inventory count");
    if(total) {
        uint8_t row = view.position(selection);
        require(row != NONE && row >= top && row < top + INVENTORY_VISIBLE_ROWS,
                "modal selection row left its viewport");
    } else require(selection == NONE && top == 0, "empty modal retained a selection");
    last_inventory_total = total;
    ++inventory_renders;
}
void render_yesno_prompt(const char*, const Item*) { restore_play_render(); }
void render() { ++full_renders; play_render_pending = false; }
void animate_ray(Position, int8_t, int8_t, uint8_t) {}
void animate_fire_burst(Position) {}
void animate_spreading_rays(Position, const uint8_t[4]) {}
void animate_fire_bursts(const Position*, uint8_t, bool) {}
void invalidate_saved_game() {}
void resume_saved_game() {}
void save_resumable_game() {}
}

static void require(bool condition, const char* message)
{
    if(!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static rogue::InputAction dispatch_input(uint8_t buttons)
{
    using namespace rogue;
    InputAction input = handle_input(buttons);
    if(input >= INPUT_WAND_UP && input <= INPUT_WAND_IMMEDIATE) {
        int8_t dx = input == INPUT_WAND_RIGHT ? 1 :
                    input == INPUT_WAND_LEFT ? -1 : 0;
        int8_t dy = input == INPUT_WAND_UP ? -1 :
                    input == INPUT_WAND_DOWN ? 1 : 0;
        if(!use_wand(ui.selection, dx, dy) && input != INPUT_WAND_IMMEDIATE)
            ui.mode = WAND_DIRECTION;
    }
    return input;
}

int main()
{
    using namespace rogue;
    start_new(0x4312);
    game.inventory[0] = {WAND_FIRE, 3};
    ui.mode = WAND_DIRECTION;
    ui.selection = 0;
    ui.previous_buttons = 0;
    uint8_t turns = game.turns;
    require(dispatch_input(AVM_BUTTON_B) == INPUT_NONE && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 3 && game.turns == turns &&
            !item_type_identified(WAND_FIRE),
            "B did not cancel wand direction without spending resources");

    for(Monster& monster : game.monsters) monster.type = NO_MONSTER;
    game.player = {10, 10};
    for(uint8_t& wall : game.walls) wall = 0;
    game.door_count = 0;
    ui.mode = WAND_DIRECTION;
    ui.previous_buttons = 0;
    require(dispatch_input(AVM_BUTTON_R) == INPUT_WAND_RIGHT && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 2 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            item_type_identified(WAND_FIRE),
            "wand direction did not fire and spend one charge and turn");

    for(uint8_t modifier = WAND_NORMAL; modifier <= WAND_OVERPOWERED;
        ++modifier) {
        start_new(0x4312);
        std::memset(game.walls, 0, sizeof game.walls);
        std::memset(game.monsters, 0, sizeof game.monsters);
        std::memset(game.inventory, 0, sizeof game.inventory);
        game.door_count = 0;
        game.player = {10, 10};
        game.hp = game.max_hp = 240;
        game.inventory[0] = {WAND_DIGGING, 2};
        set_wand_modifier(game.inventory[0], static_cast<WandModifier>(modifier));
        ui.mode = MENU;
        ui.selection = 1;
        ui.previous_buttons = 0;
        avm_test_buttons[0] = 0;
        avm_test_buttons[1] = AVM_BUTTON_A;
        avm_test_button_count = 2;
        avm_test_button_index = 0;
        play_renders = full_renders = deferred_renders = 0;
        turns = game.turns;
        dispatch_input(AVM_BUTTON_A);
        bool needs_direction = modifier == WAND_NORMAL ||
                               modifier == WAND_POWERFUL;
        require(ui.mode == (needs_direction ? WAND_DIRECTION : PLAY) &&
                item_value(game.inventory[0]) == (needs_direction ? 2 : 1) &&
                game.turns == static_cast<uint8_t>(turns +
                                                  (needs_direction ? 0 : 1)),
                "wand selection UI targeted a modifier incorrectly");
        require(play_renders == 0 && full_renders == 0 && deferred_renders == 1,
                "wand selection eagerly restored the dungeon");
        if(needs_direction) {
            dispatch_input(AVM_BUTTON_B);
            require(ui.mode == PLAY && item_value(game.inventory[0]) == 2 &&
                    game.turns == turns,
                    "canceling a normal or powerful wand spent resources");
        }
    }
    // Item actions defer their background until a page/prompt actually needs
    // it; the main loop can otherwise render only the completed turn.
    const uint8_t types[] = {DAGGER, SPEAR, LONG_SWORD, MACE, TWO_HANDED_SWORD,
                             LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, CHAIN_MAIL, SPLINT_MAIL, PLATE_MAIL, RING_STRENGTH, AMULET_SPEED,
                             SCROLL_IDENTIFY, SCROLL_ENCHANT, SCROLL_REMOVE_CURSE};
    for(uint8_t type : types) {
        for(unsigned equipped = 0; equipped < 2; ++equipped) {
            start_new(0x4312);
            std::memset(game.monsters, 0, sizeof game.monsters);
            std::memset(game.inventory, 0, sizeof game.inventory);
            game.inventory[0] = {type, 1};
            if(equipped) {
                if(is_weapon(type)) game.weapon_slot = 0;
                if(is_armor(type)) game.armor_slot = 0;
                if(is_ring(type)) game.ring_slots[0] = 0;
                if(is_amulet(type)) game.amulet_slot = 0;
            }
            ui.mode = MENU;
            ui.selection = 1;
            ui.previous_buttons = 0;
            bool target_scroll = type == SCROLL_IDENTIFY || type == SCROLL_ENCHANT ||
                                 type == SCROLL_REMOVE_CURSE;
            avm_test_buttons[0] = 0;
            avm_test_buttons[1] = AVM_BUTTON_A;
            avm_test_buttons[2] = 0;
            avm_test_buttons[3] = AVM_BUTTON_A;
            avm_test_button_count = target_scroll ? 4 : 2;
            avm_test_button_index = 0;
            play_renders = full_renders = inventory_renders = deferred_renders = 0;
            turns = game.turns;
            dispatch_input(AVM_BUTTON_A);
            require(play_renders == 0 && full_renders == 0 && deferred_renders == 1 &&
                    inventory_renders == (target_scroll ? 2u : 1u),
                    "item selection eagerly restored or displayed the dungeon");
            require(ui.mode == PLAY && game.turns == static_cast<uint8_t>(turns + 1),
                    "item selection changed the turn or left the inventory open");
            if(is_amulet(type)) require(game.amulet_slot == (equipped ? NONE : 0),
                                        "amulet selection failed to toggle equipment");
            if(is_ring(type)) require(game.ring_slots[0] == (equipped ? NONE : 0),
                                      "ring selection failed to toggle equipment");
        }
    }
    // Each new modal must rebuild its metadata after inventory/filter changes.
    std::memset(game.inventory, 0, sizeof game.inventory);
    game.inventory[0] = {HEALING, 1};
    game.inventory[7] = {WAND_FORCE, 2};
    game.inventory[15] = {LONG_SWORD, 1};
    const uint8_t browsing[] = {0, AVM_BUTTON_D, 0, AVM_BUTTON_D,
                               0, AVM_BUTTON_U, 0, AVM_BUTTON_A};
    std::memcpy(avm_test_buttons, browsing, sizeof browsing);
    avm_test_button_count = sizeof browsing;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    turns = game.turns;
    require(choose_item(F("Choose"), nullptr) == 7 && last_inventory_total == 6 &&
            game.turns == turns,
            "browsing did not skip headers or preserve the game turn");

    game.inventory[15].type = NO_ITEM;
    const uint8_t next_item[] = {0, AVM_BUTTON_D, 0, AVM_BUTTON_A};
    std::memcpy(avm_test_buttons, next_item, sizeof next_item);
    avm_test_button_count = sizeof next_item;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), nullptr) == 0 && last_inventory_total == 4,
            "reopened modal retained removed inventory rows");

    avm_test_buttons[0] = 0;
    avm_test_buttons[1] = AVM_BUTTON_A;
    avm_test_button_count = 2;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), is_potion) == 0 && last_inventory_total == 2,
            "filtered modal reused unfiltered row metadata");

    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), is_equipment) == NONE && last_inventory_total == 0,
            "empty filtered modal retained row metadata");
    std::puts("equipment, scroll, wand, and inventory UI flow passed");
    return 0;
}
