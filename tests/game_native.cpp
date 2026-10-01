#include "game.hpp"
#include "inventory_view.hpp"

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

void check_inventory_view()
{
    std::memset(game.inventory, 0, sizeof(game.inventory));
    InventoryView empty(game, nullptr);
    require(empty.count == 0 && empty.first_slot() == NONE,
            "empty inventory has selectable rows");

    game.inventory[0] = {HEALING, 2};
    game.inventory[1] = {ARMOR, 1};
    game.inventory[2] = {SWORD, 1};
    game.inventory[3] = {POISON, 1};
    game.inventory[4] = {RING_ATTACK, 1};
    game.inventory[5] = {FOOD, 1};
    game.inventory[6] = {SWORD, 2};
    game.inventory[7] = {AMULET_SPEED, 1};
    InventoryView view(game, nullptr);
    const uint8_t expected[] = {
        INVENTORY + WEAPONS, 2, 6,
        INVENTORY + ARMORS, 1,
        INVENTORY + RINGS, 4,
        INVENTORY + AMULETS, 7,
        INVENTORY + POTIONS, 0, 3,
        INVENTORY + FOODS, 5
    };
    require(view.count == sizeof(expected) &&
            std::memcmp(view.rows, expected, sizeof(expected)) == 0,
            "inventory rows are not grouped by type");
    require(view.first_slot() == 2 && view.move(6, 1) == 1 &&
            view.move(1, -1) == 6 && view.move(5, 1) == 5 &&
            view.move(2, -1) == 2,
            "inventory selection entered a header or passed the end");
    uint8_t top = 0;
    view.keep_visible(5, top);
    require(top == view.count - INVENTORY_VISIBLE_ROWS,
            "inventory scrolled beyond the last item");
    view.keep_visible(2, top);
    require(top == 1, "inventory did not scroll back to the first item");

    InventoryView potions(game, is_potion);
    require(potions.count == 3 && potions.first_slot() == 0 &&
            potions.move(0, 1) == 3 && potions.move(3, 1) == 3,
            "throw selection includes non-potions or headers");
    std::memset(game.inventory, 0, sizeof(game.inventory));
}

void check_stacked_ground_items()
{
    std::memset(game.ground, 0, sizeof(game.ground));
    game.ground[1] = {4, 5, {FOOD, 1}};
    game.ground[5] = {4, 5, {SWORD, 1}};
    game.ground[9] = {4, 5, {ARMOR, 1}};
    game.ground[12] = {6, 5, {HEALING, 1}};
    uint8_t top = ground_item_before(4, 5, GROUND_ITEMS);
    uint8_t middle = ground_item_before(4, 5, top);
    uint8_t bottom = ground_item_before(4, 5, middle);
    require(top == 9 && middle == 5 && bottom == 1 &&
            ground_item_before(4, 5, bottom) == NONE,
            "stacked items are not visited topmost first, once each");
    require(ground_item_before(6, 5, GROUND_ITEMS) == 12,
            "ground item scan includes a different tile");
    game.ground[5].item.type = NO_ITEM;
    require(ground_item_before(4, 5, top) == bottom,
            "ground item scan did not skip a picked-up item");
    std::memset(game.ground, 0, sizeof(game.ground));
}

void use_stairs(uint8_t floor, uint8_t x, uint8_t y)
{
    game.px = x;
    game.py = y;
    for(int i = 0; i < 4 && game.floor == floor; ++i)
        if(!take_stairs()) action();
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
        game.inventory[0] = {type, amount};
        require(use_inventory(0), "potion could not be drunk");
        require(potion_identified(type), "drinking did not identify potion");
    };
    game.hp = 2;
    game.weakened = 2;
    drink(HEALING, 2);
    require(game.hp > 2 && game.hp <= game.max_hp && !game.weakened &&
            game.inventory[0].type == HEALING &&
            item_value(game.inventory[0]) == 1,
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
            if(is_potion(item.item.type) &&
               !spawned[item.item.type - HEALING]) {
                spawned[item.item.type - HEALING] = true;
                ++kinds;
            }
    }
    require(kinds >= 8, "floor generation lacks potion variety");
}

