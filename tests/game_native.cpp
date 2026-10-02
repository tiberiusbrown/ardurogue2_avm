#include "game.hpp"
#include "game_internal.hpp"
#include "inventory_view.hpp"
#include "persistence.hpp"
#include "world.hpp"

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
void status_word(const char*, char) {}
void status(const char* words) { status_text += words; status_text += ' '; }
void status(const char* words, char punctuation) {
    status_text += words;
    if(punctuation) status_text += punctuation;
    status_text += ' ';
}
void status(Item) {}
void status(Item, char) {}
void status(MonsterType) {}
void status(MonsterType, char) {}
void status_number(uint8_t) {}
void status_number(uint8_t, char) {}
}

void require(bool condition, const char* reason)
{
    if(!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(1);
    }
}

void check_position_value_and_boundaries()
{
    constexpr Position origin{1, 2};
    static_assert(origin.x == 1 && origin.y == 2, "position axes changed");
    static_assert(origin == Position{1, 2} && origin != Position{2, 1},
                  "position equality changed");
    static_assert(sizeof(Position) == 2 && sizeof(Door) == 2 &&
                  sizeof(Monster) == 8 && sizeof(GroundItem) == 4 &&
                  sizeof(Game) == 932,
                  "saved entity layout changed");
    const Position corners[] = {{0, 0}, {MAP_W - 1, 0},
                                {0, MAP_H - 1}, {MAP_W - 1, MAP_H - 1}};
    const uint8_t bytes[] = {1, 2};
    require(std::memcmp(&origin, bytes, sizeof bytes) == 0 &&
            Position{1, 2} != Position{2, 2} &&
            Position{1, 2} != Position{1, 3},
            "position byte order or equality changed");

    Game saved = game;
    Session saved_session = session;
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.explored, 0, sizeof game.explored);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.doors, 0, sizeof game.doors);
    std::memset(game.ground, 0, sizeof game.ground);
    game.door_count = 0;
    game.paralyzed = game.confused = 0;
    session.ended = false;
    for(Position pos : corners) {
        game.player = pos;
        explore(pos);
        require(explored(pos) && can_see(pos),
                "boundary tile exploration or visibility changed");
    }
    game.monsters[0] = {corners[0], BAT, 1, 0, {0, 0}, 0};
    game.doors[0] = {corners[1]};
    game.door_count = 1;
    game.ground[0] = {corners[2], {FOOD, 1}};
    require(monster_at(corners[0]) == 0 &&
            monster_at(corners[1]) == NONE &&
            door_at(corners[1]) == 0 && door_at(corners[0]) == NONE &&
            item_at(corners[2]) == 0 && item_at(corners[3]) == NONE &&
            ground_item_before(corners[2], GROUND_ITEMS) == 0,
            "boundary occupancy lookup changed");
    std::memset(game.monsters, 0, sizeof game.monsters);
    game.door_count = 0;
    for(Position pos : corners) {
        game.player = pos;
        move_player(pos.x == 0 ? -1 : 1, 0);
        require(game.player == pos, "horizontal map edge changed");
        move_player(0, pos.y == 0 ? -1 : 1);
        require(game.player == pos, "vertical map edge changed");
    }
    game = saved;
    session = saved_session;
}

void check_startup_save_state()
{
    std::memset(&game, 0x5a, sizeof(game));
    game.magic = SAVE_MAGIC;
    game.version = SAVE_VERSION;
    game.valid = 0;
    game.best_score = 321;
    require(!restore_startup_save(true) && game.best_score == 321 &&
            game.magic == SAVE_MAGIC && game.version == SAVE_VERSION,
            "compatible completed save lost its best score");

    game.valid = 1;
    require(restore_startup_save(true),
            "compatible active save was not offered as a continue");

    game.magic = 0;
    require(!restore_startup_save(true) && game.best_score == 0,
            "incompatible save was not cleared");

    game.magic = SAVE_MAGIC;
    game.version = SAVE_VERSION - 1;
    game.valid = 1;
    game.best_score = 321;
    require(!restore_startup_save(true) && game.best_score == 0,
            "old save version was not cleared");

    game.magic = SAVE_MAGIC;
    game.version = SAVE_VERSION;
    game.best_score = 321;
    require(!restore_startup_save(false) && game.best_score == 0,
            "missing save was not cleared");
}

