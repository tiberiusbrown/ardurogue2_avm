#include "app_state.hpp"
#include "game.hpp"
#include "inventory_view.hpp"
#include "render.hpp"
#include "ui.hpp"

#include <cstdio>
#include <cstdlib>

namespace rogue {
Game game = {};
void status_word(const char*) {}
void status_word(const char*, char) {}
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

int main()
{
    using namespace rogue;
    start_new(0x4312);
    game.inventory[0] = {WAND_FIRE, 3};
    ui.mode = WAND_DIRECTION;
    ui.selection = 0;
    ui.previous_buttons = 0;
    uint8_t turns = game.turns;
    require(!handle_input(AVM_BUTTON_B) && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 3 && game.turns == turns &&
            !item_type_identified(WAND_FIRE),
            "B did not cancel wand direction without spending resources");

    for(Monster& monster : game.monsters) monster.type = NO_MONSTER;
    game.player = {10, 10};
    for(uint8_t& wall : game.walls) wall = 0;
    game.door_count = 0;
    ui.mode = WAND_DIRECTION;
    ui.previous_buttons = 0;
    require(!handle_input(AVM_BUTTON_R) && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 2 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            item_type_identified(WAND_FIRE),
            "wand direction did not fire and spend one charge and turn");
    std::puts("wand UI flow passed");
    return 0;
}