void check_thrown_potions()
{
    static_assert(sizeof(Monster) == 8, "monster state must fit in one byte");
    start_new(0x3456);
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.door_count = 0;
    game.px = 10;
    game.py = 10;
    game.invisible = 100; // Keep the target in place during assertions.
    game.hunger = 255;

    game.inventory[0] = {HARMING, 2};
    require(!throw_potion(0, 1, 1) && item_value(game.inventory[0]) == 2,
            "invalid throwing direction consumed a potion");
    require(throw_potion(0, 1, 0) && item_value(game.inventory[0]) == 1 &&
            !potion_identified(HARMING),
            "a missed throw did not consume one unknown potion");

    game.monsters[0] = {13, 10, ORC, 5, 0, {0, 0}};
    uint16_t wall = static_cast<uint16_t>(10 * MAP_W + 11);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.inventory[0] = {POISON, 3};
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
        game.inventory[0] = {type, 1};
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
    for(uint8_t i = 0; i < 40; ++i)
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
        game.inventory[0] = {type, 1};
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
    game.inventory[0] = {POISON, 1};
    status_text.clear();
    require(use_inventory(0) &&
            status_text.find("You feel weaker.") != std::string::npos,
            "player poison start message is missing");
    game.inventory[0] = {HEALING, 1};
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
        game.inventory[0] = {type, 1};
        status_text.clear();
        require(throw_potion(0, 1, 0) &&
                status_text.find(began) != std::string::npos,
                "monster effect start message is missing");
        game.inventory[0] = {type, 1};
        status_text.clear();
        require(throw_potion(0, 1, 0) &&
                status_text.find(began) == std::string::npos,
                "refreshing a monster effect repeated its start message");
        status_text.clear();
        for(uint8_t i = 0; i < 40; ++i)
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
    game.inventory[0] = {POISON, 1};
    status_text.clear();
    require(throw_potion(0, 1, 0) &&
            status_text.find("grows weaker.") != std::string::npos,
            "monster poison start message is missing");
    game.inventory[0] = {STRENGTH, 1};
    status_text.clear();
    require(throw_potion(0, 1, 0) &&
            status_text.find("regains its strength.") != std::string::npos,
            "monster weakness recovery message is missing");
}