void check_new_run_state()
{
    game.best_score = 321;
    game.score = 70;
    game.hp = 1;
    game.inventory[0] = {SWORD, 3};
    session = {0, DEATH, true};
    start_new(0x1234);
    require(game.best_score == 321 && game.score == 0 &&
            game.hp == 18 && game.inventory[0].type == NO_ITEM &&
            !session.ended && session.repeat_slot == NONE,
            "new run did not preserve best score while resetting run state");

    game.max_hp = 20;
    game.vamp_drain = 3;
    game.inventory[0] = {AMULET_VITALITY, 2};
    game.amulet_slot = 0;
    require(player_max_hp() == 27, "vitality bonus changed");
    game.inventory[0].info |= ITEM_CURSED;
    require(player_max_hp() == 7, "cursed vitality penalty changed");
    game.vamp_drain = 255;
    require(player_max_hp() == 1, "minimum player health changed");
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
    game.ground[1] = {{4, 5}, {FOOD, 1}};
    game.ground[5] = {{4, 5}, {SWORD, 1}};
    game.ground[9] = {{4, 5}, {ARMOR, 1}};
    game.ground[12] = {{6, 5}, {HEALING, 1}};
    uint8_t top = ground_item_before({4, 5}, GROUND_ITEMS);
    uint8_t middle = ground_item_before({4, 5}, top);
    uint8_t bottom = ground_item_before({4, 5}, middle);
    require(top == 9 && middle == 5 && bottom == 1 &&
            ground_item_before({4, 5}, bottom) == NONE,
            "stacked items are not visited topmost first, once each");
    require(ground_item_before({6, 5}, GROUND_ITEMS) == 12,
            "ground item scan includes a different tile");
    game.ground[5].item.type = NO_ITEM;
    require(ground_item_before({4, 5}, top) == bottom,
            "ground item scan did not skip a picked-up item");
    std::memset(game.ground, 0, sizeof(game.ground));
}

void use_stairs(uint8_t floor, Position pos)
{
    game.player = pos;
    for(int i = 0; i < 4 && game.floor == floor; ++i)
        if(!take_stairs()) action();
    require(game.floor != floor, "stairs did not change floors");
}

void check_local_visibility()
{
    uint8_t old_x = game.player.x, old_y = game.player.y;
    unsigned samples = 0;
    for(uint8_t y = 0; y < MAP_H && samples < 24; ++y)
        for(uint8_t x = 0; x < MAP_W && samples < 24; ++x) {
            if(wall_at(x, y) || (x + y * MAP_W) % 11 != 0)
                continue;
            game.player = {x, y};
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
                                can_see({static_cast<uint8_t>(tx), static_cast<uint8_t>(ty)}),
                                "local visibility differs from world ray");
                }
        }
    require(samples == 24, "too few visibility samples");
    game.player = {old_x, old_y};
}

