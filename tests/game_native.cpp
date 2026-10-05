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
static int ray_animations = 0, burst_animations = 0;
static int spreading_animations = 0, multi_burst_animations = 0;
static uint8_t spreading_steps[4] = {}, multi_burst_count = 0;
static bool multi_burst_powerful = false;
static rogue::Position animated_origin = {}, animated_burst = {};
static uint8_t animated_steps = 0;

namespace rogue {
Game game = {};
}

using namespace rogue;

namespace rogue {
const char* status_word(const char*) { return nullptr; }
void status_words(const char*) {}
void status_suffix(char) {}
void status_capitalize() {}
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
void animate_ray(Position origin, int8_t, int8_t, uint8_t steps) {
    ++ray_animations;
    animated_origin = origin;
    animated_steps = steps;
}
void animate_fire_burst(Position center) {
    ++burst_animations;
    animated_burst = center;
}
void animate_spreading_rays(Position, const uint8_t steps[4]) {
    ++spreading_animations;
    std::memcpy(spreading_steps, steps, 4);
}
void animate_fire_bursts(const Position*, uint8_t count, bool powerful) {
    ++multi_burst_animations;
    multi_burst_count = count;
    multi_burst_powerful = powerful;
}
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
                  sizeof(Game) == 774 && sizeof(Item) == 2,
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

void check_floor_generation_snapshots()
{
    // Hash fields individually: the saved layout and native tail padding may
    // change without changing generated terrain, occupants, or RNG behavior.
    const uint16_t seeds[] = {0, 0x1234, 0x4312, 0xffff};
    const uint32_t expected[4][2] = {
        {0xcfea7fdcu, 0xf3c78df8u}, {0x630b2ec8u, 0xf8c71261u},
        {0xc076a16du, 0xf85500e5u}, {0x1ceb4681u, 0x270ace4au}
    };
    bool matched = true;
    for(unsigned sample = 0; sample < 4; ++sample)
        for(uint8_t ascent = 0; ascent < 2; ++ascent) {
            uint32_t hash = 2166136261u;
            auto append = [&](const void* data, size_t size) {
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                for(size_t i = 0; i < size; ++i)
                    hash = (hash ^ bytes[i]) * 16777619u;
            };
            for(uint8_t floor = 0; floor < FLOORS; ++floor) {
                game = {};
                game.run_seed = seeds[sample];
                game.random_state = 0x51ad;
                game.floor = floor;
                game.has_amulet = ascent;
                make_floor();
                require(game.random_state == 0x51ad,
                        "floor generation consumed combat randomness");
                append(game.walls, sizeof game.walls);
                append(game.explored, sizeof game.explored);
                append(game.doors, sizeof game.doors);
                append(&game.door_count, sizeof game.door_count);
                append(game.monsters, sizeof game.monsters);
                append(game.ground, sizeof game.ground);
                append(&game.player, sizeof game.player);
                append(&game.up, sizeof game.up);
                append(&game.down, sizeof game.down);
            }
            if(hash != expected[sample][ascent]) {
                std::fprintf(stderr, "Floor generation seed %04x ascent %u: %08x != %08x\n",
                             unsigned(seeds[sample]), unsigned(ascent), unsigned(hash),
                             unsigned(expected[sample][ascent]));
                matched = false;
            }
        }
    require(matched, "floor generation snapshot changed");
}

void check_new_run_state()
{
    game.best_score = 321;
    game.score = 70;
    game.hp = 1;
    game.inventory[0] = {LONG_SWORD, 3};
    session = {0, DEATH, true};
    start_new(0x1234);
    require(game.best_score == 321 && game.score == 0 &&
            game.hp == 18 && game.strength == 5 && game.dexterity == 4 &&
            game.magic_resistance == 2 && game.speed == 4 &&
            game.inventory[0].type == NO_ITEM &&
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
    require(empty.count() == 0 && empty.first_slot() == NONE &&
            empty.entry_at(0) == NONE,
            "empty inventory has selectable rows");

    game.inventory[0] = {HEALING, 2};
    game.inventory[1] = {CHAIN_MAIL, 1};
    game.inventory[2] = {LONG_SWORD, 1};
    game.inventory[3] = {POISON, 1};
    game.inventory[4] = {RING_ATTACK, 1};
    game.inventory[5] = {FOOD, 1};
    game.inventory[6] = {LONG_SWORD, 2};
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
    require(sizeof(view) < INVENTORY + INVENTORY_GROUPS + 1,
            "inventory view still stores a row mapping");
    require(view.count() == sizeof(expected),
            "inventory row count changed");
    for(uint8_t row = 0; row < sizeof(expected); ++row)
        require(view.entry_at(row) == expected[row],
                "inventory rows are not grouped by type");
    require(view.entry_at(sizeof(expected)) == NONE,
            "inventory row lookup passed the end");
    require(view.first_slot() == 2 && view.move(6, 1) == 1 &&
            view.move(1, -1) == 6 && view.move(5, 1) == 5 &&
            view.move(2, -1) == 2,
            "inventory selection entered a header or passed the end");
    uint8_t top = 0;
    view.keep_visible(5, top);
    require(top == view.count() - INVENTORY_VISIBLE_ROWS,
            "inventory scrolled beyond the last item");
    view.keep_visible(2, top);
    require(top == 1, "inventory did not scroll back to the first item");

    InventoryView potions(game, is_potion);
    require(potions.count() == 3 && potions.entry_at(0) == INVENTORY + POTIONS &&
            potions.entry_at(1) == 0 && potions.entry_at(2) == 3 &&
            potions.first_slot() == 0 &&
            potions.move(0, 1) == 3 && potions.move(3, 1) == 3,
            "throw selection includes non-potions or headers");

    const uint8_t types[INVENTORY] = {
        WAND_FORCE, FOOD, SCROLL_IDENTIFY, HEALING,
        RING_ATTACK, CHAIN_MAIL, LONG_SWORD, AMULET_SPEED,
        YENDOR_AMULET, WAND_FIRE, SCROLL_FEAR, POISON,
        RING_STRENGTH, CHAIN_MAIL, LONG_SWORD, FOOD
    };
    for(uint8_t slot = 0; slot < INVENTORY; ++slot)
        game.inventory[slot] = {types[slot], 1};
    InventoryView full(game, nullptr);
    const uint8_t full_rows[] = {
        INVENTORY + WEAPONS, 6, 14,
        INVENTORY + ARMORS, 5, 13,
        INVENTORY + RINGS, 4, 12,
        INVENTORY + AMULETS, 7,
        INVENTORY + WANDS, 0, 9,
        INVENTORY + POTIONS, 3, 11,
        INVENTORY + SCROLLS, 2, 10,
        INVENTORY + FOODS, 1, 15,
        INVENTORY + QUEST_ITEMS, 8
    };
    require(full.count() == sizeof(full_rows),
            "full inventory row count changed");
    for(uint8_t row = 0; row < sizeof(full_rows); ++row)
        require(full.entry_at(row) == full_rows[row],
                "full inventory ordering changed");
    top = 0;
    full.keep_visible(8, top);
    require(top == full.count() - INVENTORY_VISIBLE_ROWS,
            "full inventory scrolling changed");
    std::memset(game.inventory, 0, sizeof(game.inventory));
}

void check_stacked_ground_items()
{
    std::memset(game.ground, 0, sizeof(game.ground));
    game.ground[1] = {{4, 5}, {FOOD, 1}};
    game.ground[5] = {{4, 5}, {LONG_SWORD, 1}};
    game.ground[9] = {{4, 5}, {CHAIN_MAIL, 1}};
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
            uint16_t sight[13];
            ray_sight(opaque, sight);
            for(uint8_t sy = 0; sy < 13; ++sy)
                for(uint8_t sx = 0; sx < 13; ++sx) {
                    int16_t tx = static_cast<int16_t>(x) + sx - 6;
                    int16_t ty = static_cast<int16_t>(y) + sy - 6;
                    if(tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H)
                        require(ray_visible(sx, sy, opaque) ==
                                can_see({static_cast<uint8_t>(tx), static_cast<uint8_t>(ty)}) &&
                                bool(sight[sy] & (1u << sx)) == ray_visible(sx, sy, opaque),
                                "local/shared visibility differs from world ray");
                }
        }
    require(samples == 24, "too few visibility samples");
    game.player = {old_x, old_y};
}

void check_shared_rays()
{
    uint32_t random = 0x62f341u;
    for(unsigned pattern = 0; pattern < 1024; ++pattern) {
        uint16_t opaque[13] = {}, sight[13];
        if(pattern == 1)
            for(uint16_t& row : opaque) row = 0x1fff;
        else if(pattern >= 2 && pattern < 171)
            opaque[(pattern - 2) / 13] = static_cast<uint16_t>(1u << ((pattern - 2) % 13));
        else if(pattern >= 171)
            for(uint16_t& row : opaque) {
                random = random * 1664525u + 1013904223u;
                row = static_cast<uint16_t>((random >> 16) & 0x1fff);
            }
        for(uint16_t& row : sight) row = 0xffff;
        ray_sight(opaque, sight);
        for(uint8_t y = 0; y < 13; ++y) {
            require(!(sight[y] & ~0x1fffu), "shared ray output exceeds viewport");
            for(uint8_t x = 0; x < 13; ++x)
                require(bool(sight[y] & (1u << x)) == ray_visible(x, y, opaque),
                        "shared ray differs for a blocker pattern");
        }
    }
}

void check_light_masks()
{
    require(player_light_radius() == 6, "default player light radius changed");
    for(uint8_t radius = 0; radius <= 6; ++radius) {
        unsigned count = 0;
        for(uint8_t sy = 0; sy < 13; ++sy) {
            uint16_t mask = light_mask(radius, sy);
            require(!(mask & ~0x1fffu), "light mask extends beyond viewport");
            for(uint8_t sx = 0; sx < 13; ++sx) {
                int dx = sx - 6, dy = sy - 6;
                bool expected = dx * dx + dy * dy <= radius * radius;
                bool actual = (mask & (1u << sx)) != 0;
                require(actual == expected, "light mask differs from reference circle");
                count += actual;
            }
            require(light_mask(7, sy) == light_mask(6, sy) &&
                    light_mask(255, sy) == light_mask(6, sy),
                    "light mask radius was not clamped");
        }
        if(radius == 0)
            require(count == 1 && light_mask(0, 6) == (1u << 6),
                    "radius zero does not show only the center");
        if(radius == 1) require(count == 5, "radius one circle changed");
        if(radius == 6) require(count == 113, "default circle changed");
    }
    require(light_mask(6, 13) == 0 && light_mask(6, 255) == 0,
            "invalid light mask row is not empty");
}

void check_monster_accessors()
{
    for(unsigned type = 0; type <= 255; ++type) {
        MonsterInfo info = monster_info(static_cast<uint8_t>(type));
        require(monster_flags(type) == info.flags &&
                monster_strength(type) == info.strength &&
                monster_dexterity(type) == info.dexterity &&
                monster_speed(type) == info.speed &&
                monster_armor(type) == info.armor &&
                monster_health(type) == info.health &&
                monster_xp(type) == info.xp,
                "specialized monster accessor differs from full info");
        if(type == NO_MONSTER || type > LORD)
            require(!info.flags && !info.strength && !info.dexterity &&
                    !info.speed && !info.armor && !info.health && !info.xp,
                    "invalid monster type has nonzero stats");
    }
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
    for(uint8_t sy = 0; sy < 13; ++sy)
        for(uint8_t sx = 0; sx < 13; ++sx) {
            int dx = sx - 6, dy = sy - 6;
            bool expected = dx * dx + dy * dy <= 36;
            require(ray_visible(sx, sy, opaque) == expected &&
                    can_see({static_cast<uint8_t>(20 + dx),
                             static_cast<uint8_t>(15 + dy)}) == expected,
                    "default visibility differs from previous circle");
        }
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

void check_active_doors()
{
    start_new(0x1234);
    game.doors[0] = {{12, 10}};
    game.door_count = 1;
    uint16_t tile = 10 * MAP_W + 12;
    game.walls[tile >> 3] &= static_cast<uint8_t>(~(1u << (tile & 7)));
    require(!door_open(0) && blocked(12, 10) &&
            door_at({12, 10}) == 0, "closed door lookup failed");
    open_door(0);
    require(door_open(0) && !blocked(12, 10) &&
            door_at({12, 10}) == 0 &&
            door_position(0) == Position{12, 10},
            "open door lost its position or blocked passage");
    game.doors[0].pos.y &= 0x7f;
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.player = {11, 10};
    move_player(1, 0);
    require(door_open(0) && game.player == Position{11, 10},
            "player did not open the door in place");
    move_player(1, 0);
    require(game.player == Position{12, 10} && door_open(0),
            "player could not pass through the open door");
    make_floor();
    for(uint8_t i = 0; i < game.door_count; ++i)
        require(!door_open(i), "door state survived floor generation");
}

void check_rogue_progression()
{
    constexpr uint16_t seed = 0x4c29;
    start_new(seed);
    require(game.floor == 0 && game.player == game.up && !game.has_amulet,
            "new run did not begin on the first descending floor");
    status_text.clear();
    require(!take_stairs() && game.floor == 0 && !session.ended &&
            status_text.find("Yendor Amulet") != std::string::npos,
            "upward stairs were available before Yendor");

    game.hp = 12;
    game.hunger = 193;
    game.weakened = 1;
    game.inventory[0] = {FOOD, 2};
    game.random_state = 0x7788;
    game.player = game.down;
    require(take_stairs() && game.floor == 1 && game.player == game.up,
            "descent did not generate the next floor");
    require(game.hp == 12 && game.hunger == 193 && game.weakened == 1 &&
            game.inventory[0].type == FOOD && game.random_state == 0x7788,
            "run state changed on descent");
    std::array<uint8_t, sizeof(game.walls)> descent_walls;
    std::memcpy(descent_walls.data(), game.walls, descent_walls.size());
    Game descending = game;
    game.monsters[0].type = NO_MONSTER;
    game.ground[0].item.type = NO_ITEM;
    explore({0, 0});
    if(game.door_count) open_door(0);

    game.player = game.down;
    require(take_stairs() && game.floor == 2 && game.player == game.up,
            "second descent failed");
    require(!explored({0, 0}) && game.monsters[0].type != NO_MONSTER &&
            game.ground[0].item.type != NO_ITEM,
            "previous floor state survived descent");
    for(uint8_t i = 0; i < game.door_count; ++i)
        require(!door_open(i), "opened door survived descent");
    Game continuing = game;
    std::array<uint8_t, sizeof(Game)> saved_bytes;
    std::memcpy(saved_bytes.data(), &game, saved_bytes.size());
    std::memset(&game, 0, sizeof(Game));
    std::memcpy(&game, saved_bytes.data(), saved_bytes.size());
    require(restore_startup_save(true) &&
            std::memcmp(&game, &continuing, sizeof(Game)) == 0,
            "descending save could not continue");

    game.has_amulet = 1;
    game.player = game.up;
    require(take_stairs() && game.floor == 1 && game.player == game.down,
            "ascent did not generate the shallower floor");
    require(std::memcmp(game.walls, descent_walls.data(),
                        descent_walls.size()) != 0,
            "ascent repeated the descending layout");
    for(const GroundItem& item : game.ground)
        require(item.item.type == NO_ITEM, "ordinary loot spawned on ascent");
    for(const Monster& monster : game.monsters)
        require(monster.type != NO_MONSTER,
                "ascent floor did not contain monsters");
    for(uint8_t i = 0; i < game.door_count; ++i)
        require(!door_open(i), "opened door survived onto ascent floor");
    require(game.hp == 12 && game.hunger == 193 && game.weakened == 1 &&
            game.inventory[0].type == FOOD && game.random_state == 0x7788,
            "run state changed on ascent");
    Game ascending = game;
    std::memcpy(saved_bytes.data(), &game, saved_bytes.size());
    std::memset(&game, 0, sizeof(Game));
    std::memcpy(&game, saved_bytes.data(), saved_bytes.size());
    require(restore_startup_save(true) &&
            std::memcmp(&game, &ascending, sizeof(Game)) == 0,
            "ascending save could not continue");
    game.player = game.down;
    require(!take_stairs() && game.floor == 1,
            "descent remained available after Yendor");

    start_new(seed);
    game.floor = 1;
    make_floor();
    require(std::memcmp(game.walls, descending.walls,
                        sizeof(game.walls)) == 0 &&
            std::memcmp(game.monsters, descending.monsters,
                        sizeof(game.monsters)) == 0,
            "descent generation changed with the same seed");
    game.has_amulet = 1;
    make_floor();
    require(std::memcmp(game.walls, ascending.walls,
                        sizeof(game.walls)) == 0 &&
            std::memcmp(game.monsters, ascending.monsters,
                        sizeof(game.monsters)) == 0,
            "ascent generation changed with the same seed");

    start_new(seed);
    game.floor = FLOORS - 1;
    make_floor();
    require(game.monsters[MONSTERS - 1].type == LORD,
            "Lord did not spawn at final descending depth");
    game.player = game.down;
    require(!take_stairs() && game.floor == FLOORS - 1,
            "final descending floor allowed deeper stairs");
    game.inventory[0] = {LONG_SWORD, 1};
    require(drop_disposition(0) == DROP_DISCARD_ALL,
            "Yendor spawn slot was reused before the Lord died");
    defeat_monster(MONSTERS - 1);
    require(game.monsters[MONSTERS - 1].type == NO_MONSTER &&
            game.ground[15].item.type == YENDOR_AMULET,
            "Lord death did not leave Yendor");
    std::memset(game.monsters, 0, sizeof(game.monsters));
    require(take_item(15) == PICKUP_TAKEN && game.has_amulet &&
            game.ground[15].item.type == NO_ITEM,
            "Yendor pickup did not begin ascent");
    while(game.floor) {
        game.player = game.up;
        require(take_stairs(), "ascent stairs failed");
    }
    game.player = game.up;
    require(take_stairs() && session.ended && session.result == ESCAPED &&
            !game.valid, "returning with Yendor did not win");
}

void check_indirect_lord_death()
{
    start_new(0x7931);
    game.floor = FLOORS - 1;
    make_floor();
    std::memset(game.walls, 0, sizeof(game.walls));
    std::memset(game.monsters, 0, sizeof(game.monsters));
    std::memset(game.ground, 0, sizeof(game.ground));
    game.door_count = 0;
    game.player = {10, 10};
    game.hp = game.max_hp = 240;
    game.magic_resistance = 0;
    game.hunger = 255;
    game.inventory[0] = {RING_FIRE_IMMUNITY, 1};
    game.ring_slots[0] = 0;
    game.monsters[0] = {{10, 14}, DRAGON, 48, 0, {0, 0}, 0};
    game.monsters[1] = {{11, 10}, LORD, 1, 0, {0, 0}, 0};
    for(int i = 0; i < 30 && game.monsters[1].type; ++i)
        end_turn();
    require(game.monsters[1].type == NO_MONSTER &&
            game.ground[15].item.type == YENDOR_AMULET,
            "dragon fire did not leave Yendor when it killed the Lord");
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
    uint8_t strength = game.strength;
    drink(STRENGTH);
    require(game.strength == strength + 1, "strength potion did not increase strength");
    uint8_t dexterity = game.dexterity;
    drink(DEXTERITY);
    require(game.dexterity == dexterity + 1, "dexterity potion did not increase accuracy");
    drink(POISON);
    require(game.weakened, "poison did not weaken the player");
    strength = game.strength;
    drink(STRENGTH);
    require(!game.weakened && game.strength == strength,
            "strength potion did not restore weakening");
    game.strength = 11;
    drink(STRENGTH);
    require(game.strength == 12, "strength potion did not reach the intended cap");
    for(unsigned i = 0; i < 20; ++i) drink(STRENGTH);
    require(game.strength == 12, "strength potions exceeded base STR 12");
    game.weakened = 3;
    drink(STRENGTH);
    require(!game.weakened && game.strength == 12,
            "strength potion at the cap did not cure weakness separately");
    game.strength = strength;
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
    game.inventory[1] = {LONG_SWORD, 1};
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
    game.player = {3, 4};
    game.weapon_slot = game.armor_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    game.magic_resistance = 0;
    game.hp = game.max_hp;
    game.hunger = 255;
    status_text.clear();
}

void fill_item_inventory()
{
    for(Item& item : game.inventory) item = {LONG_SWORD, 1};
}

void check_ground_item_exchange()
{
    reset_item_fixture();
    game.ground[5] = {{3, 4}, {CHAIN_MAIL, 2}};
    uint8_t old_turn = game.turns;
    require(take_item(5) == PICKUP_TAKEN &&
            game.inventory[0].type == CHAIN_MAIL &&
            game.ground[5].item.type == NO_ITEM &&
            game.turns == static_cast<uint8_t>(old_turn + 1),
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
        game.ground[i] = {{3, 4}, {CHAIN_MAIL, 1}};
    game.ground[5].item = {FOOD, 3};
    game.inventory[2] = {LONG_SWORD, 4};
    game.weapon_slot = 2;
    session.repeat_slot = 2;
    old_turn = game.turns;
    require(take_item(5) == PICKUP_NEEDS_SWAP &&
            game.turns == old_turn &&
            swap_ground_item(5, 2) &&
            game.inventory[2].type == FOOD &&
            game.ground[5].item.type == LONG_SWORD &&
            game.ground[5].pos == Position{3, 4} &&
            game.weapon_slot == NONE && session.repeat_slot == NONE &&
            game.turns == static_cast<uint8_t>(old_turn + 1) &&
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
    require(game.ground[generated].item.type != NO_ITEM,
            "explicit floor generation did not create fresh loot");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {LONG_SWORD, 1}};
    game.inventory[3] = {CHAIN_MAIL, 5};
    game.armor_slot = 3;
    game.magic_resistance = 5;
    require(swap_ground_item(7, 3) && game.armor_slot == NONE &&
            player_armor_rating() == 0 && game.magic_resistance == 5,
            "armor swap left armor equipped or changed MR");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {LONG_SWORD, 1}};
    game.inventory[3] = {RING_ATTACK, 2};
    game.ring_slots[1] = 3;
    require(swap_ground_item(7, 3) && game.ring_slots[1] == NONE,
            "ring swap left the ring equipped");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {LONG_SWORD, 1}};
    game.inventory[3] = {AMULET_VITALITY, 2};
    game.amulet_slot = 3;
    game.hp = player_max_hp();
    require(swap_ground_item(7, 3) && game.amulet_slot == NONE &&
            game.hp == player_max_hp(),
            "amulet swap did not clamp health");

    reset_item_fixture();
    fill_item_inventory();
    game.ground[7] = {{3, 4}, {LONG_SWORD, 1}};
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
            game.ground[15].item.type == NO_ITEM,
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
    game.inventory[0] = {LONG_SWORD, 2};
    game.ground[0].item.type = NO_ITEM;
    game.ground[1] = {{3, 4}, {CHAIN_MAIL, 1}};
    require(drop_disposition(0) == DROP_GROUND,
            "empty ground slot was unavailable");
    session.repeat_slot = 0;
    uint8_t old_turn = game.turns;
    require(drop_inventory(0) && game.ground[0].item.type == LONG_SWORD &&
            game.ground[1].item.type == CHAIN_MAIL &&
            game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == NONE &&
            game.turns == static_cast<uint8_t>(old_turn + 1),
            "drop onto occupied tile did not use an empty slot");

    reset_item_fixture();
    game.inventory[0] = {FOOD, 7};
    game.ground[2] = {{3, 4}, {FOOD, 60}};
    game.ground[3] = {{3, 4}, {FOOD, 60}};
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(i != 2 && i != 3) game.ground[i] = {{3, 4}, {CHAIN_MAIL, 1}};
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
    require(drop_inventory(0) && item_value(game.ground[2].item) == 63 &&
            game.ground[0].item.type == HEALING &&
            item_value(game.ground[0].item) == 4,
            "drop remainder did not use a reusable slot");

    reset_item_fixture();
    game.inventory[0] = {FOOD, 3};
    game.ground[2] = {{3, 4}, {FOOD, 60}};
    require(drop_inventory(0) && item_value(game.ground[2].item) == 63 &&
            game.inventory[0].type == NO_ITEM,
            "drop did not fully merge into a ground stack");

    reset_item_fixture();
    game.inventory[0] = {LONG_SWORD, 1};
    for(GroundItem& ground : game.ground)
        ground = {{3, 4}, {CHAIN_MAIL, 1}};
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
    game.inventory[0] = {CHAIN_MAIL, static_cast<uint8_t>(3 | ITEM_CURSED)};
    game.armor_slot = 0;
    game.magic_resistance = 3;
    unchanged = game;
    require(drop_disposition(0) == DROP_INVALID &&
            !drop_inventory(0) && !drop_inventory(0, true) &&
            std::memcmp(&game, &unchanged, sizeof(game)) == 0,
            "cursed equipped armor was removed");

    reset_item_fixture();
    game.inventory[0] = {CHAIN_MAIL, 3};
    game.armor_slot = 0;
    game.magic_resistance = 3;
    require(drop_inventory(0) && game.armor_slot == NONE &&
            player_armor_rating() == 0 && game.magic_resistance == 3 &&
            game.ground[0].item.type == CHAIN_MAIL,
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
    game.inventory[1] = make_equipment(LONG_SWORD, 3);
    require(use_inventory(0, 1) && item_type_identified(SCROLL_IDENTIFY) &&
            item_is_identified(game.inventory[1]) &&
            item_value(game.inventory[0]) == 1,
            "identify scroll did not reveal target or consume one scroll");
    game.inventory[0] = {SCROLL_ENCHANT, 1};
    require(use_inventory(0, 1) && equipment_enchant(game.inventory[1]) == 4 &&
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

void check_mapping_active_floor()
{
    start_new(0x2468);
    std::memset(game.monsters, 0, sizeof(game.monsters));
    game.inventory[0] = {SCROLL_MAPPING, 1};
    require(use_inventory(0), "mapping scroll could not be read");
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x)
            if(!wall_at(x, y))
                require(explored({x, y}), "mapping scroll did not reveal every floor tile");
    make_floor();
    for(uint8_t byte : game.explored)
        require(byte == 0, "exploration survived floor generation");
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
    open_door(0);
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
                info.armor == e.def && info.health == e.hp &&
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
    defeat_monster(mimic);
    require(game.monsters[mimic].type == NO_MONSTER,
            "defeated monster remained on active floor");
    make_floor();
    require(std::memcmp(game.monsters, before.monsters,
                        sizeof(game.monsters)) == 0,
            "fresh generation was affected by the prior kill");
    require(std::memcmp(game.ground, before.ground,
                        sizeof(game.ground)) == 0,
            "fresh generation was affected by the prior kill");
}

void check_passive_bats()
{
    auto arena = [](uint16_t seed) {
        start_new(seed);
        std::memset(game.walls, 0, sizeof(game.walls));
        std::memset(game.monsters, 0, sizeof(game.monsters));
        game.door_count = 0;
        game.player = {10, 10};
        game.hunger = 255;
        // Give the bat exactly one action, independent of fractional scheduling.
        game.speed = monster_info(BAT).speed;
        game.monsters[0] = {{13, 10}, BAT, 1, 0, {0, 0}, 0};
        status_text.clear();
    };
    unsigned directions = 0;
    for(uint16_t seed = 1; seed <= 128; ++seed) {
        arena(seed);
        Position before = game.monsters[0].pos;
        end_turn();
        Position after = game.monsters[0].pos;
        require((after.x == before.x && (after.y + 1 == before.y ||
                 after.y == before.y + 1)) ||
                (after.y == before.y && (after.x + 1 == before.x ||
                 after.x == before.x + 1)),
                "passive bat skipped its action or did not move one cardinal tile");
        directions |= after.x > before.x ? 1 : after.x < before.x ? 2 :
                      after.y > before.y ? 4 : 8;
        require(!(game.monsters[0].state & MON_AGGRO),
                "wandering made a passive bat aggressive");
    }
    require(directions == 15, "passive bats did not wander in all four directions");

    for(uint16_t seed = 1; seed <= 128; ++seed) {
        // Block each possible direction with a different obstacle. In particular,
        // trying the player's tile must wait rather than move or attack.
        arena(seed);
        game.player = {12, 10};
        uint8_t hp = game.hp;
        uint16_t wall = 10 * MAP_W + 14;
        game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
        game.doors[0] = {{13, 11}};
        game.door_count = 1;
        game.monsters[1] = {{13, 9}, MIMIC, monster_info(MIMIC).health,
                             0, {0, 0}, 0};
        end_turn();
        require(game.monsters[0].pos == Position{13, 10} && game.hp == hp &&
                !door_open(0) && !(game.monsters[0].state & MON_AGGRO) &&
                status_text.find("hits you") == std::string::npos,
                "passive bat attacked the player or crossed a blocked tile");
    }

    arena(0x4312);
    game.monsters[0].stun = 2;
    end_turn();
    require(game.monsters[0].pos == Position{13, 10} && game.monsters[0].stun == 1,
            "paralyzed passive bat wandered");

    arena(0x4312);
    game.monsters[0].state |= MON_AGGRO;
    end_turn();
    require(game.monsters[0].pos == Position{12, 10},
            "aggressive bat stopped pursuing the player");
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
        game.magic_resistance = 0;
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

    for(uint8_t type : {uint8_t(GOBLIN), uint8_t(SNAKE)})
        for(uint8_t open = 0; open < 4; ++open) {
            arena(type, 12);
            game.monsters[0].pos.y = 12;
            game.speed = monster_speed(type); // Exactly one enemy action.
            game.doors[0] = {{11, 12}};
            game.doors[1] = {{12, 11}};
            game.door_count = 2;
            if(open & 1) open_door(0);
            if(open & 2) open_door(1);
            Position expected = {12, 12};
            uint8_t expected_open = open;
            if(type == GOBLIN && !(open & 1)) expected_open |= 1;
            else if(type == GOBLIN && !(open & 2)) expected_open |= 2;
            else if(open & 1) expected.x = 11;
            else if(open & 2) expected.y = 11;
            end_turn();
            require(game.monsters[0].pos == expected &&
                    door_open(0) == bool(expected_open & 1) &&
                    door_open(1) == bool(expected_open & 2),
                    "cached monster door lookup changed opening/movement priority");
        }

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
    require(breathed && game.hp < 240 && ray_animations > 0 &&
            burst_animations > 0 && animated_origin == Position{13, 10} &&
            animated_steps == 3 && animated_burst == game.player,
            "dragon fire damage or shared animation changed");
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
    uint8_t hit_range = static_cast<uint8_t>(12 * 2 + monster_dexterity + 1);
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

void wand_arena(uint8_t type, uint8_t charges = 2)
{
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.inventory, 0, sizeof game.inventory);
    std::memset(game.ground, 0, sizeof game.ground);
    game.door_count = 0;
    game.player = {10, 10};
    game.hp = game.max_hp = 240;
    game.magic_resistance = 0;
    game.hunger = 255;
    game.inventory[0] = {type, charges};
    status_text.clear();
    ray_animations = burst_animations = 0;
    spreading_animations = multi_burst_animations = 0;
    std::memset(spreading_steps, 0, sizeof spreading_steps);
    multi_burst_count = 0;
    multi_burst_powerful = false;
}

void wand_modifier_at(WandModifier modifier)
{
    set_wand_modifier(game.inventory[0], modifier);
}

void check_wand_encoding_and_scrolls()
{
    static_assert(sizeof(Item) == 2 && WAND_MODIFIER_MASK == 0x70 &&
                  WAND_CHARGE_MASK == 0x0f, "wand encoding changed");
    for(uint8_t value = 0; value < 6; ++value) {
        WandModifier modifier = static_cast<WandModifier>(value);
        for(uint8_t charges = 0; charges <= 15; ++charges) {
            Item wand = {WAND_FIRE, ITEM_IDENTIFIED};
            set_wand_charges(wand, charges);
            set_wand_modifier(wand, modifier);
            require(wand_charges(wand) == charges && item_value(wand) == charges &&
                    wand_modifier(wand) == modifier && item_is_identified(wand),
                    "wand charge/modifier/identification encoding failed");
            set_item_value(wand, static_cast<uint8_t>(15 - charges));
            require(wand_modifier(wand) == modifier && item_is_identified(wand) &&
                    wand_charges(wand) == 15 - charges,
                    "generic item value overwrote wand modifier bits");
        }
        wand_arena(WAND_FIRE, 10);
        wand_modifier_at(modifier);
        game.inventory[1] = {SCROLL_ENCHANT, 1};
        require(use_inventory(1, 0) && wand_charges(game.inventory[0]) == 14 &&
                wand_modifier(game.inventory[0]) == modifier &&
                !item_is_identified(game.inventory[0]),
                "enchant changed modifier or identification");
        game.inventory[1] = {SCROLL_ENCHANT, 1};
        require(use_inventory(1, 0) && wand_charges(game.inventory[0]) == 15 &&
                wand_modifier(game.inventory[0]) == modifier,
                "enchant exceeded wand charge cap or changed modifier");
        game.inventory[1] = {SCROLL_REMOVE_CURSE, 1};
        status_text.clear();
        require(use_inventory(1, 0) && wand_charges(game.inventory[0]) == 15 &&
                wand_modifier(game.inventory[0]) ==
                    (modifier == WAND_CURSED || modifier == WAND_UNRELIABLE
                        ? WAND_NORMAL : modifier) &&
                !item_is_identified(game.inventory[0]),
                "remove curse changed the wrong wand bits");
        require(status_text.find(modifier == WAND_CURSED ||
                                 modifier == WAND_UNRELIABLE
                    ? "glows white." : "Nothing happens.") !=
                    std::string::npos,
                "remove curse displayed the wrong outcome");
    }
    Item powerful = {WAND_FIRE, 0};
    set_wand_modifier(powerful, WAND_POWERFUL);
    require((powerful.info & ITEM_CURSED) && !item_is_cursed(powerful) &&
            !wand_afflicted(powerful), "powerful wand was called cursed");
    set_wand_modifier(powerful, WAND_OVERPOWERED);
    require((powerful.info & ITEM_CURSED) && !item_is_cursed(powerful) &&
            wand_spreads(powerful) && wand_powerful(powerful),
            "overpowered wand composition or curse classification failed");
    require(wand_needs_direction(Item{WAND_FIRE, 1}) &&
            !wand_needs_direction(Item{WAND_FIRE, 0x11}) &&
            !wand_needs_direction(Item{WAND_FIRE, 0x21}) &&
            !wand_needs_direction(Item{WAND_FIRE, 0x31}) &&
            wand_needs_direction(Item{WAND_FIRE, 0x41}) &&
            !wand_needs_direction(Item{WAND_FIRE, 0x51}),
            "wand direction modes are wrong");
}

void check_wand_identity_and_generation()
{
    static_assert(WAND_COUNT == 7 && sizeof(Item) == 2,
                  "wand layout changed");
    start_new(0x1248);
    uint8_t first[WAND_COUNT];
    bool seen[WAND_COUNT] = {};
    for(uint8_t i = 0; i < WAND_COUNT; ++i) {
        uint8_t type = static_cast<uint8_t>(WAND_FORCE + i);
        require(is_wand(type) && !item_type_identified(type),
                "wand range or initial knowledge is wrong");
        first[i] = item_appearance(type);
        require(first[i] < WAND_COUNT && !seen[first[i]],
                "wand appearances are not a permutation");
        seen[first[i]] = true;
    }
    start_new(0x1248);
    for(uint8_t i = 0; i < WAND_COUNT; ++i)
        require(item_appearance(static_cast<uint8_t>(WAND_FORCE + i)) == first[i],
                "wand appearance changed for the same seed");
    start_new(0x1249);
    bool different = false;
    for(uint8_t i = 0; i < WAND_COUNT; ++i)
        different |= item_appearance(static_cast<uint8_t>(WAND_FORCE + i)) != first[i];
    require(different, "wand appearances do not vary between runs");

    unsigned generated = 0;
    for(uint16_t seed = 1; seed <= 100; ++seed) {
        start_new(seed);
        for(uint8_t floor = 0; floor < FLOORS; ++floor) {
            game.floor = floor;
            uint16_t combat_random = game.random_state;
            make_floor();
            std::array<Item, GROUND_ITEMS> generated_items;
            for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
                generated_items[i] = game.ground[i].item;
            require(game.random_state == combat_random,
                    "floor generation consumed combat randomness");
            for(const GroundItem& ground : game.ground)
                if(is_wand(ground.item.type)) {
                    ++generated;
                    require(item_value(ground.item) >= 3 &&
                            item_value(ground.item) <= 10 &&
                            wand_modifier(ground.item) <= WAND_OVERPOWERED,
                            "generated wand charge or modifier range changed");
                }
            make_floor();
            for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
                require(game.ground[i].item.type == generated_items[i].type &&
                        game.ground[i].item.info == generated_items[i].info,
                        "same seed and floor changed generated wand items");
        }
        game.has_amulet = 1;
        make_floor();
        for(const GroundItem& ground : game.ground)
            require(ground.item.type == NO_ITEM,
                    "ascent generated ordinary items");
    }
    require(generated > 0, "descent never generated a wand");

    wand_arena(WAND_FIRE);
    game.ground[0] = {game.player, {WAND_FIRE, 4}};
    game.ground[1] = {game.player, {WAND_FIRE, 7}};
    require(take_item(0) == PICKUP_TAKEN &&
            take_item(1) == PICKUP_TAKEN &&
            game.inventory[0].type == WAND_FIRE &&
            game.inventory[1].type == WAND_FIRE &&
            item_value(game.inventory[0]) == 2 &&
            item_value(game.inventory[1]) == 4 &&
            game.inventory[2].type == WAND_FIRE &&
            item_value(game.inventory[2]) == 7,
            "wands stacked instead of occupying separate inventory slots");
}

void check_wand_rays_and_charges()
{
    wand_arena(WAND_STRIKING, 2);
    uint8_t turns = game.turns;
    require(!use_wand(0, 0, 0) && item_value(game.inventory[0]) == 2 &&
            game.turns == turns, "invalid or canceled direction spent a charge");
    require(use_wand(0, 1, 0) && item_value(game.inventory[0]) == 1 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            item_type_identified(WAND_STRIKING) &&
            ray_animations == 1 && animated_origin == Position{10, 10} &&
            animated_steps == 6,
            "empty wand shot did not spend a turn, charge, or identify");
    game.inventory[1] = {WAND_STRIKING, 3};
    require(item_type_identified(game.inventory[1].type),
            "wand use did not identify other wands of its type");
    require(!item_is_identified(game.inventory[1]),
            "identifying one wand identified a second individual wand");
    session.ended = true;
    require(use_wand(1, 1, 0) && item_is_identified(game.inventory[1]) &&
            wand_charges(game.inventory[1]) == 2,
            "globally known wand did not reveal its own properties on use");
    session.ended = false;
    session.repeat_slot = 0;
    require(use_wand(0, 1, 0) && game.inventory[0].type == NO_ITEM &&
            session.repeat_slot == NONE,
            "last wand charge did not crumble and clear repeat slot");

    wand_arena(WAND_FIRE, 6);
    wand_modifier_at(WAND_OVERPOWERED);
    game.inventory[1] = {WAND_FIRE, 4};
    game.inventory[2] = {SCROLL_IDENTIFY, 1};
    require(use_inventory(2, 0) && item_type_identified(WAND_FIRE) &&
            item_is_identified(game.inventory[0]) &&
            !item_is_identified(game.inventory[1]) &&
            wand_modifier(game.inventory[0]) == WAND_OVERPOWERED,
            "identify scroll did not preserve per-wand knowledge");

    wand_arena(WAND_FORCE);
    RayResult ray = scan_ray(game.player, 1, 0, 6);
    require(ray.steps == 6 && ray.end == Position{16, 10} &&
            ray.monster == NONE, "open ray range changed");
    uint16_t wall = 10 * MAP_W + 12;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    ray = scan_ray(game.player, 1, 0, 6);
    require(ray.steps == 1 && ray.end == Position{11, 10} && ray.blocker,
            "ray did not stop before a wall");
    game.walls[wall >> 3] = 0;
    game.doors[0] = {{12, 10}};
    game.door_count = 1;
    ray = scan_ray(game.player, 1, 0, 6);
    require(ray.steps == 1 && ray.blocker,
            "ray did not stop before a closed door");
    open_door(0);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    game.monsters[1] = {{13, 10}, ORC, 20, 0, {0, 0}, 0};
    ray = scan_ray(game.player, 1, 0, 6);
    require(ray.steps == 2 && ray.monster == 0 &&
            ray.end == Position{12, 10},
            "ray did not select the first monster");
}

void check_wand_effects()
{
    wand_arena(WAND_STRIKING);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].hp >= 77 &&
            game.monsters[0].hp <= 88 && animated_steps == 2,
            "striking damage or ray endpoint changed");

    wand_arena(WAND_ICE);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].hp >= 85 &&
            game.monsters[0].hp <= 92 &&
            monster_effect(game.monsters[0], MON_SLOWED) == 15,
            "ice damage or slowing changed");

    wand_arena(WAND_TELEPORT);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) &&
            game.monsters[0].pos != Position{12, 10} &&
            !blocked(game.monsters[0].pos.x, game.monsters[0].pos.y) &&
            game.monsters[0].pos != game.player &&
            game.monsters[0].pos != game.up &&
            game.monsters[0].pos != game.down,
            "teleport wand chose an invalid position");

    wand_arena(WAND_FORCE);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{20, 10},
            "force wand did not move target eight tiles");
    wand_arena(WAND_FORCE);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    uint16_t wall = 10 * MAP_W + 15;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{14, 10} &&
            game.monsters[0].stun == 4,
            "force wall collision did not stun at last free tile");
    wand_arena(WAND_FORCE);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    game.monsters[1] = {{15, 10}, GOBLIN, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{14, 10} &&
            game.monsters[1].pos == Position{15, 10} &&
            game.monsters[0].stun == 4 && game.monsters[1].stun == 4,
            "force monster collision overlapped or failed to stun");

    wand_arena(WAND_DIGGING);
    for(uint8_t x = 11; x <= 17; ++x) {
        uint16_t bit = 10 * MAP_W + x;
        game.walls[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
    }
    game.doors[0] = {{13, 10}};
    game.door_count = 1;
    session.ended = true;
    require(use_wand(0, 1, 0) && burst_animations == 0 &&
            ray_animations == 0 && door_open(0),
            "digging used projectile animation or left door shut");
    for(uint8_t x = 11; x <= 16; ++x)
        require(!wall_at(x, 10) && explored({x, 10}),
                "digging failed to carve and explore six tiles");
    require(wall_at(17, 10), "digging exceeded six tiles");
    wand_arena(WAND_DIGGING);
    game.player = {MAP_W - 2, 10};
    session.ended = true;
    require(use_wand(0, 1, 0) && !wall_at(MAP_W - 1, 10),
            "edge digging failed or exceeded map bounds");

    wand_arena(WAND_POLYMORPH);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) &&
            (game.monsters[0].type == PHANTOM ||
             game.monsters[0].type == TARANTULA) &&
            game.monsters[0].hp == monster_info(game.monsters[0].type).health,
            "polymorph did not choose a neighboring type and reset HP");
    wand_arena(WAND_POLYMORPH);
    game.monsters[0] = {{12, 10}, LORD, 128, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].type == LORD,
            "Lord was polymorphed");
    for(uint16_t seed = 1; seed <= 32; ++seed) {
        wand_arena(WAND_POLYMORPH);
        game.random_state = seed;
        game.monsters[0] = {{12, 10}, ANGEL, 24, 0, {0, 0}, 0};
        session.ended = true;
        require(use_wand(0, 1, 0) && game.monsters[0].type != LORD,
                "polymorph created the Lord");
    }
}

