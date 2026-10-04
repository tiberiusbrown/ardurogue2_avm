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
void render_play() { ++play_renders; }
void render_inventory(const char*, const InventoryView&, uint8_t, uint8_t)
{
    ++inventory_renders;
}
void render_yesno_prompt(const char*, const Item*) {}
void render() { ++full_renders; }
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
        play_renders = full_renders = 0;
        turns = game.turns;
        dispatch_input(AVM_BUTTON_A);
        bool needs_direction = modifier == WAND_NORMAL ||
                               modifier == WAND_POWERFUL;
        require(ui.mode == (needs_direction ? WAND_DIRECTION : PLAY) &&
                item_value(game.inventory[0]) == (needs_direction ? 2 : 1) &&
                game.turns == static_cast<uint8_t>(turns +
                                                  (needs_direction ? 0 : 1)),
                "wand selection UI targeted a modifier incorrectly");
        require(play_renders == 1 && full_renders == 0,
                "wand selection displayed an intermediate dungeon frame");
        if(needs_direction) {
            dispatch_input(AVM_BUTTON_B);
            require(ui.mode == PLAY && item_value(game.inventory[0]) == 2 &&
                    game.turns == turns,
                    "canceling a normal or powerful wand spent resources");
        }
    }
    // Inventory actions restore the play framebuffer once, after the final
    // selection, so status pagination has a dungeon background without an
    // intermediate display of the empty status area.
    const uint8_t types[] = {SWORD, ARMOR, RING_STRENGTH, AMULET_SPEED,
                             SCROLL_IDENTIFY, SCROLL_ENCHANT, SCROLL_REMOVE_CURSE};
    for(uint8_t type : types) {
        for(unsigned equipped = 0; equipped < 2; ++equipped) {
            start_new(0x4312);
            std::memset(game.monsters, 0, sizeof game.monsters);
            std::memset(game.inventory, 0, sizeof game.inventory);
            game.inventory[0] = {type, 1};
            if(equipped) {
                if(type == SWORD) game.weapon_slot = 0;
                if(type == ARMOR) game.armor_slot = 0;
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
            play_renders = full_renders = inventory_renders = 0;
            turns = game.turns;
            dispatch_input(AVM_BUTTON_A);
            require(play_renders == 1 && full_renders == 0 &&
                    inventory_renders == (target_scroll ? 2u : 1u),
                    "item selection redrew or displayed the dungeon before its final choice");
            require(ui.mode == PLAY && game.turns == static_cast<uint8_t>(turns + 1),
                    "item selection changed the turn or left the inventory open");
            if(is_amulet(type)) require(game.amulet_slot == (equipped ? NONE : 0),
                                        "amulet selection failed to toggle equipment");
            if(is_ring(type)) require(game.ring_slots[0] == (equipped ? NONE : 0),
                                      "ring selection failed to toggle equipment");
        }
    }
    std::puts("equipment, scroll, and wand UI flow passed");
    return 0;
}
