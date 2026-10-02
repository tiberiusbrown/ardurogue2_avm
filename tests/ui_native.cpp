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
void render_play() {}
void render_inventory(const char*, const InventoryView&, uint8_t, uint8_t) {}
void render_yesno_prompt(const char*, const Item*) {}
void render() {}
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
        turns = game.turns;
        dispatch_input(AVM_BUTTON_A);
        bool needs_direction = modifier == WAND_NORMAL ||
                               modifier == WAND_POWERFUL;
        require(ui.mode == (needs_direction ? WAND_DIRECTION : PLAY) &&
                item_value(game.inventory[0]) == (needs_direction ? 2 : 1) &&
                game.turns == static_cast<uint8_t>(turns +
                                                  (needs_direction ? 0 : 1)),
                "wand selection UI targeted a modifier incorrectly");
        if(needs_direction) {
            dispatch_input(AVM_BUTTON_B);
            require(ui.mode == PLAY && item_value(game.inventory[0]) == 2 &&
                    game.turns == turns,
                    "canceling a normal or powerful wand spent resources");
        }
    }
    std::puts("wand UI flow passed");
    return 0;
}