void check_wand_fire_and_lord()
{
    wand_arena(WAND_FIRE);
    game.monsters[0] = {{12, 10}, ORC, 5, 0, {0, 0}, 0};
    game.monsters[1] = {{13, 11}, GOBLIN, 5, 0, {0, 0}, 0};
    game.monsters[2] = {{15, 10}, GOBLIN, 5, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].type == NO_MONSTER &&
            game.monsters[1].type == NO_MONSTER &&
            game.monsters[2].type == GOBLIN && burst_animations == 1 &&
            animated_burst == Position{12, 10},
            "fire wand failed 3x3 multi-target burst");
    wand_arena(WAND_FIRE);
    uint16_t wall = 10 * MAP_W + 11;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    session.ended = true;
    require(use_wand(0, 1, 0) && game.hp <= 232 && game.hp >= 225,
            "point-blank fire wand did not burn player");
    wand_arena(WAND_FIRE);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.inventory[1] = {RING_FIRE_IMMUNITY, 1};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 1, 0) && game.hp == 240,
            "fire immunity did not prevent self-inflicted wand damage");
    wand_arena(WAND_FIRE);
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.inventory[1] = {RING_FIRE_IMMUNITY,
                         static_cast<uint8_t>(ITEM_CURSED | 1)};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 1, 0) && game.hp <= 224 && game.hp >= 210,
            "cursed fire ring did not double wand self-damage");
    for(uint8_t type : {WAND_FIRE, WAND_STRIKING, WAND_ICE}) {
        wand_arena(type);
        game.monsters[0] = {{12, 10}, LORD, 1, 0, {0, 0}, 0};
        session.ended = true;
        require(use_wand(0, 1, 0) && game.monsters[0].type == NO_MONSTER &&
                game.ground[15].item.type == YENDOR_AMULET &&
                game.ground[15].pos == Position{12, 10},
                "wand Lord kill did not leave Yendor");
    }
}

