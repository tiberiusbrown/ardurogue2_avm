#include "game.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rogue {
Game game = {};
}

using namespace rogue;

void require(bool condition, const char* reason)
{
    if(!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(1);
    }
}

void use_stairs(uint8_t floor, uint8_t x, uint8_t y)
{
    game.px = x;
    game.py = y;
    for(int i = 0; i < 4 && game.floor == floor; ++i)
        action();
    require(game.floor != floor, "stairs did not change floors");
}

int main()
{
    start_new(0x1234);
    require(game.floor == 0 && game.hp == 18 && game.valid,
            "new game state is wrong");
    require(wall_at(-1, 0) && wall_at(MAP_W, 0),
            "map bounds are not solid");

    std::array<uint8_t, sizeof(game.walls)> first_floor;
    std::memcpy(first_floor.data(), game.walls, first_floor.size());

    // Find an unoccupied floor tile and leave an item on it.
    bool found = false;
    uint8_t drop_x = 0, drop_y = 0;
    for(uint8_t y = 0; y < MAP_H && !found; ++y)
        for(uint8_t x = 0; x < MAP_W && !found; ++x)
            if(!wall_at(x, y) && item_at(x, y) == NONE &&
               monster_at(x, y) == NONE &&
               !(x == game.up_x && y == game.up_y) &&
               !(x == game.down_x && y == game.down_y)) {
                drop_x = x;
                drop_y = y;
                found = true;
            }
    require(found, "no free floor tile");
    game.px = drop_x;
    game.py = drop_y;
    game.inventory[0] = {FOOD, 1};
    require(drop_inventory(0) && game.inventory[0].type == NO_ITEM,
            "inventory drop failed");
    require(item_at(drop_x, drop_y) >= GROUND_ITEMS,
            "dropped item is missing");

    uint8_t down_x = game.down_x, down_y = game.down_y;
    use_stairs(0, down_x, down_y);
    require(game.floor == 1, "descent failed");
    use_stairs(1, game.up_x, game.up_y);
    require(game.floor == 0, "ascent failed");
    require(std::memcmp(first_floor.data(), game.walls,
                        first_floor.size()) == 0,
            "floor changed on revisit");
    require(item_at(drop_x, drop_y) >= GROUND_ITEMS,
            "dropped item was lost across floors");
    game.px = drop_x;
    game.py = drop_y;
    action();
    require(game.inventory[0].type == FOOD &&
            item_at(drop_x, drop_y) == NONE,
            "dropped item pickup failed");

    game.has_amulet = 1;
    game.px = game.up_x;
    game.py = game.up_y;
    for(int i = 0; i < 4 && !session.ended; ++i)
        action();
    require(session.ended && session.result == 1 && !game.valid,
            "amulet victory failed");

    start_new(0x4321);
    game.hp = 1;
    game.hunger = 0;
    game.turns = 3;
    end_turn();
    require(session.ended && session.result == 0 && !game.valid,
            "starvation death failed");

    std::puts("native game checks passed");
    return 0;
}