void check_circular_light_radius()
{
    std::array<uint8_t, sizeof(game.walls)> saved_walls;
    std::memcpy(saved_walls.data(), game.walls, saved_walls.size());
    uint8_t old_x = game.player.x, old_y = game.player.y;
    uint8_t old_door_count = game.door_count;
    std::memset(game.walls, 0, sizeof(game.walls));
    game.door_count = 0;
    game.player = {20, 15};
    uint16_t opaque[13] = {};
    require(ray_visible(12, 6, opaque) && can_see({26, 15}),
            "cardinal tile at the light radius is hidden");
    require(ray_visible(11, 9, opaque) && can_see({25, 18}),
            "diagonal tile inside the light radius is hidden");
    require(!ray_visible(12, 7, opaque) && !can_see({26, 16}),
            "tile outside the light radius is visible");
    require(!ray_visible(11, 11, opaque) && !can_see({25, 20}),
            "diagonal tile outside the light radius is visible");
    std::memcpy(game.walls, saved_walls.data(), saved_walls.size());
    game.door_count = old_door_count;
    game.player = {old_x, old_y};
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
    explore({10, 10});
    require(explored({10, 10}), "explored tile was not recorded");
    require(!explored({11, 10}) && !explored({10, 11}) && !explored({11, 11}),
            "exploration leaked into adjacent tiles");
    explore({MAP_W - 1, MAP_H - 1});
    require(explored({MAP_W - 1, MAP_H - 1}), "last tile was not recorded");
    require(!explored({MAP_W - 2, MAP_H - 1}),
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

void check_confused_wall_bump()
{
    start_new(0x1234);
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.door_count = 0;
    game.player = {10, 10};
    uint16_t wall = static_cast<uint16_t>(9 * MAP_W + 10);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));

    uint8_t turns = game.turns;
    move_player(0, -1);
    require(game.turns == turns, "normal wall bump consumed a turn");

    uint16_t redirected_seed = 0, normal_seed = 0;
    for(uint16_t seed = 1; seed < 1024; ++seed) {
        uint16_t state = seed;
        if(next_random(state) % 2) {
            normal_seed = seed;
        } else if(next_random(state) % 4 == 3) {
            redirected_seed = seed;
        }
        if(redirected_seed && normal_seed) break;
    }
    require(redirected_seed && normal_seed,
            "could not find confusion direction test seeds");

    game.confused = 3;
    game.random_state = normal_seed;
    move_player(0, -1);
    require(game.turns == turns && game.confused == 3,
            "unmodified wall bump while confused consumed a turn");

    game.random_state = redirected_seed;
    move_player(1, 0);
    require(game.turns == static_cast<uint8_t>(turns + 1) &&
            game.confused == 2 && game.player == Position{10, 10},
            "confusion-generated wall bump did not consume a turn");
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
    for(uint8_t i = 0; i < POTION_COUNT; ++i)
        original[i] = potion_color(static_cast<uint8_t>(HEALING + i));
    start_new(0x1234);
    for(uint8_t i = 0; i < POTION_COUNT; ++i)
        require(original[i] == potion_color(static_cast<uint8_t>(HEALING + i)),
                "same run seed changed potion names");
    start_new(0x4321);
    bool different = false;
    for(uint8_t i = 0; i < POTION_COUNT; ++i)
        if(original[i] != potion_color(static_cast<uint8_t>(HEALING + i)))
            different = true;
    require(different, "different runs share the same potion names");

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
    uint8_t healing_appearance = potion_color(HEALING);
    Game saved = game;
    std::memset(&game, 0, sizeof(game));
    game = saved;
    require(potion_identified(HARMING) &&
            potion_color(HEALING) == healing_appearance,
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

void check_repeat_inventory_action()
{
    start_new(0x1234);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.inventory[1] = {SWORD, 1};
    require(use_inventory(1) && session.repeat_slot == 1,
            "equipping a weapon did not record the repeat action");

    game.inventory[0] = {FOOD, 2};
    require(use_inventory(0) && session.repeat_slot == 1 &&
            item_value(game.inventory[0]) == 1,
            "eating food changed the previous repeat action");
    action();
    require(item_value(game.inventory[0]) == 1,
            "repeat action ate another food");
    require(use_inventory(0) && game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == 1,
            "eating the last food changed the previous repeat action");

    game.ground[0] = {{game.player.x, game.player.y}, {HEALING, 2}};
    take_item(0);
    require(game.inventory[0].type == HEALING && session.repeat_slot == 1,
            "pickup did not reuse the consumed food slot");
    action();
    require(item_value(game.inventory[0]) == 2,
            "repeat action drank an unrelated potion after slot reuse");
    require(use_inventory(0) && session.repeat_slot == 1 &&
            item_value(game.inventory[0]) == 1,
            "drinking a potion changed the previous repeat action");
    action();
    require(item_value(game.inventory[0]) == 1,
            "repeat action drank another potion");
    require(use_inventory(0) && game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == 1,
            "drinking the last potion changed the previous repeat action");

    game.inventory[0] = {SCROLL_MAPPING, 1};
    require(use_inventory(0) && game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == 1,
            "reading a scroll changed the previous repeat action");

    start_new(0x1234);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.inventory[0] = {FOOD, 1};
    require(use_inventory(0) && session.repeat_slot == NONE,
            "food recorded a repeat action when none existed");
    game.inventory[0] = {HEALING, 1};
    require(use_inventory(0) && session.repeat_slot == NONE,
            "potion recorded a repeat action when none existed");
    game.inventory[0] = {SCROLL_MAPPING, 1};
    require(use_inventory(0) && session.repeat_slot == NONE,
            "scroll recorded a repeat action when none existed");
}

void reset_item_fixture()
{
    start_new(0x2468);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    std::memset(game.ground, 0, sizeof(game.ground));
    std::memset(game.inventory, 0, sizeof(game.inventory));
    std::memset(&game.marks[game.floor], 0, sizeof(FloorMarks));
    game.player = {3, 4};
    game.weapon_slot = game.armor_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    game.defense = 0;
    game.hp = game.max_hp;
    game.hunger = 255;
    status_text.clear();
}

void fill_item_inventory()
{
    for(Item& item : game.inventory) item = {SWORD, 1};
}

void check_ground_item_exchange()
{
    reset_item_fixture();
    game.ground[5] = {{3, 4}, {ARMOR, 2}};
    uint8_t old_turn = game.turns;
    require(take_item(5) == PICKUP_TAKEN &&
            game.inventory[0].type == ARMOR &&
            game.ground[5].item.type == NO_ITEM &&
            game.turns == static_cast<uint8_t>(old_turn + 1) &&
            marked(game.marks[0], TAKEN_ITEMS, 5),
            "ordinary pickup failed");

    reset_item_fixture();
    game.inventory[0] = {HEALING, 60};
    game.ground[5] = {{3, 4}, {HEALING, static_cast<uint8_t>(3 | ITEM_IDENTIFIED)}};
    require(take_item(5) == PICKUP_TAKEN &&
            item_value(game.inventory[0]) == 63 &&
            item_is_identified(game.inventory[0]),
            "inventory stack did not fully merge");

    reset_item_fixture();
    game.inventory[0] = {HEALING, static_cast<uint8_t>(60 | ITEM_CURSED)};
    game.ground[5] = {{3, 4}, {HEALING,
                            static_cast<uint8_t>(3 | ITEM_IDENTIFIED)}};
    require(take_item(5) == PICKUP_TAKEN &&
            game.inventory[0].info ==
                static_cast<uint8_t>(63 | ITEM_IDENTIFIED),
            "stack merge leaked a curse bit");

    reset_item_fixture();
    fill_item_inventory();
    game.inventory[0] = {FOOD, 60};
    game.inventory[1] = {FOOD, 59};
    game.ground[5] = {{3, 4}, {FOOD, 7}};
    require(take_item(5) == PICKUP_TAKEN &&
            item_value(game.inventory[0]) == 63 &&
            item_value(game.inventory[1]) == 63,
            "pickup did not span compatible stacks");

    reset_item_fixture();
    game.inventory[0] = {SCROLL_MAPPING, 60};
    game.ground[5] = {{3, 4}, {SCROLL_MAPPING, 8}};
    require(take_item(5) == PICKUP_TAKEN &&
            item_value(game.inventory[0]) == 63 &&
            game.inventory[1].type == SCROLL_MAPPING &&
            item_value(game.inventory[1]) == 5,
            "pickup did not put remainder in an empty slot");

    reset_item_fixture();
    fill_item_inventory();
    game.inventory[0] = {FOOD, 62};
    game.ground[5] = {{3, 4}, {FOOD, 2}};
    Game unchanged = game;
    Session unchanged_session = session;
    require(take_item(5) == PICKUP_NEEDS_SWAP &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0 &&
            std::memcmp(&session, &unchanged_session, sizeof(session)) == 0,
            "failed pickup partially changed game state");

    reset_item_fixture();
    fill_item_inventory();
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        game.ground[i] = {{3, 4}, {ARMOR, 1}};
    game.ground[5].item = {FOOD, 3};
    game.inventory[2] = {SWORD, 4};
    game.weapon_slot = 2;
    session.repeat_slot = 2;
    old_turn = game.turns;
    require(take_item(5) == PICKUP_NEEDS_SWAP &&
            game.turns == old_turn &&
            swap_ground_item(5, 2) &&
            game.inventory[2].type == FOOD &&
            game.ground[5].item.type == SWORD &&
            game.ground[5].pos == Position{3, 4} &&
            game.weapon_slot == NONE && session.repeat_slot == NONE &&
            game.turns == static_cast<uint8_t>(old_turn + 1) &&
            marked(game.marks[0], TAKEN_ITEMS, 5) &&
            ground_item_before({3, 4}, 6) == 5 &&
            ground_item_before({3, 4}, 5) == 4,
            "full-table swap did not preserve the exact ground slot");

    reset_item_fixture();
    fill_item_inventory();
    uint8_t generated = NONE;
    make_floor();
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type != NO_ITEM) { generated = i; break; }
    require(generated != NONE, "floor had no generated item to test");
    require(swap_ground_item(generated, 0), "generated item swap failed");
    make_floor();
    require(game.ground[generated].item.type == NO_ITEM &&
            marked(game.marks[0], TAKEN_ITEMS, generated),
            "swapped generated item respawned on floor reconstruction");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {SWORD, 1}};
    game.inventory[3] = {ARMOR, 5};
    game.armor_slot = 3;
    game.defense = 5;
    require(swap_ground_item(7, 3) && game.armor_slot == NONE &&
            game.defense == 0, "armor swap left its defense equipped");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {SWORD, 1}};
    game.inventory[3] = {RING_ATTACK, 2};
    game.ring_slots[1] = 3;
    require(swap_ground_item(7, 3) && game.ring_slots[1] == NONE,
            "ring swap left the ring equipped");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {SWORD, 1}};
    game.inventory[3] = {AMULET_VITALITY, 2};
    game.amulet_slot = 3;
    game.hp = player_max_hp();
    require(swap_ground_item(7, 3) && game.amulet_slot == NONE &&
            game.hp == player_max_hp(),
            "amulet swap did not clamp health");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {SWORD, 1}};
    game.inventory[3] = {RING_INVISIBILITY,
                         static_cast<uint8_t>(1 | ITEM_CURSED)};
    game.ring_slots[0] = 3;
    unchanged = game;
    require(!swap_ground_item(7, 3) &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0,
            "cursed equipped item could be swapped");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[15] = {{3, 4}, {YENDOR_AMULET, 1}};
    require(take_item(15) == PICKUP_TAKEN && game.has_amulet &&
            game.ground[15].item.type == NO_ITEM &&
            marked(game.marks[0], TAKEN_ITEMS, 15),
            "Yendor pickup required inventory space");

    reset_item_fixture();
    session.repeat_slot = INVENTORY;
    action();
    require(session.repeat_slot == NONE,
            "out-of-range repeat slot was not cleared");
}