void check_wand_enchant_and_save()
{
    wand_arena(WAND_FIRE, 5);
    game.inventory[1] = {SCROLL_ENCHANT, 1};
    require(use_inventory(1, 0) && item_value(game.inventory[0]) == 9,
            "enchant scroll did not add four wand charges");
    game.inventory[1] = {SCROLL_ENCHANT, 1};
    require(use_inventory(1, 0) && item_value(game.inventory[0]) == 13,
            "second enchant scroll changed charge increment");
    game.inventory[1] = {SCROLL_ENCHANT, 1};
    require(use_inventory(1, 0) && item_value(game.inventory[0]) == 15,
            "wand enchant cap changed");
    identify_type(WAND_FIRE);
    Game saved = game;
    std::memset(&game, 0, sizeof game);
    game = saved;
    require(restore_startup_save(true) &&
            game.inventory[0].type == WAND_FIRE &&
            item_value(game.inventory[0]) == 15 &&
            item_type_identified(WAND_FIRE),
            "save round trip lost wand type, charges, or knowledge");

    wand_arena(WAND_ICE, 7);
    wand_modifier_at(WAND_SPREADING);
    identify_item(0);
    require(drop_inventory(0) &&
            game.ground[0].item.info == static_cast<uint8_t>(
                ITEM_IDENTIFIED | (WAND_SPREADING << WAND_MODIFIER_SHIFT) | 7),
            "drop lost individual wand information");
    require(take_item(0) == PICKUP_TAKEN &&
            game.inventory[0].info == static_cast<uint8_t>(
                ITEM_IDENTIFIED | (WAND_SPREADING << WAND_MODIFIER_SHIFT) | 7),
            "pickup lost individual wand information");
    saved = game;
    std::memset(&game, 0, sizeof game);
    game = saved;
    require(restore_startup_save(true) &&
            game.inventory[0].info == static_cast<uint8_t>(
                ITEM_IDENTIFIED | (WAND_SPREADING << WAND_MODIFIER_SHIFT) | 7),
            "save lost per-wand modifier or identification");
}

