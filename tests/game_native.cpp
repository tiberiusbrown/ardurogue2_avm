#include "game.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rogue {
Game game = {};
}

using namespace rogue;

namespace rogue {
void status_word(const char*) {}
void status(const char*) {}
void status(Item) {}
void status(MonsterType) {}
void status_number(uint8_t) {}
}

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

void check_local_visibility()
{
    uint8_t old_x = game.px, old_y = game.py;
    unsigned samples = 0;
    for(uint8_t y = 0; y < MAP_H && samples < 24; ++y)
        for(uint8_t x = 0; x < MAP_W && samples < 24; ++x) {
            if(wall_at(x, y) || (x + y * MAP_W) % 11 != 0)
                continue;
            game.px = x;
            game.py = y;
            ++samples;
            uint16_t opaque[13] = {};
            for(uint8_t sy = 0; sy < 13; ++sy)
                for(uint8_t sx = 0; sx < 13; ++sx)
                    if(blocked(static_cast<int16_t>(x) + sx - 6,
                               static_cast<int16_t>(y) + sy - 6))
                        opaque[sy] |= static_cast<uint16_t>(1u << sx);
            for(uint8_t sy = 0; sy < 13; ++sy)
                for(uint8_t sx = 0; sx < 13; ++sx) {
                    int16_t tx = static_cast<int16_t>(x) + sx - 6;
                    int16_t ty = static_cast<int16_t>(y) + sy - 6;
                    if(tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H)
                        require(ray_visible(sx, sy, opaque) ==
                                can_see(static_cast<uint8_t>(tx),
                                        static_cast<uint8_t>(ty)),
                                "local visibility differs from world ray");
                }
        }
    require(samples == 24, "too few visibility samples");
    game.px = old_x;
    game.py = old_y;
}

void check_wall_faces()
{
    std::array<uint8_t, sizeof(game.walls)> saved_walls;
    std::memcpy(saved_walls.data(), game.walls, saved_walls.size());
    std::memset(game.walls, 0xff, sizeof(game.walls));
    const uint16_t floor = 10 + 10 * MAP_W;
    game.walls[floor >> 3] &= static_cast<uint8_t>(~(1u << (floor & 7)));
    require(wall_exposed(11, 10), "wall beside floor has no face");
    require(!wall_exposed(12, 10), "solid wall interior has a face");
    require(!wall_exposed(10, 10), "floor was classified as a wall");
    std::memcpy(game.walls, saved_walls.data(), saved_walls.size());
}

void check_exploration_resolution()
{
    std::array<uint8_t, sizeof(game.explored)> saved_explored;
    std::memcpy(saved_explored.data(), game.explored, saved_explored.size());
    std::memset(game.explored, 0, sizeof(game.explored));
    explore(10, 10);
    require(explored(10, 10), "explored tile was not recorded");
    require(!explored(11, 10) && !explored(10, 11) && !explored(11, 11),
            "exploration leaked into adjacent tiles");
    explore(MAP_W - 1, MAP_H - 1);
    require(explored(MAP_W - 1, MAP_H - 1), "last tile was not recorded");
    require(!explored(MAP_W - 2, MAP_H - 1),
            "last tile leaked into its neighbor");
    std::memcpy(game.explored, saved_explored.data(), saved_explored.size());
}

void check_floor_marks()
{
    static_assert(sizeof(FloorMarks) == 7, "floor flags should occupy 51 bits");
    FloorMarks marks = {};
    mark(marks, TAKEN_ITEMS, GROUND_ITEMS - 1);
    mark(marks, KILLED_MONSTERS, MONSTERS - 1);
    mark(marks, OPENED_DOORS, DOORS - 1);
    mark(marks, VISITED_ROOMS, ROOMS - 1);
    require(marked(marks, TAKEN_ITEMS, GROUND_ITEMS - 1) &&
            marked(marks, KILLED_MONSTERS, MONSTERS - 1) &&
            marked(marks, OPENED_DOORS, DOORS - 1) &&
            marked(marks, VISITED_ROOMS, ROOMS - 1),
            "packed floor flags lost a high bit");
    require(!marked(marks, KILLED_MONSTERS, 0) &&
            !marked(marks, OPENED_DOORS, 0) &&
            !marked(marks, VISITED_ROOMS, 0),
            "packed floor flags overlap across groups");
    FloorMarks old_marks = game.marks[game.floor];
    game.marks[game.floor] = {};
    require(!door_open(0), "unmarked door appears open");
    mark(game.marks[game.floor], OPENED_DOORS, 0);
    require(door_open(0), "door did not use its floor flag");
    game.marks[game.floor] = old_marks;
}

int main()
{
    start_new(0x1234);
    require(game.floor == 0 && game.hp == 18 && game.valid &&
            game.version == SAVE_VERSION,
            "new game state is wrong");
    require(wall_at(-1, 0) && wall_at(MAP_W, 0),
            "map bounds are not solid");
    check_local_visibility();
    check_wall_faces();
    check_exploration_resolution();
    check_floor_marks();

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
    game.inventory[0] = {FOOD, 1, {0, 0}};
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