void check_ground_item_drop()
{
    reset_item_fixture();
    game.inventory[0] = {SWORD, 2};
    game.ground[0].item.type = NO_ITEM;
    game.ground[1] = {{3, 4}, {ARMOR, 1}};
    require(drop_disposition(0) == DROP_DISCARD_ALL &&
            !drop_inventory(0) && game.inventory[0].type == SWORD,
            "unmarked empty ground slot was reused");
    require(!marked(game.marks[0], TAKEN_ITEMS, 15),
            "reserved ground slot was unexpectedly marked reusable");
    mark(game.marks[0], TAKEN_ITEMS, 0);
    session.repeat_slot = 0;
    uint8_t old_turn = game.turns;
    require(drop_inventory(0) && game.ground[0].item.type == SWORD &&
            game.ground[1].item.type == ARMOR &&
            game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == NONE &&
            game.turns == static_cast<uint8_t>(old_turn + 1),
            "drop onto occupied tile did not use marked slot");

    reset_item_fixture();
    game.inventory[0] = {FOOD, 7};
    game.ground[2] = {{3, 4}, {FOOD, 60}};
    game.ground[3] = {{3, 4}, {FOOD, 60}};
    require(drop_disposition(0) == DROP_DISCARD_REST &&
            !drop_inventory(0) && item_value(game.ground[2].item) == 60 &&
            item_value(game.inventory[0]) == 7,
            "partial drop changed state before discard confirmation");
    require(drop_inventory(0, true) &&
            item_value(game.ground[2].item) == 63 &&
            item_value(game.ground[3].item) == 63 &&
            game.inventory[0].type == NO_ITEM &&
            status_text.find("The rest is discarded.") != std::string::npos,
            "partial discard did not fill ground stacks");

    reset_item_fixture();
    game.inventory[0] = {HEALING, 7};
    game.ground[2] = {{3, 4}, {HEALING, 60}};
    mark(game.marks[0], TAKEN_ITEMS, 4);
    require(drop_inventory(0) && item_value(game.ground[2].item) == 63 &&
            game.ground[4].item.type == HEALING &&
            item_value(game.ground[4].item) == 4,
            "drop remainder did not use a reusable slot");

    reset_item_fixture();
    game.inventory[0] = {FOOD, 3};
    game.ground[2] = {{3, 4}, {FOOD, 60}};
    require(drop_inventory(0) && item_value(game.ground[2].item) == 63 &&
            game.inventory[0].type == NO_ITEM,
            "drop did not fully merge into a ground stack");

    reset_item_fixture();
    game.inventory[0] = {SWORD, 1};
    session.repeat_slot = 0;
    Game unchanged = game;
    require(drop_disposition(0) == DROP_DISCARD_ALL &&
            !drop_inventory(0) &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0,
            "declined discard mutated game state");
    require(drop_inventory(0, true) &&
            game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == NONE &&
            status_text.find("You discard") != std::string::npos,
            "confirmed full discard failed");

    reset_item_fixture();
    game.inventory[0] = {ARMOR, static_cast<uint8_t>(3 | ITEM_CURSED)};
    game.armor_slot = 0;
    game.defense = 3;
    mark(game.marks[0], TAKEN_ITEMS, 4);
    unchanged = game;
    require(drop_disposition(0) == DROP_INVALID &&
            !drop_inventory(0) && !drop_inventory(0, true) &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0,
            "cursed equipped armor was removed");

    reset_item_fixture();
    game.inventory[0] = {ARMOR, 3};
    game.armor_slot = 0;
    game.defense = 3;
    mark(game.marks[0], TAKEN_ITEMS, 4);
    require(drop_inventory(0) && game.armor_slot == NONE &&
            game.defense == 0 && game.ground[4].item.type == ARMOR,
            "uncursed equipped armor did not drop cleanly");

    reset_item_fixture();
    game.inventory[0] = {AMULET_VITALITY, 2};
    game.amulet_slot = 0;
    game.hp = player_max_hp();
    require(drop_inventory(0, true) && game.amulet_slot == NONE &&
            game.hp == player_max_hp(),
            "uncursed equipped amulet did not discard cleanly");

    reset_item_fixture();
    game.inventory[0] = {RING_INVISIBILITY, 1};
    game.ring_slots[0] = 0;
    require(player_is_invisible() && drop_inventory(0, true) &&
            !player_is_invisible() &&
            status_text.find("You become visible again.") != std::string::npos,
            "discarding invisibility ring did not announce visibility");

    reset_item_fixture();
    game.inventory[0] = {YENDOR_AMULET, 1};
    unchanged = game;
    require(drop_disposition(0) == DROP_INVALID &&
            !drop_inventory(0, true) &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0,
            "Yendor amulet could be discarded");
}