void check_powerful_wands()
{
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{28, 10},
            "powerful force did not push sixteen tiles");
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    uint16_t wall = 10 * MAP_W + 18;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{17, 10} &&
            game.monsters[0].stun == 8, "powerful force wall stun failed");
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    game.monsters[1] = {{18, 10}, GOBLIN, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos == Position{17, 10} &&
            game.monsters[0].stun == 8 && game.monsters[1].stun == 8,
            "powerful force monster collision failed");

    wand_arena(WAND_TELEPORT);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    game.monsters[1] = {{13, 11}, GOBLIN, 20, 0, {0, 0}, 0};
    game.monsters[2] = {{14, 10}, GOBLIN, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) &&
            game.monsters[0].pos != Position{12, 10} &&
            game.monsters[1].pos != Position{13, 11} &&
            game.monsters[2].pos == Position{14, 10} &&
            !blocked(game.monsters[0].pos.x, game.monsters[0].pos.y) &&
            !blocked(game.monsters[1].pos.x, game.monsters[1].pos.y),
            "powerful teleport area or legal positions failed");
    wand_arena(WAND_TELEPORT);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{16, 11}, ORC, 20, 0, {0, 0}, 0};
    game.monsters[1] = {{15, 9}, GOBLIN, 20, 0, {0, 0}, 0};
    game.monsters[2] = {{18, 10}, GOBLIN, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].pos != Position{16, 11} &&
            game.monsters[1].pos != Position{15, 9} &&
            game.monsters[2].pos == Position{18, 10},
            "powerful teleport ignored an empty ray endpoint area");

    wand_arena(WAND_DIGGING);
    wand_modifier_at(WAND_POWERFUL);
    for(uint8_t y = 9; y <= 11; ++y)
        for(uint8_t x = 11; x <= 17; ++x) {
            uint16_t bit = y * MAP_W + x;
            game.walls[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
        }
    game.doors[0] = {{13, 9}};
    game.doors[1] = {{13, 10}};
    game.doors[2] = {{13, 11}};
    game.door_count = 3;
    session.ended = true;
    require(use_wand(0, 1, 0) && ray_animations == 0,
            "powerful digging fired a projectile");
    for(uint8_t y = 9; y <= 11; ++y) {
        for(uint8_t x = 11; x <= 16; ++x)
            require(!wall_at(x, y) && explored({x, y}),
                    "powerful digging missed horizontal corridor tile");
        require(wall_at(17, y) && door_open(static_cast<uint8_t>(y - 9)),
                "powerful digging range or door opening failed");
    }
    wand_arena(WAND_DIGGING);
    wand_modifier_at(WAND_POWERFUL);
    game.player = {1, 1};
    session.ended = true;
    require(use_wand(0, 0, -1) && explored({0, 0}) &&
            explored({1, 0}) && explored({2, 0}),
            "powerful vertical digging failed edge clipping");

    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].hp >= 53 &&
            game.monsters[0].hp <= 76, "powerful striking damage failed");
    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, LORD, 1, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].type == NO_MONSTER &&
            game.ground[15].item.type == YENDOR_AMULET,
            "powerful striking Lord kill lost Yendor");
    wand_arena(WAND_ICE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    game.monsters[1] = {{13, 11}, GOBLIN, 100, 0, {0, 0}, 0};
    game.monsters[2] = {{14, 10}, GOBLIN, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].hp <= 92 &&
            game.monsters[1].hp <= 92 && game.monsters[2].hp == 100 &&
            monster_effect(game.monsters[0], MON_SLOWED) == 15 &&
            monster_effect(game.monsters[1], MON_SLOWED) == 15,
            "powerful ice area failed");
    wand_arena(WAND_POLYMORPH);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    game.monsters[1] = {{13, 11}, ANGEL, 100, 0, {0, 0}, 0};
    game.monsters[2] = {{11, 9}, LORD, 128, 0, {0, 0}, 0};
    game.monsters[3] = {{14, 10}, GOBLIN, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].type != ORC &&
            game.monsters[1].type == DRAGON && game.monsters[2].type == LORD &&
            game.monsters[3].type == GOBLIN &&
            game.monsters[0].hp == monster_info(game.monsters[0].type).health &&
            game.monsters[1].hp == monster_info(game.monsters[1].type).health,
            "powerful polymorph area or Lord immunity failed");

    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    game.monsters[1] = {{14, 12}, GOBLIN, 100, 0, {0, 0}, 0};
    game.monsters[2] = {{15, 10}, GOBLIN, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[1].hp >= 85 &&
            game.monsters[1].hp <= 92 && game.monsters[2].hp == 100 &&
            game.hp >= 225 && game.hp <= 232 && multi_burst_animations == 1 &&
            multi_burst_count == 1 && multi_burst_powerful,
            "powerful fire coverage, damage, or animation failed");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    game.inventory[1] = {RING_FIRE_IMMUNITY, 1};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 1, 0) && game.hp == 240,
            "fire ring failed on powerful burst");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, ORC, 100, 0, {0, 0}, 0};
    game.inventory[1] = {RING_FIRE_IMMUNITY,
                         static_cast<uint8_t>(ITEM_CURSED | 1)};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 1, 0) && game.hp >= 210 && game.hp <= 224,
            "cursed fire ring failed on powerful burst");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_POWERFUL);
    game.monsters[0] = {{12, 10}, LORD, 1, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 1, 0) && game.monsters[0].type == NO_MONSTER &&
            game.ground[15].item.type == YENDOR_AMULET,
            "powerful fire Lord kill lost Yendor");
}

