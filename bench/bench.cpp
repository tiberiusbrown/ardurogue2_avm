#include "bench.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "inventory_view.hpp"
#include "world.hpp"
#include <string.h>

using namespace rogue;

// One manifest drives compiled entry points and the profiler's case list.
// Name, description, UI preparation, final button, terrain, inventory item,
// unmeasured DOWN/UP presses within the inventory modal.
#define TURN_BENCHMARKS(X) \
    X(move_room, "Move in a lit room", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(move_corridor, "Move along a branching corridor", play, RIGHT, corridor, NO_ITEM, 0, 0) \
    X(move_map_edge, "Move with viewport clipping at map corner", play, RIGHT, edge, NO_ITEM, 0, 0) \
    X(move_dense, "Move on an explored maze with 12 active enemies", play, RIGHT, dense, NO_ITEM, 0, 0) \
    X(wait, "Wait one turn", wait, A, room, NO_ITEM, 0, 0) \
    X(wait_dense, "Wait with 12 enemies, doors and ground items", wait, A, dense, NO_ITEM, 0, 0) \
    X(attack_hit, "Bump attack that hits a surviving goblin", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(attack_miss, "Bump attack that misses a goblin", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(attack_kill, "Bump attack that kills and awards XP", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(open_door, "Open a closed door without moving", play, RIGHT, corridor, NO_ITEM, 0, 0) \
    X(eat_food, "Confirm eating food", use, A, room, FOOD, 0, 0) \
    X(drink_healing, "Confirm drinking a healing potion", use, A, room, HEALING, 0, 0) \
    X(equip_weapon, "Confirm equipping a long sword", use, A, room, LONG_SWORD, 0, 0) \
    X(equip_armor, "Confirm equipping chain mail", use, A, room, CHAIN_MAIL, 0, 0) \
    X(equip_ring, "Confirm equipping a dexterity ring", use, A, room, RING_DEXTERITY, 0, 0) \
    X(equip_cursed_amulet, "Equip an unidentified amulet of speed and discover its curse", use, A, room, AMULET_SPEED, 0, 0) \
    X(scroll_mapping, "Confirm a mapping scroll and reveal the map", use, A, room, SCROLL_MAPPING, 0, 0) \
    X(scroll_teleport, "Confirm a teleport scroll", use, A, room, SCROLL_TELEPORT, 0, 0) \
    X(drop_food, "Confirm dropping food", drop, A, room, FOOD, 0, 0) \
    X(wand_digging, "Submit digging direction; carve blocked terrain", wand, RIGHT, corridor, WAND_DIGGING, 0, 0) \
    X(pickup_food, "Confirm pickup after stepping onto food", pickup, A, room, NO_ITEM, 0, 0) \
    X(inventory_open_full, "Open a full pack spanning all nine groups", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_full, "Scroll down across the last group in a full mixed pack", browse, DOWN, room, NO_ITEM, 14, 0) \
    X(inventory_up_full, "Scroll up near the bottom of a full mixed pack", browse, UP, room, NO_ITEM, 15, 5) \
    X(inventory_open_wands, "Open a full pack of identified wands with long names", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_wands, "Scroll down to the last of 16 identified wands", browse, DOWN, room, NO_ITEM, 14, 0) \
    X(inventory_up_wands, "Scroll up through seven visible long wand names", browse, UP, room, NO_ITEM, 15, 6) \
    X(inventory_open_singletons, "Open all nine groups with one item each in late pack slots", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_singletons, "Scroll down across the last of nine singleton groups", browse, DOWN, room, NO_ITEM, 7, 0) \
    X(inventory_up_singletons, "Scroll up across singleton group headers near the bottom", browse, UP, room, NO_ITEM, 8, 3)

namespace {
enum class Terrain : uint8_t { room, corridor, edge, dense };
enum class Case : uint8_t {
#define CASE_ENUM(name, description, setup, button, terrain, item, down, up) name,
    TURN_BENCHMARKS(CASE_ENUM)
#undef CASE_ENUM
    count
};

void add_monster(uint8_t index, Position pos, uint8_t type = GOBLIN, uint8_t hp = 100)
{
    carve(pos.x, pos.y);
    game.monsters[index] = {pos, type, hp, 0, {0, 0}, MON_AGGRO};
}

void add_ground(uint8_t index, Position pos)
{
    game.ground[index] = {pos, {FOOD, 1}};
}

void add_inventory(uint8_t slot, ItemType type)
{
    Item item = is_equipment(type) ? make_equipment(type, -3) : Item{type, 15};
    item.info |= ITEM_IDENTIFIED;
    if(is_equipment(type)) item.info |= ITEM_CURSED;
    if(is_wand(type)) {
        set_wand_charges(item, static_cast<uint8_t>(15 - slot % 6));
        set_wand_modifier(item, slot % 2 ? WAND_UNRELIABLE : WAND_OVERPOWERED);
    }
    game.inventory[slot] = item;
}

// Deliberately interleave groups so late rows repeatedly scan the whole pack.
void prepare_full_inventory()
{
    add_inventory(0, WAND_TELEPORT);
    add_inventory(1, FOOD);
    add_inventory(2, SCROLL_MASS_CONFUSE);
    add_inventory(3, INVISIBILITY);
    add_inventory(4, RING_SEE_INVISIBLE);
    add_inventory(5, PLATE_MAIL);
    add_inventory(6, TWO_HANDED_SWORD);
    add_inventory(7, AMULET_REGENERATION);
    add_inventory(8, YENDOR_AMULET);
    add_inventory(9, WAND_POLYMORPH);
    add_inventory(10, SCROLL_REMOVE_CURSE);
    add_inventory(11, EXPERIENCE);
    add_inventory(12, RING_INVISIBILITY);
    add_inventory(13, SPLINT_MAIL);
    add_inventory(14, LONG_SWORD);
    add_inventory(15, FOOD);
}

void prepare_singleton_inventory()
{
    // Nine is the maximum group count. Empty leading slots increase scanning.
    add_inventory(7, YENDOR_AMULET);
    add_inventory(8, FOOD);
    add_inventory(9, SCROLL_MASS_CONFUSE);
    add_inventory(10, INVISIBILITY);
    add_inventory(11, WAND_POLYMORPH);
    add_inventory(12, AMULET_REGENERATION);
    add_inventory(13, RING_SEE_INVISIBLE);
    add_inventory(14, PLATE_MAIL);
    add_inventory(15, TWO_HANDED_SWORD);
}

// Preparation runs once, before the initial render and any measured input.
void prepare(Case scenario, Terrain terrain, ItemType item)
{
    memset(&game, 0, sizeof game);
    memset(game.walls, 0xff, sizeof game.walls);
    memset(game.identified_items, 0xff, sizeof game.identified_items);
    game.run_seed = game.random_state = 0x4312;
    game.valid = 1;
    game.player = {32, 16};
    game.up = {2, 2};
    game.down = {61, 29};
    game.hp = game.max_hp = 100;
    game.level = 1;
    game.strength = 5;
    game.dexterity = game.speed = 4;
    game.magic_resistance = 2;
    game.hunger = 200;
    game.weapon_slot = game.armor_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    session = {NONE, DEATH, false};

    switch(terrain) {
    case Terrain::room:
        for(uint8_t y = 11; y < 22; ++y)
            for(uint8_t x = 27; x < 38; ++x) carve(x, y);
        break;
    case Terrain::edge:
        game.player = {0, 0};
        for(uint8_t y = 0; y < 9; ++y)
            for(uint8_t x = 0; x < 9; ++x) carve(x, y);
        break;
    case Terrain::corridor:
        for(uint8_t x = 24; x < 43; ++x) carve(x, 16);
        for(uint8_t y = 10; y < 23; ++y) carve(32, y);
        break;
    case Terrain::dense:
        memset(game.explored, 0xff, sizeof game.explored);
        for(uint8_t y = 10; y < 24; ++y)
            for(uint8_t x = 26; x < 40; ++x)
                if(x % 4 != 1 || y % 4 == 0) carve(x, y);
        // Explicit calls keep position/type constants out of guest RAM.
        add_monster(0, {30, 16}, GOBLIN);
        add_monster(1, {34, 16}, TROLL);
        add_monster(2, {32, 14}, RATTLESNAKE);
        add_monster(3, {32, 18}, GOBLIN);
        add_monster(4, {28, 12}, TROLL);
        add_monster(5, {36, 12}, RATTLESNAKE);
        add_monster(6, {28, 20}, GOBLIN);
        add_monster(7, {36, 20}, TROLL);
        add_monster(8, {30, 10}, RATTLESNAKE);
        add_monster(9, {34, 10}, GOBLIN);
        add_monster(10, {30, 22}, TROLL);
        add_monster(11, {34, 22}, RATTLESNAKE);
        game.doors[0] = {{31, 16}};
        game.doors[1] = {{35, 16}};
        game.door_count = 2;
        add_ground(0, {28, 16});
        add_ground(1, {32, 12});
        add_ground(2, {36, 16});
        add_ground(3, {32, 20});
        break;
    }

    if(item != NO_ITEM) {
        game.inventory[0] = is_equipment(item) ? make_equipment(item, 0) : Item{item, 1};
        game.inventory[0].info |= ITEM_IDENTIFIED;
        if(is_wand(item)) {
            set_wand_charges(game.inventory[0], 3);
            set_wand_modifier(game.inventory[0], WAND_NORMAL);
        }
    }
    switch(scenario) {
    case Case::inventory_open_full:
    case Case::inventory_down_full:
    case Case::inventory_up_full:
        prepare_full_inventory();
        break;
    case Case::inventory_open_wands:
    case Case::inventory_down_wands:
    case Case::inventory_up_wands:
        for(uint8_t slot = 0; slot < INVENTORY; ++slot)
            add_inventory(slot, static_cast<ItemType>(WAND_FORCE + (slot + 1) % WAND_COUNT));
        break;
    case Case::inventory_open_singletons:
    case Case::inventory_down_singletons:
    case Case::inventory_up_singletons:
        prepare_singleton_inventory();
        break;
    case Case::attack_hit:
    case Case::attack_miss:
    case Case::attack_kill: {
        game.dexterity = scenario == Case::attack_miss ? 0 : MAX_PHYSICAL_STAT;
        add_monster(0, {33, 16}, GOBLIN, scenario == Case::attack_kill ? 1 : 100);
        uint8_t evasion = monster_dexterity(GOBLIN);
        uint8_t limit = static_cast<uint8_t>(game.dexterity * 2 + evasion + 1);
        for(uint16_t seed = 1; ; ++seed) {
            uint16_t state = seed;
            if((next_random(state) % limit >= evasion) == (scenario != Case::attack_miss)) {
                game.random_state = seed;
                break;
            }
        }
        break;
    }
    case Case::open_door:
        game.doors[0] = {{33, 16}};
        game.door_count = 1;
        break;
    case Case::drink_healing: game.hp = 50; break;
    case Case::equip_cursed_amulet: {
        game.inventory[0].info = ITEM_CURSED | 1;
        constexpr uint8_t index = POTION_COUNT + SCROLL_COUNT + RING_COUNT;
        game.identified_items[index >> 3] &= static_cast<uint8_t>(~(1u << (index & 7)));
        break;
    }
    case Case::wand_digging:
        game.walls[(16 * MAP_W + 35) >> 3] |= static_cast<uint8_t>(1u << (35 & 7));
        break;
    case Case::pickup_food: add_ground(0, {33, 16}); break;
    default: break;
    }
}
} // namespace

extern "C" {
volatile uint8_t bench_case = 0;

// Stable breakpoint before the selector is read; no save or inferior call needed.
__attribute__((noinline)) void bench_select()
{
#if defined(__AVM__)
    asm volatile("" ::: "memory");
#endif
}
}

#define CASE_ENTRY(name, description, setup, button, terrain, item, down, up) \
    extern "C" __attribute__((noinline)) void bench_##name() \
    { prepare(Case::name, Terrain::terrain, item); }
TURN_BENCHMARKS(CASE_ENTRY)
#undef CASE_ENTRY

uint8_t bench_count() { return static_cast<uint8_t>(Case::count); }

void bench_setup()
{
    switch(static_cast<Case>(bench_case)) {
#define CASE_DISPATCH(name, description, setup, button, terrain, item, down, up) \
    case Case::name: bench_##name(); return;
    TURN_BENCHMARKS(CASE_DISPATCH)
#undef CASE_DISPATCH
    case Case::count: break;
    }
    // An invalid selector never reaches a playable scenario.
    game.valid = 0;
}