void check_scrolls_and_identification()
{
    start_new(0x2468);
    auto permutation = [](uint8_t first, uint8_t count) {
        bool seen[POTION_COUNT] = {};
        for(uint8_t i = 0; i < count; ++i) {
            uint8_t type = static_cast<uint8_t>(first + i);
            uint8_t appearance = item_appearance(type);
            require(appearance < count && !seen[appearance] &&
                    !item_type_identified(type),
                    "new appearance table is not an unknown permutation");
            seen[appearance] = true;
        }
    };
    uint16_t random_state = game.random_state;
    for(uint16_t seed = 1; seed <= 64; ++seed) {
        game.run_seed = seed;
        permutation(HEALING, POTION_COUNT);
        permutation(SCROLL_IDENTIFY, SCROLL_COUNT);
        permutation(RING_SEE_INVISIBLE, RING_COUNT);
        permutation(AMULET_SPEED, AMULET_COUNT);
        require(game.random_state == random_state,
                "appearance lookup advanced gameplay randomness");
    }
    game.run_seed = 0x2468;
    uint8_t first_scroll[SCROLL_COUNT];
    for(uint8_t i = 0; i < SCROLL_COUNT; ++i)
        first_scroll[i] = item_appearance(static_cast<uint8_t>(SCROLL_IDENTIFY + i));
    start_new(0x2468);
    for(uint8_t i = 0; i < SCROLL_COUNT; ++i)
        require(first_scroll[i] ==
                    item_appearance(static_cast<uint8_t>(SCROLL_IDENTIFY + i)),
                "scroll appearances change for the same run seed");
    start_new(0x2469);
    bool different = false;
    for(uint8_t i = 0; i < SCROLL_COUNT; ++i)
        if(first_scroll[i] !=
           item_appearance(static_cast<uint8_t>(SCROLL_IDENTIFY + i)))
            different = true;
    require(different, "scroll appearances do not change between runs");
    for(const GroundItem& ground : game.ground)
        if(ground.item.type)
            require(!item_is_identified(ground.item),
                    "generated item starts identified");

    std::memset(game.monsters, 0, sizeof(game.monsters));
    std::memset(game.inventory, 0, sizeof(game.inventory));
    game.hunger = 255;
    game.invisible = 100;
    game.inventory[0] = {SCROLL_IDENTIFY, 2};
    game.inventory[1] = {SWORD, 3};
    require(use_inventory(0, 1) && item_type_identified(SCROLL_IDENTIFY) &&
            item_is_identified(game.inventory[1]) &&
            item_value(game.inventory[0]) == 1,
            "identify scroll did not reveal target or consume one scroll");
    game.inventory[0] = {SCROLL_ENCHANT, 1};
    require(use_inventory(0, 1) && item_value(game.inventory[1]) == 4 &&
            game.inventory[0].type == NO_ITEM,
            "enchant scroll did not improve target or get consumed");
    game.inventory[0] = {SCROLL_REMOVE_CURSE, 1};
    game.inventory[1].info |= ITEM_CURSED;
    require(use_inventory(0, 1) && !item_is_cursed(game.inventory[1]),
            "remove curse scroll left target cursed");
    game.inventory[0] = {SCROLL_MAPPING, 1};
    require(use_inventory(0) && game.explored[0] == 0xff &&
            game.explored[sizeof(game.explored) - 1] == 0xff,
            "mapping scroll did not reveal the floor");

    std::memset(game.walls, 0, sizeof(game.walls));
    game.door_count = 0;
    game.monsters[0] = {{static_cast<uint8_t>(game.player.x + 1), game.player.y},
                        ORC, 12, 0, {0, 0}, MON_AGGRO};
    game.inventory[0] = {SCROLL_MASS_CONFUSE, 1};
    require(use_inventory(0) &&
            monster_effect(game.monsters[0], MON_CONFUSED),
            "mass confusion scroll did not affect a visible monster");
    game.inventory[0] = {SCROLL_MASS_POISON, 1};
    require(use_inventory(0) &&
            monster_effect(game.monsters[0], MON_WEAKENED),
            "mass poison scroll did not affect a visible monster");
    game.inventory[0] = {SCROLL_FEAR, 1};
    require(use_inventory(0) && (game.monsters[0].state & MON_AFRAID),
            "fear scroll did not scare a visible monster");
    game.inventory[0] = {SCROLL_TORMENT, 1};
    uint8_t hp = game.monsters[0].hp;
    require(use_inventory(0) && game.monsters[0].hp <= hp / 2 + 1,
            "torment scroll did not damage a visible monster");
    game.inventory[0] = {SCROLL_TELEPORT, 1};
    require(use_inventory(0) && !wall_at(game.player.x, game.player.y) &&
            monster_at({game.player.x, game.player.y}) == NONE,
            "teleport scroll placed player on an invalid tile");
}