void check_cursed_wands()
{
    uint16_t right_seed = 1;
    for(; right_seed < 1000; ++right_seed) {
        uint16_t state = right_seed;
        if(next_random(state) % 4 == 1) break;
    }
    require(right_seed < 1000, "no deterministic rightward force seed");
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_CURSED);
    game.random_state = right_seed;
    session.ended = true;
    require(use_wand(0, 0, 0) && game.player == Position{18, 10} &&
            wand_charges(game.inventory[0]) == 1 &&
            item_is_identified(game.inventory[0]),
            "cursed force did not immediately displace player");
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_CURSED);
    game.random_state = right_seed;
    uint16_t wall = 10 * MAP_W + 12;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    session.ended = true;
    require(use_wand(0, 0, 0) && game.player == Position{11, 10} &&
            game.paralyzed >= 4, "cursed force wall collision failed");
    wand_arena(WAND_FORCE);
    wand_modifier_at(WAND_CURSED);
    game.random_state = right_seed;
    game.monsters[0] = {{12, 10}, ORC, 20, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 0, 0) && game.player == Position{11, 10} &&
            game.paralyzed >= 4 && game.monsters[0].stun >= 4,
            "cursed force monster collision failed");

    wand_arena(WAND_TELEPORT);
    wand_modifier_at(WAND_CURSED);
    session.ended = true;
    require(use_wand(0, 0, 0) && game.player != Position{10, 10} &&
            !blocked(game.player.x, game.player.y) &&
            game.player != game.up && game.player != game.down,
            "cursed teleport chose an invalid destination");
    wand_arena(WAND_DIGGING);
    wand_modifier_at(WAND_CURSED);
    std::array<uint8_t, sizeof game.walls> walls;
    std::memcpy(walls.data(), game.walls, walls.size());
    session.ended = true;
    require(use_wand(0, 0, 0) && game.hp >= 217 && game.hp <= 228 &&
            std::memcmp(walls.data(), game.walls, walls.size()) == 0,
            "cursed digging damage or terrain failed");
    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_CURSED);
    session.ended = true;
    require(use_wand(0, 0, 0) && game.hp >= 217 && game.hp <= 228,
            "cursed striking damage failed");
    wand_arena(WAND_ICE);
    wand_modifier_at(WAND_CURSED);
    session.ended = true;
    require(use_wand(0, 0, 0) && game.hp >= 225 && game.hp <= 232 &&
            game.slowed >= 8 && game.slowed <= 15,
            "cursed ice damage or slow failed");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_CURSED);
    game.monsters[0] = {{11, 10}, ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 0, 0) && animated_burst == game.player &&
            game.hp >= 225 && game.hp <= 232 &&
            game.monsters[0].hp >= 85 && game.monsters[0].hp <= 92,
            "cursed fire did not burst on player and nearby monsters");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_CURSED);
    game.inventory[1] = {RING_FIRE_IMMUNITY, 1};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 0, 0) && game.hp == 240,
            "fire immunity failed on cursed wand");
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_CURSED);
    game.inventory[1] = {RING_FIRE_IMMUNITY,
                         static_cast<uint8_t>(ITEM_CURSED | 1)};
    game.ring_slots[0] = 1;
    session.ended = true;
    require(use_wand(0, 0, 0) && game.hp >= 210 && game.hp <= 224,
            "cursed fire ring penalty failed on cursed wand");
    for(uint16_t seed = 1; seed <= 32; ++seed) {
        wand_arena(WAND_POLYMORPH);
        wand_modifier_at(WAND_CURSED);
        game.random_state = seed;
        session.ended = true;
        require(use_wand(0, 0, 0) &&
                (game.weakened || game.confused || game.slowed ||
                 game.paralyzed),
                "cursed polymorph applied no adverse condition");
    }
}