void check_enemy_roster()
{
    struct Expected { uint16_t flags; uint8_t str, dex, speed, def, hp, xp; };
    const Expected expected[] = {
        {0, 0, 0, 0, 0, 0, 0},
        {0, 1, 6, 8, 0, 1, 1},
        {MON_MEAN, 2, 3, 3, 0, 3, 2},
        {MON_MEAN | MON_POISON, 3, 3, 3, 0, 4, 3},
        {MON_MEAN | MON_OPENER, 4, 2, 2, 0, 6, 5},
        {MON_MEAN | MON_OPENER, 5, 4, 4, 1, 10, 6},
        {MON_MEAN | MON_NATURAL_INVIS | MON_OPENER | MON_SEE_INVIS,
            6, 4, 4, 1, 12, 7},
        {MON_MEAN | MON_OPENER, 7, 4, 4, 3, 16, 8},
        {MON_MEAN | MON_PARALYZE_HIT, 5, 4, 4, 0, 12, 9},
        {MON_MEAN | MON_OPENER, 8, 4, 4, 2, 20, 11},
        {MON_MEAN | MON_NOMOVE, 7, 4, 4, 3, 20, 11},
        {MON_MEAN | MON_CONFUSE_HIT | MON_OPENER | MON_SEE_INVIS,
            9, 4, 4, 3, 24, 14},
        {MON_MEAN | MON_REGENS | MON_OPENER, 10, 3, 3, 5, 32, 18},
        {MON_MEAN, 7, 6, 6, 1, 24, 18},
        {MON_MEAN | MON_FIRE_BREATH, 12, 4, 4, 8, 48, 25},
        {MON_MEAN | MON_CONFUSE_HIT | MON_PARALYZE_HIT |
             MON_OPENER | MON_SEE_INVIS, 10, 6, 6, 3, 24, 35},
        {MON_MEAN | MON_REGENS | MON_POISON | MON_CONFUSE_HIT |
             MON_PARALYZE_HIT | MON_SEE_INVIS, 16, 6, 8, 8, 128, 90}
    };
    for(uint8_t type = BAT; type <= LORD; ++type) {
        MonsterInfo info = monster_info(type);
        const Expected& e = expected[type];
        require(info.flags == e.flags && info.strength == e.str &&
                info.dexterity == e.dex && info.speed == e.speed &&
                info.defense == e.def && info.health == e.hp &&
                info.xp == e.xp, "enemy stats differ from ArduRogue");
    }
    const uint32_t floor_types[FLOORS] = {
        (1u << BAT) | (1u << SNAKE),
        (1u << SNAKE) | (1u << RATTLESNAKE),
        (1u << ZOMBIE) | (1u << GOBLIN) | (1u << PHANTOM),
        (1u << ZOMBIE) | (1u << GOBLIN) | (1u << PHANTOM) | (1u << ORC),
        1u << PHANTOM,
        (1u << GOBLIN) | (1u << ORC) | (1u << HOBGOBLIN),
        (1u << ORC) | (1u << HOBGOBLIN) | (1u << TARANTULA) | (1u << MIMIC),
        (1u << ORC) | (1u << HOBGOBLIN) | (1u << TARANTULA) | (1u << MIMIC),
        (1u << HOBGOBLIN) | (1u << TARANTULA) | (1u << MIMIC) | (1u << INCUBUS),
        (1u << MIMIC) | (1u << TARANTULA) | (1u << HOBGOBLIN),
        (1u << TARANTULA) | (1u << HOBGOBLIN) | (1u << MIMIC) |
            (1u << INCUBUS) | (1u << TROLL),
        (1u << HOBGOBLIN) | (1u << MIMIC) | (1u << INCUBUS) |
            (1u << TROLL) | (1u << GRIFFIN),
        (1u << MIMIC) | (1u << INCUBUS) | (1u << TROLL) |
            (1u << GRIFFIN) | (1u << DRAGON),
        (1u << INCUBUS) | (1u << TROLL) | (1u << GRIFFIN) | (1u << DRAGON),
        (1u << INCUBUS) | (1u << ANGEL) | (1u << DRAGON),
        (1u << INCUBUS) | (1u << ANGEL) | (1u << LORD)
    };
    for(uint8_t floor = 0; floor < FLOORS; ++floor) {
        uint32_t seen = 0;
        for(uint16_t seed = 1; seed <= 80; ++seed) {
            start_new(seed);
            game.floor = floor;
            make_floor();
            for(const Monster& monster : game.monsters) {
                require(monster.type > NO_MONSTER && monster.type <= LORD &&
                        (floor_types[floor] & (1u << monster.type)),
                        "enemy generated on the wrong floor");
                seen |= 1u << monster.type;
            }
        }
        require(seen == floor_types[floor], "floor is missing an enemy type");
    }
    start_new(0x1234);
    game.floor = 9;
    make_floor();
    uint8_t mimic = NONE;
    for(uint8_t i = 0; i < MONSTERS; ++i)
        if(game.monsters[i].type == MIMIC) {
            mimic = i;
            break;
        }
    require(mimic != NONE, "test floor has no mimic");
    Game before = game;
    mark(game.marks[game.floor], KILLED_MONSTERS, mimic);
    make_floor();
    for(uint8_t i = 0; i < MONSTERS; ++i)
        if(i != mimic)
            require(std::memcmp(&game.monsters[i], &before.monsters[i],
                                sizeof(Monster)) == 0,
                    "killing a mimic changed another spawn on revisit");
    require(std::memcmp(game.ground, before.ground,
                        sizeof(game.ground)) == 0,
            "killing a mimic changed item generation on revisit");
}