void check_mapping_persists_rooms()
{
    start_new(0x2468);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.inventory[0] = {SCROLL_MAPPING, 1};
    require(use_inventory(0), "mapping scroll could not be read");
    for(uint8_t i = 0; i < ROOMS; ++i)
        require(marked(game.marks[0], VISITED_ROOMS, i),
                "mapping scroll did not mark every room visited");

    make_floor();
    for(const Room& room : game.rooms)
        require(explored({static_cast<uint8_t>(room.x + 1), static_cast<uint8_t>(room.y + 1)}),
                "mapped room was forgotten after rebuilding the floor");
}

void check_teleport_avoids_prompts()
{
    uint16_t seed_for_destination = 0;
    uint8_t candidate_x = 0, candidate_y = 0;
    for(uint16_t seed = 1; seed < 1024; ++seed) {
        uint16_t state = seed;
        uint8_t x = static_cast<uint8_t>(next_random(state) % MAP_W);
        uint8_t y = static_cast<uint8_t>(next_random(state) % MAP_H);
        if(x > 2 && y > 2) {
            seed_for_destination = seed;
            candidate_x = x;
            candidate_y = y;
            break;
        }
    }
    require(seed_for_destination, "could not find a teleport test destination");

    for(uint8_t excluded = 0; excluded < 3; ++excluded) {
        start_new(0x2468);
        std::memset(game.walls, 0, sizeof(game.walls));
        std::memset(game.monsters, 0, sizeof(game.monsters));
        std::memset(game.ground, 0, sizeof(game.ground));
        game.door_count = 0;
        game.player = {2, 2};
        game.up = {0, 0};
        game.down = {1, 0};
        if(excluded == 0)
            game.ground[0] = {{candidate_x, candidate_y}, {FOOD, 1}};
        else if(excluded == 1) {
            game.up = {candidate_x, candidate_y};
        } else {
            game.down = {candidate_x, candidate_y};
        }
        game.random_state = seed_for_destination;
        game.inventory[0] = {SCROLL_TELEPORT, 1};
        status_text.clear();
        require(use_inventory(0) &&
                status_text.find("You teleport!") != std::string::npos &&
                game.player != Position{candidate_x, candidate_y} &&
                item_at({game.player.x, game.player.y}) == NONE &&
                game.player != game.up && game.player != game.down,
                "teleport landed on an item or stair");
    }
}