void check_spreading_and_overpowered_wands()
{
    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_SPREADING);
    require(use_wand(0, 0, 0) && spreading_animations == 1 &&
            ray_animations == 0 && burst_animations == 0 &&
            multi_burst_animations == 1 && multi_burst_count == 4 &&
            !multi_burst_powerful,
            "spreading fire did not animate four normal bursts together");

    wand_arena(WAND_STRIKING, 3);
    wand_modifier_at(WAND_SPREADING);
    game.speed = 1;
    const Position targets[] = {{10, 8}, {12, 10}, {10, 12}, {8, 10}};
    for(uint8_t i = 0; i < 4; ++i)
        game.monsters[i] = {targets[i], ORC, 100, 10, {0, 0}, 0};
    uint8_t turns = game.turns;
    require(use_wand(0, 0, 0) && wand_charges(game.inventory[0]) == 2 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            spreading_animations == 1 && ray_animations == 0 &&
            spreading_steps[0] == 2 && spreading_steps[1] == 2 &&
            spreading_steps[2] == 2 && spreading_steps[3] == 2,
            "spreading rays did not animate together or spend one charge/turn");
    for(uint8_t i = 0; i < 4; ++i)
        require(game.monsters[i].hp >= 77 && game.monsters[i].hp <= 88,
                "spreading ray missed a cardinal target");

    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_SPREADING);
    game.speed = 1;
    uint16_t wall = 10 * MAP_W + 11;
    game.walls[wall >> 3] |= static_cast<uint8_t>(1u << (wall & 7));
    game.doors[0] = {{10, 9}};
    game.door_count = 1;
    game.monsters[0] = {{10, 12}, ORC, 100, 10, {0, 0}, 0};
    require(use_wand(0, 0, 0) && spreading_steps[0] == 0 &&
            spreading_steps[1] == 0 && spreading_steps[2] == 2 &&
            spreading_steps[3] == 6 && game.monsters[0].hp < 100,
            "spreading rays failed independent blockers");

    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_OVERPOWERED);
    game.speed = 1;
    for(uint8_t i = 0; i < 4; ++i)
        game.monsters[i] = {targets[i], ORC, 100, 10, {0, 0}, 0};
    require(use_wand(0, 0, 0) && spreading_animations == 1 &&
            wand_charges(game.inventory[0]) == 1,
            "overpowered striking did not spread");
    for(uint8_t i = 0; i < 4; ++i)
        require(game.monsters[i].hp >= 53 && game.monsters[i].hp <= 76,
                "overpowered striking did not amplify all four rays");

    wand_arena(WAND_DIGGING);
    wand_modifier_at(WAND_OVERPOWERED);
    session.ended = true;
    require(use_wand(0, 0, 0) && explored({10, 4}) && explored({16, 10}) &&
            explored({10, 16}) && explored({4, 10}) &&
            explored({9, 4}) && explored({16, 9}) &&
            ray_animations == 0 && spreading_animations == 0,
            "overpowered digging did not carve three-wide cross");

    wand_arena(WAND_FIRE);
    wand_modifier_at(WAND_OVERPOWERED);
    game.speed = 1;
    const Position endpoints[] = {{10, 4}, {16, 10}, {10, 16}, {4, 10}};
    const Position outer[] = {{12, 4}, {16, 12}, {8, 16}, {4, 8}};
    for(uint8_t i = 0; i < 4; ++i)
        game.monsters[i] = {endpoints[i], ORC, 100, 10, {0, 0}, 0};
    for(uint8_t i = 0; i < 4; ++i)
        game.monsters[4 + i] = {outer[i], ORC, 100, 10, {0, 0}, 0};
    game.monsters[8] = {{13, 4}, ORC, 100, 10, {0, 0}, 0};
    require(use_wand(0, 0, 0) && spreading_animations == 1 &&
            multi_burst_animations == 1 && multi_burst_count == 4 &&
            multi_burst_powerful, "overpowered fire animation did not compose");
    for(uint8_t i = 0; i < 4; ++i)
        require(game.monsters[i].hp >= 85 && game.monsters[i].hp <= 92 &&
                game.monsters[4 + i].hp >= 85 &&
                game.monsters[4 + i].hp <= 92,
                "overpowered fire missed an endpoint or radius-two target");
    require(game.monsters[8].hp == 100,
            "overpowered fire exceeded its five-by-five radius");
}

