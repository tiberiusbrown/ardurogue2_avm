#include "game.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static std::string status_text;

namespace rogue {
Game game = {};
}

using namespace rogue;

namespace rogue {
void status_word(const char*) {}
void status(const char* words) { status_text += words; status_text += ' '; }
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

void check_circular_light_radius()
{
    std::array<uint8_t, sizeof(game.walls)> saved_walls;
    std::memcpy(saved_walls.data(), game.walls, saved_walls.size());
    uint8_t old_x = game.px, old_y = game.py;
    uint8_t old_door_count = game.door_count;
    std::memset(game.walls, 0, sizeof(game.walls));
    game.door_count = 0;
    game.px = 20;
    game.py = 15;
    uint16_t opaque[13] = {};
    require(ray_visible(12, 6, opaque) && can_see(26, 15),
            "cardinal tile at the light radius is hidden");
    require(ray_visible(11, 9, opaque) && can_see(25, 18),
            "diagonal tile inside the light radius is hidden");
    require(!ray_visible(12, 7, opaque) && !can_see(26, 16),
            "tile outside the light radius is visible");
    require(!ray_visible(11, 11, opaque) && !can_see(25, 20),
            "diagonal tile outside the light radius is visible");
    std::memcpy(game.walls, saved_walls.data(), saved_walls.size());
    game.door_count = old_door_count;
    game.px = old_x;
    game.py = old_y;
}

void check_wall_faces()
{
    auto check_neighbors = [] {
        for(uint8_t y = 0; y < MAP_H; ++y)
            for(uint8_t x = 0; x < MAP_W; ++x) {
                bool expected = false;
                if(wall_at(x, y))
                    for(int8_t dy = -1; dy <= 1; ++dy)
                        for(int8_t dx = -1; dx <= 1; ++dx)
                            if((dx || dy) && !wall_at(x + dx, y + dy))
                                expected = true;
                require(wall_exposed(x, y) == expected,
                        "wall exposure differs from its neighboring tiles");
            }
    };
    check_neighbors();
    std::array<uint8_t, sizeof(game.walls)> saved_walls;
    std::memcpy(saved_walls.data(), game.walls, saved_walls.size());
    std::memset(game.walls, 0xff, sizeof(game.walls));
    const uint16_t floor = 10 + 10 * MAP_W;
    game.walls[floor >> 3] &= static_cast<uint8_t>(~(1u << (floor & 7)));
    require(wall_exposed(11, 10), "wall beside floor has no face");
    require(wall_exposed(11, 11), "diagonal room corner has no face");
    require(!wall_exposed(12, 10), "solid wall interior has a face");
    require(!wall_exposed(12, 11), "wall beyond the corner has a face");
    require(!wall_exposed(10, 10), "floor was classified as a wall");
    check_neighbors();
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

void check_potions()
{
    start_new(0x1234);
    bool colors[POTION_COUNT] = {};
    for(uint8_t type = HEALING; type <= INVISIBILITY; ++type) {
        uint8_t color = potion_color(type);
        require(color < POTION_COUNT && !colors[color],
                "potion appearances are not a permutation");
        colors[color] = true;
        require(!potion_identified(type), "new potion starts identified");
    }
    uint8_t original[POTION_COUNT];
    std::memcpy(original, game.potion_appearance, sizeof(original));
    start_new(0x1234);
    require(std::memcmp(original, game.potion_appearance, sizeof(original)) == 0,
            "same run seed changed potion names");
    start_new(0x4321);
    require(std::memcmp(original, game.potion_appearance, sizeof(original)) != 0,
            "different runs share the same potion names");

    auto drink = [](uint8_t type, uint8_t amount = 1) {
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.inventory[0] = {type, amount, {0, 0}};
        require(use_inventory(0), "potion could not be drunk");
        require(potion_identified(type), "drinking did not identify potion");
    };
    game.hp = 2;
    game.weakened = 2;
    drink(HEALING, 2);
    require(game.hp > 2 && game.hp <= game.max_hp && !game.weakened &&
            game.inventory[0].type == HEALING && game.inventory[0].amount == 1,
            "healing or potion stack is wrong");
    uint8_t strength = game.attack;
    drink(STRENGTH);
    require(game.attack == strength + 1, "strength potion did not increase attack");
    uint8_t dexterity = game.dexterity;
    drink(DEXTERITY);
    require(game.dexterity == dexterity + 1, "dexterity potion did not increase accuracy");
    drink(POISON);
    require(game.weakened, "poison did not weaken the player");
    strength = game.attack;
    drink(STRENGTH);
    require(!game.weakened && game.attack == strength,
            "strength potion did not restore weakening");
    drink(CONFUSION);
    require(game.confused, "confusion potion had no duration");
    drink(PARALYSIS);
    require(game.paralyzed && !use_inventory(0),
            "paralysis does not prevent item use");
    game.paralyzed = 0;
    drink(SLOWING);
    require(game.slowed, "slowing potion had no duration");
    drink(INVISIBILITY);
    require(game.invisible, "invisibility potion had no duration");
    uint8_t old_level = game.level;
    drink(EXPERIENCE);
    require(game.level > old_level, "experience potion did not grant levels");
    game.hp = game.max_hp;
    drink(HARMING);
    require(game.hp < game.max_hp && game.max_hp - game.hp <= 10,
            "harming potion dealt the wrong damage");
    require(potion_identified(HEALING),
            "identified potion was forgotten");
    Game saved = game;
    std::memset(&game, 0, sizeof(game));
    game = saved;
    require(potion_identified(HARMING) &&
            potion_color(HEALING) == saved.potion_appearance[0],
            "potion knowledge did not survive save state copy");

    bool spawned[POTION_COUNT] = {};
    uint8_t kinds = 0;
    for(uint16_t seed = 1; seed <= 24; ++seed) {
        start_new(seed);
        for(const GroundItem& item : game.ground)
            if(is_potion(item.type) && !spawned[item.type - HEALING]) {
                spawned[item.type - HEALING] = true;
                ++kinds;
            }
    }
    require(kinds >= 8, "floor generation lacks potion variety");
}

void check_thrown_potions()
{
    static_assert(sizeof(Monster) == 7, "monster effects exceed two bytes");
    start_new(0x3456);
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.door_count = 0;
    game.px = 10;
    game.py = 10;
    game.invisible = 100; // Keep the target in place during assertions.
    game.hunger = 255;

    game.inventory[0] = {HARMING, 2, {0, 0}};
    require(!throw_potion(0, 1, 1) && game.inventory[0].amount == 2,
            "invalid throwing direction consumed a potion");
    require(throw_potion(0, 1, 0) && game.inventory[0].amount == 1 &&
            !potion_identified(HARMING),
            "a missed throw did not consume one unknown potion");

    game.monsters[0] = {13, 10, ORC, 5, 0, {0, 0}};
    uint16_t wall = static_cast<uint16_t>(10 * MAP_W + 11);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.inventory[0] = {POISON, 3, {0, 0}};
    require(throw_potion(0, 1, 0) && !potion_identified(POISON) &&
            !monster_effect(game.monsters[0], MON_WEAKENED),
            "potion passed through a wall");
    game.walls[wall >> 3] = 0;
    game.door_count = 1;
    game.doors[0] = {11, 10};
    require(throw_potion(0, 1, 0) && !potion_identified(POISON),
            "potion passed through a closed door");
    mark(game.marks[game.floor], OPENED_DOORS, 0);
    game.monsters[1] = {14, 10, ORC, 5, 0, {0, 0}};
    require(throw_potion(0, 1, 0) && potion_identified(POISON) &&
            monster_effect(game.monsters[0], MON_WEAKENED) &&
            !monster_effect(game.monsters[1], MON_WEAKENED) &&
            game.inventory[0].type == NO_ITEM,
            "throw did not hit only the first monster or consume its stack");

    auto throw_at_target = [](uint8_t type) {
        game.inventory[0] = {type, 1, {0, 0}};
        require(throw_potion(0, 1, 0) && potion_identified(type),
                "thrown potion failed to identify on hit");
    };
    game.monsters[0].hp = 2;
    throw_at_target(HEALING);
    require(game.monsters[0].hp > 2 &&
            !monster_effect(game.monsters[0], MON_WEAKENED),
            "healing did not restore monster health and strength");
    throw_at_target(CONFUSION);
    require(monster_effect(game.monsters[0], MON_CONFUSED),
            "confusion did not affect the monster");
    throw_at_target(SLOWING);
    require(monster_effect(game.monsters[0], MON_SLOWED),
            "slowing did not affect the monster");
    throw_at_target(INVISIBILITY);
    require(monster_effect(game.monsters[0], MON_INVISIBLE),
            "invisibility did not affect the monster");
    Game saved = game;
    std::memset(&game, 0, sizeof(game));
    game = saved;
    require(monster_effect(game.monsters[0], MON_CONFUSED) &&
            monster_effect(game.monsters[0], MON_SLOWED) &&
            monster_effect(game.monsters[0], MON_INVISIBLE),
            "monster effects were not retained in the save state");
    throw_at_target(PARALYSIS);
    require(game.monsters[0].stun, "paralysis did not stun the monster");
    throw_at_target(POISON);
    throw_at_target(STRENGTH);
    require(!monster_effect(game.monsters[0], MON_WEAKENED),
            "strength did not cure monster weakness");
    uint8_t hp = game.monsters[0].hp;
    throw_at_target(DEXTERITY);
    throw_at_target(EXPERIENCE);
    require(game.monsters[0].hp == hp,
            "player-only potion changed monster health");
    for(uint8_t i = 0; i < 16; ++i)
        end_turn();
    require(!monster_effect(game.monsters[0], MON_CONFUSED) &&
            !monster_effect(game.monsters[0], MON_SLOWED) &&
            !monster_effect(game.monsters[0], MON_INVISIBLE),
            "monster potion effects did not expire");

    game.monsters[0].hp = 1;
    game.monsters[0].x = 13;
    game.monsters[0].y = 10;
    uint16_t old_score = game.score;
    throw_at_target(HARMING);
    require(game.monsters[0].type == NO_MONSTER &&
            marked(game.marks[game.floor], KILLED_MONSTERS, 0) &&
            game.score > old_score,
            "harming did not defeat and credit the monster");
}

void check_effect_messages()
{
    auto player_effect = [](uint8_t type, uint8_t Game::*duration,
                            const char* began, const char* ended) {
        start_new(0x4567);
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.inventory[0] = {type, 1, {0, 0}};
        status_text.clear();
        require(use_inventory(0) && status_text.find(began) != std::string::npos,
                "player effect start message is missing");
        game.*duration = 1;
        status_text.clear();
        end_turn();
        require(status_text.find(ended) != std::string::npos,
                "player effect end message is missing");
    };
    player_effect(CONFUSION, &Game::confused,
                  "You feel confused.", "You are no longer confused.");
    player_effect(PARALYSIS, &Game::paralyzed,
                  "You are paralyzed!", "You can move again.");
    player_effect(SLOWING, &Game::slowed,
                  "You feel sluggish.", "You move normally again.");
    player_effect(INVISIBILITY, &Game::invisible,
                  "You turn invisible.", "You become visible again.");

    start_new(0x4567);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.inventory[0] = {POISON, 1, {0, 0}};
    status_text.clear();
    require(use_inventory(0) &&
            status_text.find("You feel weaker.") != std::string::npos,
            "player poison start message is missing");
    game.inventory[0] = {HEALING, 1, {0, 0}};
    status_text.clear();
    require(use_inventory(0) &&
            status_text.find("Your strength returns.") != std::string::npos,
            "player weakness recovery message is missing");

    auto monster_effect_message = [](uint8_t type, const char* began,
                                     const char* ended) {
        start_new(0x5678);
        std::memset(game.walls, 0, sizeof(game.walls));
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.door_count = 0;
        game.px = 10;
        game.py = 10;
        game.invisible = 100;
        game.monsters[0] = {13, 10, ORC, 5, 0, {0, 0}};
        game.inventory[0] = {type, 1, {0, 0}};
        status_text.clear();
        require(throw_potion(0, 1, 0) &&
                status_text.find(began) != std::string::npos,
                "monster effect start message is missing");
        game.inventory[0] = {type, 1, {0, 0}};
        status_text.clear();
        require(throw_potion(0, 1, 0) &&
                status_text.find(began) == std::string::npos,
                "refreshing a monster effect repeated its start message");
        status_text.clear();
        for(uint8_t i = 0; i < 16; ++i)
            end_turn();
        require(status_text.find(ended) != std::string::npos,
                "monster effect end message is missing");
    };
    monster_effect_message(CONFUSION, "becomes confused.",
                           "is no longer confused.");
    monster_effect_message(PARALYSIS, "is paralyzed!",
                           "can move again.");
    monster_effect_message(SLOWING, "slows down.",
                           "moves normally again.");
    monster_effect_message(INVISIBILITY, "vanishes.",
                           "becomes visible again.");

    start_new(0x5678);
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.door_count = 0;
    game.px = 10;
    game.py = 10;
    game.invisible = 100;
    game.monsters[0] = {13, 10, ORC, 5, 0, {0, 0}};
    game.inventory[0] = {POISON, 1, {0, 0}};
    status_text.clear();
    require(throw_potion(0, 1, 0) &&
            status_text.find("grows weaker.") != std::string::npos,
            "monster poison start message is missing");
    game.inventory[0] = {STRENGTH, 1, {0, 0}};
    status_text.clear();
    require(throw_potion(0, 1, 0) &&
            status_text.find("regains its strength.") != std::string::npos,
            "monster weakness recovery message is missing");
}

int main()
{
    check_potions();
    check_thrown_potions();
    check_effect_messages();
    start_new(0x1234);
    require(game.floor == 0 && game.hp == 18 && game.valid &&
            game.version == SAVE_VERSION,
            "new game state is wrong");
    require(wall_at(-1, 0) && wall_at(MAP_W, 0),
            "map bounds are not solid");
    check_local_visibility();
    check_circular_light_radius();
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