void check_thrown_potions()
{
    static_assert(sizeof(Monster) == 8, "monster state must fit in one byte");
    start_new(0x3456);
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.door_count = 0;
    game.player = {10, 10};
    game.invisible = 100; // Keep the target in place during assertions.
    game.hunger = 255;

    game.inventory[0] = {HARMING, 2};
    require(!throw_potion(0, 1, 1) && item_value(game.inventory[0]) == 2,
            "invalid throwing direction consumed a potion");
    require(throw_potion(0, 1, 0) && item_value(game.inventory[0]) == 1 &&
            !potion_identified(HARMING),
            "a missed throw did not consume one unknown potion");

    game.monsters[0] = {{13, 10}, ORC, 5, 0, {0, 0}};
    uint16_t wall = static_cast<uint16_t>(10 * MAP_W + 11);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.inventory[0] = {POISON, 3};
    require(throw_potion(0, 1, 0) && !potion_identified(POISON) &&
            !monster_effect(game.monsters[0], MON_WEAKENED),
            "potion passed through a wall");
    game.walls[wall >> 3] = 0;
    game.door_count = 1;
    game.doors[0] = {{11, 10}};
    require(throw_potion(0, 1, 0) && !potion_identified(POISON),
            "potion passed through a closed door");
    mark(game.marks[game.floor], OPENED_DOORS, 0);
    game.monsters[0].pos = {13, 10};
    game.monsters[1] = {{14, 10}, ORC, 5, 0, {0, 0}};
    require(throw_potion(0, 1, 0) && potion_identified(POISON) &&
            monster_effect(game.monsters[0], MON_WEAKENED) &&
            !monster_effect(game.monsters[1], MON_WEAKENED) &&
            game.inventory[0].type == NO_ITEM,
            "throw did not hit only the first monster or consume its stack");

    auto throw_at_target = [](uint8_t type) {
        game.monsters[0].pos = {13, 10};
        game.monsters[1].type = NO_MONSTER;
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
    game.monsters[0].pos = {13, 10};
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
        game.player = {10, 10};
        game.invisible = 100;
        game.monsters[0] = {{13, 10}, ORC, 5, 0, {0, 0}};
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
    game.player = {10, 10};
    game.invisible = 100;
    game.monsters[0] = {{13, 10}, ORC, 5, 0, {0, 0}};
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
        game.player = {10, 10};
        game.hp = game.max_hp = 240;
        game.hunger = 255;
        game.monsters[0] = {{x, 10}, type, monster_info(type).health,
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
    require(game.monsters[0].pos.x == 11 && !(game.monsters[0].state & MON_AGGRO),
            "unprovoked mimic moved");
    move_player(1, 0);
    require(game.monsters[0].state & MON_AGGRO, "attacked mimic did not wake");

    arena(GOBLIN, 12);
    game.doors[0] = {{11, 10}};
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
        game.monsters[0].pos = {13, 10};
        status_text.clear();
        end_turn();
        breathed = status_text.find("breathes fire!") != std::string::npos;
    }
    require(breathed && game.hp < 240, "dragon fire did not hurt the player");
    arena(DRAGON, 13);
    game.monsters[1] = {{10, 11}, GOBLIN, monster_info(GOBLIN).health,
                        100, {0, 0}, 0};
    breathed = false;
    for(int i = 0; i < 80 && !breathed; ++i) {
        game.monsters[0].pos = {13, 10};
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
        game.monsters[0].pos = {13, 10};
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
        game.monsters[0].pos = {13, 10};
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
        game.monsters[0].pos = {13, 10};
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
            game.monsters[0].pos = {11, 10};
            end_turn();
            affected = type == RATTLESNAKE ? game.weakened != 0 :
                type == TARANTULA ? game.paralyzed != 0 : game.confused != 0;
        }
        require(affected, "enemy on-hit effect did not trigger");
    }
}