void check_enemy_abilities()
{
    auto arena = [](uint8_t type, uint8_t x) {
        start_new(0x84e2);
        std::memset(game.walls, 0, sizeof(game.walls));
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.door_count = 0;
        game.px = 10;
        game.py = 10;
        game.hp = game.max_hp = 240;
        game.hunger = 255;
        game.monsters[0] = {x, 10, type, monster_info(type).health,
                            0, {0, 0}, 0};
        status_text.clear();
    };
    arena(PHANTOM, 13);
    require(!player_can_see_monster(0), "phantom is naturally visible");
    game.inventory[0] = {RING_SEE_INVISIBLE, 1};
    game.ring_slots[0] = 0;
    require(player_can_see_monster(0), "see invisible ring cannot reveal phantom");

    arena(MIMIC, 11);
    for(int i = 0; i < 8; ++i)
        end_turn();
    require(game.monsters[0].x == 11 && !(game.monsters[0].state & MON_AGGRO),
            "unprovoked mimic moved");
    move_player(1, 0);
    require(game.monsters[0].state & MON_AGGRO, "attacked mimic did not wake");

    arena(GOBLIN, 12);
    game.doors[0] = {11, 10};
    game.door_count = 1;
    for(int i = 0; i < 8 && !door_open(0); ++i)
        end_turn();
    require(door_open(0), "door-opening enemy could not open a door");

    arena(TROLL, 18);
    game.invisible = 200;
    game.monsters[0].hp = 1;
    for(int i = 0; i < 80 && game.monsters[0].hp == 1; ++i)
        end_turn();
    require(game.monsters[0].hp > 1, "regenerating enemy did not heal");

    arena(DRAGON, 13);
    bool breathed = false;
    for(int i = 0; i < 80 && !breathed; ++i) {
        game.monsters[0].x = 13;
        game.monsters[0].y = 10;
        status_text.clear();
        end_turn();
        breathed = status_text.find("breathes fire!") != std::string::npos;
    }
    require(breathed && game.hp < 240, "dragon fire did not hurt the player");
    arena(DRAGON, 13);
    game.monsters[1] = {10, 11, GOBLIN, monster_info(GOBLIN).health,
                        100, {0, 0}, 0};
    breathed = false;
    for(int i = 0; i < 80 && !breathed; ++i) {
        game.monsters[0].x = 13;
        game.monsters[0].y = 10;
        status_text.clear();
        end_turn();
        breathed = status_text.find("breathes fire!") != std::string::npos;
    }
    require(breathed && (game.monsters[1].type == NO_MONSTER ||
            game.monsters[1].hp < monster_info(GOBLIN).health),
            "dragon fire did not splash a nearby enemy");
    arena(DRAGON, 13);
    uint16_t wall = static_cast<uint16_t>(10 * MAP_W + 12);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    for(int i = 0; i < 40; ++i) {
        game.monsters[0].x = 13;
        game.monsters[0].y = 10;
        status_text.clear();
        end_turn();
        require(status_text.find("breathes fire!") == std::string::npos,
                "dragon breathed through a wall");
    }
    arena(DRAGON, 13);
    game.inventory[0] = {RING_FIRE_IMMUNITY, 1};
    game.ring_slots[0] = 0;
    breathed = false;
    for(int i = 0; i < 80 && !breathed; ++i) {
        game.monsters[0].x = 13;
        game.monsters[0].y = 10;
        status_text.clear();
        end_turn();
        breathed = status_text.find("breathes fire!") != std::string::npos;
    }
    require(breathed && game.hp == 240, "fire immunity failed against dragon");
    arena(DRAGON, 13);
    game.inventory[0] = {RING_FIRE_IMMUNITY,
                         static_cast<uint8_t>(ITEM_CURSED | 1)};
    game.ring_slots[0] = 0;
    breathed = false;
    for(int i = 0; i < 80 && !breathed; ++i) {
        game.monsters[0].x = 13;
        game.monsters[0].y = 10;
        status_text.clear();
        end_turn();
        breathed = status_text.find("breathes fire!") != std::string::npos;
    }
    require(breathed && game.hp <= 224,
            "cursed fire ring did not amplify dragon breath");

    const uint8_t attackers[] = {RATTLESNAKE, TARANTULA, INCUBUS};
    for(uint8_t type : attackers) {
        arena(type, 11);
        bool affected = false;
        for(int i = 0; i < 120 && !affected; ++i) {
            game.hp = 240;
            game.monsters[0].x = 11;
            game.monsters[0].y = 10;
            end_turn();
            affected = type == RATTLESNAKE ? game.weakened != 0 :
                type == TARANTULA ? game.paralyzed != 0 : game.confused != 0;
        }
        require(affected, "enemy on-hit effect did not trigger");
    }
}

int main()
{
    check_inventory_view();
    check_stacked_ground_items();
    check_enemy_roster();
    check_enemy_abilities();
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
    mark(game.marks[game.floor], TAKEN_ITEMS, 0);
    game.ground[0].item.type = NO_ITEM;
    game.px = drop_x;
    game.py = drop_y;
    game.inventory[0] = {FOOD, 1};
    require(drop_inventory(0) && game.inventory[0].type == NO_ITEM,
            "inventory drop failed");
    require(item_at(drop_x, drop_y) != NONE,
            "dropped item is missing");
    game.hp = player_max_hp();
    action();
    require(item_at(drop_x, drop_y) != NONE,
            "A action still picks up a ground item");

    uint8_t down_x = game.down_x, down_y = game.down_y;
    use_stairs(0, down_x, down_y);
    require(game.floor == 1, "descent failed");
    game.px = game.up_x;
    game.py = game.up_y;
    game.hp = player_max_hp();
    action();
    require(game.floor == 1, "A action still takes the stairs");
    use_stairs(1, game.up_x, game.up_y);
    require(game.floor == 0, "ascent failed");
    require(std::memcmp(first_floor.data(), game.walls,
                        first_floor.size()) == 0,
            "floor changed on revisit");
    require(item_at(drop_x, drop_y) == NONE,
            "dropped item was unexpectedly restored across floors");

    game.has_amulet = 1;
    game.px = game.up_x;
    game.py = game.up_y;
    for(int i = 0; i < 4 && !session.ended; ++i)
        if(!take_stairs()) action();
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