void check_unreliable_wand()
{
    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_UNRELIABLE);
    game.random_state = 0x1234;
    uint16_t state = game.random_state;
    uint8_t direction = static_cast<uint8_t>(next_random(state) % 4);
    const Position targets[] = {{10, 8}, {12, 10}, {10, 12}, {8, 10}};
    for(uint8_t i = 0; i < 4; ++i)
        game.monsters[i] = {targets[i], ORC, 100, 0, {0, 0}, 0};
    session.ended = true;
    require(use_wand(0, 0, 0) && wand_charges(game.inventory[0]) == 1 &&
            item_is_identified(game.inventory[0]) && ray_animations == 1,
            "unreliable wand did not activate immediately");
    for(uint8_t i = 0; i < 4; ++i)
        require(i == direction ? game.monsters[i].hp < 100 :
                game.monsters[i].hp == 100,
                "unreliable wand did not choose exactly one cardinal ray");
    wand_arena(WAND_STRIKING);
    wand_modifier_at(WAND_UNRELIABLE);
    for(uint8_t y = 9; y <= 11; ++y)
        for(uint8_t x = 9; x <= 11; ++x) {
            if(x == 10 && y == 10) continue;
            uint16_t bit = y * MAP_W + x;
            game.walls[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
        }
    uint8_t turns = game.turns;
    require(use_wand(0, 0, 0) && wand_charges(game.inventory[0]) == 1 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            item_is_identified(game.inventory[0]),
            "unreliable wall shot failed to spend one charge and turn");
}