void check_vampire_amulet()
{
    auto arena = []() {
        start_new(0x84e2);
        std::memset(game.walls, 0, sizeof(game.walls));
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.door_count = 0;
        game.player = {10, 10};
        game.hp = 10;
        game.hunger = 255;
        game.invisible = 100;
        game.dexterity = 12;
        game.monsters[0] = {{11, 10}, GOBLIN, 10, 0, {0, 0}, 0};
        game.inventory[0] = {AMULET_VAMPIRE,
                             static_cast<uint8_t>(ITEM_CURSED | 1)};
        game.amulet_slot = 0;
        status_text.clear();
    };
    uint8_t monster_dexterity = monster_info(GOBLIN).dexterity;
    uint8_t hit_range = static_cast<uint8_t>(12 * 3 + monster_dexterity + 1);
    uint16_t hit_seed = 0, miss_seed = 0;
    for(uint16_t seed = 1; seed < 1024; ++seed) {
        uint16_t state = seed;
        if(next_random(state) % hit_range < monster_dexterity)
            miss_seed = seed;
        else
            hit_seed = seed;
        if(hit_seed && miss_seed) break;
    }
    require(hit_seed && miss_seed, "could not find melee test seeds");

    arena();
    game.random_state = miss_seed;
    move_player(1, 0);
    require(game.hp == 10 && game.monsters[0].hp == 10,
            "cursed vampire amulet drained on a miss");

    arena();
    game.random_state = hit_seed;
    move_player(1, 0);
    require(game.hp == 9 && game.monsters[0].hp < 10 &&
            status_text.find("Your amulet drains your life.") !=
                std::string::npos,
            "cursed vampire amulet did not drain on a hit");

    arena();
    game.inventory[0].info = 1;
    game.random_state = hit_seed;
    move_player(1, 0);
    require(game.hp == 11, "positive vampire amulet stopped healing");

    arena();
    game.hp = game.monsters[0].hp = 1;
    game.random_state = hit_seed;
    move_player(1, 0);
    require(game.monsters[0].type == NO_MONSTER && game.hp == 0 &&
            session.ended && session.result == DEATH && game.turns == 0,
            "lethal cursed vampire drain did not end the run after a kill");
}

int main()
{
    check_startup_save_state();
    check_new_run_state();
    check_position_value_and_boundaries();
    check_inventory_view();
    check_stacked_ground_items();
    check_enemy_roster();
    check_enemy_abilities();
    check_vampire_amulet();
    check_confused_wall_bump();
    check_potions();
    check_repeat_inventory_action();
    check_ground_item_exchange();
    check_ground_item_drop();
    check_scrolls_and_identification();
    check_mapping_persists_rooms();
    check_teleport_avoids_prompts();
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
            if(!wall_at(x, y) && item_at({x, y}) == NONE &&
               monster_at({x, y}) == NONE &&
               !(x == game.up.x && y == game.up.y) &&
               !(x == game.down.x && y == game.down.y)) {
                drop_x = x;
                drop_y = y;
                found = true;
            }
    require(found, "no free floor tile");
    mark(game.marks[game.floor], TAKEN_ITEMS, 0);
    game.ground[0].item.type = NO_ITEM;
    game.player = {drop_x, drop_y};
    game.inventory[0] = {FOOD, 1};
    require(drop_inventory(0) && game.inventory[0].type == NO_ITEM,
            "inventory drop failed");
    require(item_at({drop_x, drop_y}) != NONE,
            "dropped item is missing");
    game.hp = player_max_hp();
    action();
    require(item_at({drop_x, drop_y}) != NONE,
            "A action still picks up a ground item");

    Position down = game.down;
    use_stairs(0, down);
    require(game.floor == 1, "descent failed");
    game.player = game.up;
    game.hp = player_max_hp();
    action();
    require(game.floor == 1, "A action still takes the stairs");
    use_stairs(1, game.up);
    require(game.floor == 0, "ascent failed");
    require(std::memcmp(first_floor.data(), game.walls,
                        first_floor.size()) == 0,
            "floor changed on revisit");
    require(item_at({drop_x, drop_y}) == NONE,
            "dropped item was unexpectedly restored across floors");

    game.has_amulet = 1;
    game.player = game.up;
    for(int i = 0; i < 4 && !session.ended; ++i)
        if(!take_stairs()) action();
    require(session.ended && session.result == ESCAPED && !game.valid,
            "amulet victory failed");

    start_new(0x4321);
    game.hp = 1;
    game.hunger = 0;
    game.turns = 3;
    end_turn();
    require(session.ended && session.result == DEATH && !game.valid,
            "starvation death failed");

    std::puts("native game checks passed");
    return 0;
}
