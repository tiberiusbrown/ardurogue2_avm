#pragma once

#include <stdint.h>

#if defined(__AVM__)
#define ROGUE_ROM_DATA __attribute__((address_space(1)))
#else
#define ROGUE_ROM_DATA
#endif

namespace rogue {

constexpr uint8_t MAP_W = 64;
constexpr uint8_t MAP_H = 32;
constexpr uint8_t FLOORS = 16;
constexpr uint8_t ROOMS = 12;
constexpr uint8_t DOORS = 11;
constexpr uint8_t MONSTERS = 12;
constexpr uint8_t GROUND_ITEMS = 16;
constexpr uint8_t DROPPED_ITEMS = 8;
constexpr uint8_t INVENTORY = 16;
constexpr uint8_t NONE = 0xff;

enum ItemType : uint8_t {
    NO_ITEM, FOOD, HEALING, SWORD, ARMOR, AMULET
};

enum MonsterType : uint8_t {
    NO_MONSTER, RAT, SNAKE, SKELETON, ORC, TROLL, LORD
};

enum Message : uint8_t {
    WELCOME, WALL, OPENED, HIT, MISSED, HURT, KILLED, FOUND,
    FULL, HEALED, FED, EQUIPPED, EMPTY, STAIRS, AMULET_FOUND,
    HUNGRY, NO_ITEM_HERE, DROPPED, PICKED_UP
};

struct Room { uint8_t x, y, w, h; };
struct Door { uint8_t x, y, open; };
struct Monster { uint8_t x, y, type, hp, stun, spawn; };
struct GroundItem { uint8_t x, y, type, amount; };
struct DroppedItem { uint8_t floor, x, y, type, amount; };
struct Item { uint8_t type, amount; };
struct FloorMarks {
    uint16_t taken_items;
    uint16_t killed_monsters;
    uint16_t opened_doors;
    uint16_t visited_rooms;
};

struct Game {
    uint8_t walls[MAP_W * MAP_H / 8];
    uint8_t explored[MAP_W * MAP_H / 32]; // One bit per 2x2 tiles.
    FloorMarks marks[FLOORS];
    Room rooms[ROOMS];
    Door doors[DOORS];
    Monster monsters[MONSTERS];
    GroundItem ground[GROUND_ITEMS];
    Item inventory[INVENTORY];
    uint16_t run_seed, random_state, score, best_score;
    uint8_t magic, version, valid, floor;
    uint8_t px, py, up_x, up_y, down_x, down_y;
    uint8_t hp, max_hp, level, xp, attack, defense, hunger, turns;
    uint8_t has_amulet, door_count, weapon_slot, armor_slot;
    DroppedItem dropped[DROPPED_ITEMS];
};

struct Session {
    Message message;
    uint8_t repeat_slot, result;
    bool ended;
};

extern Game game;
extern Session session;

uint16_t next_random(uint16_t& state);
bool marked(uint16_t bits, uint8_t index);
void mark(uint16_t& bits, uint8_t index);
bool wall_at(int16_t x, int16_t y);
void explore(uint8_t x, uint8_t y);
bool explored(uint8_t x, uint8_t y);
uint8_t door_at(uint8_t x, uint8_t y);
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

} // namespace rogue