void check_combat_rules();
void print_armor_distributions();
void print_weapon_distributions();
void check_weapon_and_equipment_rules();
void check_benchmark_scenarios();

bool generation_command(int argc, char** argv);

int main(int argc, char** argv)
{
    if(generation_command(argc, argv)) return 0;
    if(argc == 2 && std::strcmp(argv[1], "--combat-distributions") == 0) {
        print_weapon_distributions();
        print_armor_distributions();
        return 0;
    }
    if(argc == 2 && std::strcmp(argv[1], "--armor-distributions") == 0) {
        print_armor_distributions();
        return 0;
    }
    check_benchmark_scenarios();
    check_combat_rules();
    check_weapon_and_equipment_rules();
    check_wand_encoding_and_scrolls();
    check_wand_identity_and_generation();
    check_wand_rays_and_charges();
    check_wand_effects();
    check_wand_fire_and_lord();
    check_wand_enchant_and_save();
    check_powerful_wands();
    check_cursed_wands();
    check_spreading_and_overpowered_wands();
    check_unreliable_wand();
    check_startup_save_state();
    check_floor_generation_snapshots();
    check_new_run_state();
    check_position_value_and_boundaries();
    check_inventory_view();
    check_stacked_ground_items();
    check_enemy_roster();
    check_passive_bats();
    check_enemy_abilities();
    check_vampire_amulet();
    check_confused_wall_bump();
    check_potions();
    check_repeat_inventory_action();
    check_ground_item_exchange();
    check_ground_item_drop();
    check_scrolls_and_identification();
    check_mapping_active_floor();
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
    check_light_masks();
    check_shared_rays();
    check_monster_accessors();
    check_circular_light_radius();
    check_wall_faces();
    check_exploration_resolution();
    check_active_doors();

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
    require(!take_stairs() && game.floor == 1,
            "upward stairs worked before Yendor");

    check_rogue_progression();
    check_indirect_lord_death();

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
