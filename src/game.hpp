#pragma once

#include <stdint.h>

#if defined(__AVM__)
#include <avm/pgmspace.h>
#else
#include <string.h>
#define PROGMEM
#define F(s) s
#define memcpy_P(dst, src, size) memcpy((dst), (src), (size))
#endif

namespace rogue {

constexpr uint8_t MAP_W = 64;
constexpr uint8_t MAP_H = 32;
constexpr int16_t LIGHT_RADIUS = 6;
constexpr bool in_light_radius(int16_t dx, int16_t dy)
{
    return dx >= -LIGHT_RADIUS && dx <= LIGHT_RADIUS &&
           dy >= -LIGHT_RADIUS && dy <= LIGHT_RADIUS &&
           dx * dx + dy * dy <= LIGHT_RADIUS * LIGHT_RADIUS;
}
constexpr uint8_t FLOORS = 16;
constexpr uint8_t ROOMS = 12;
constexpr uint8_t DOORS = 11;
constexpr uint8_t MONSTERS = 12;
constexpr uint8_t GROUND_ITEMS = 16;
constexpr uint8_t DROPPED_ITEMS = 8;
constexpr uint8_t INVENTORY = 16;
constexpr uint8_t NONE = 0xff;
constexpr uint8_t SAVE_VERSION = 5;

enum ItemType : uint8_t {
    NO_ITEM, FOOD, HEALING, CONFUSION, POISON, HARMING,
    STRENGTH, DEXTERITY, PARALYSIS, SLOWING, EXPERIENCE,
    INVISIBILITY, SWORD, ARMOR, AMULET
};
constexpr uint8_t POTION_COUNT = INVISIBILITY - HEALING + 1;
constexpr bool is_potion(uint8_t type)
{
    return type >= HEALING && type <= INVISIBILITY;
}

enum MonsterType : uint8_t {
    NO_MONSTER, RAT, SNAKE, ZOMBIE, ORC, TROLL, LORD
};

struct Room { uint8_t x, y, w, h; };
struct Door { uint8_t x, y; };
struct Monster { uint8_t x, y, type, hp, stun; };
struct GroundItem { uint8_t x, y, type, amount; };
struct DroppedItem { uint8_t floor, x, y, type, amount; };
struct Item { uint8_t type, amount, reserved[2]; };
static_assert(sizeof(Item) == 4, "Item must reserve four bytes");
struct FloorMarks {
    // 16 item, 12 monster, 11 door, and 12 room flags: 51 bits.
    uint8_t bits[7];
};

enum FloorMark : uint8_t {
    TAKEN_ITEMS = 0,
    KILLED_MONSTERS = TAKEN_ITEMS + GROUND_ITEMS,
    OPENED_DOORS = KILLED_MONSTERS + MONSTERS,
    VISITED_ROOMS = OPENED_DOORS + DOORS
};

struct Game {
    uint8_t walls[MAP_W * MAP_H / 8];
    uint8_t explored[MAP_W * MAP_H / 8]; // One bit per tile.
    FloorMarks marks[FLOORS];
    Room rooms[ROOMS];
    Door doors[DOORS];
    Monster monsters[MONSTERS];
    GroundItem ground[GROUND_ITEMS];
    Item inventory[INVENTORY];
    uint16_t run_seed, random_state, score, best_score;
    uint8_t magic, version, valid, floor;
    uint8_t px, py, up_x, up_y, down_x, down_y;
    uint8_t hp, max_hp, level, xp, attack, dexterity, defense, hunger, turns;
    uint8_t weakened, confused, paralyzed, slowed, invisible;
    uint8_t has_amulet, door_count, weapon_slot, armor_slot;
    uint8_t potion_appearance[POTION_COUNT], identified_potions[2];
    DroppedItem dropped[DROPPED_ITEMS];
};

struct Session {
    uint8_t repeat_slot, result;
    bool ended;
};

extern Game game;
extern Session session;

// Status text is drawn immediately. status_word is the wrapping primitive.
void status_word(const char* word);
void status(const char PROGMEM* words);
void status(Item item);
void status(MonsterType monster);
void status_number(uint8_t value);

uint16_t next_random(uint16_t& state);
bool marked(const FloorMarks& marks, FloorMark group, uint8_t index);
void mark(FloorMarks& marks, FloorMark group, uint8_t index);
bool wall_at(int16_t x, int16_t y);
bool wall_exposed(uint8_t x, uint8_t y);
void explore(uint8_t x, uint8_t y);
bool explored(uint8_t x, uint8_t y);
uint8_t door_at(uint8_t x, uint8_t y);
bool door_open(uint8_t index);
bool blocked(int16_t x, int16_t y);
void visit_room();
void make_floor();
bool can_see(uint8_t x, uint8_t y);
bool ray_visible(uint8_t sx, uint8_t sy, const uint16_t opaque[13]);

void start_new(uint16_t seed);
void finish(uint8_t result);
uint8_t monster_at(uint8_t x, uint8_t y);
uint8_t item_at(uint8_t x, uint8_t y);
void end_turn();
void move_player(int8_t dx, int8_t dy);
void action();
bool use_inventory(uint8_t slot);
bool drop_inventory(uint8_t slot);
bool potion_identified(uint8_t type);
uint8_t potion_color(uint8_t type);

} // namespace rogue
