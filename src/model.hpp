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
constexpr uint8_t INVENTORY = 16;
constexpr uint8_t NONE = 0xff;
constexpr uint8_t SAVE_MAGIC = 0xa7;
constexpr uint8_t SAVE_VERSION = 12;

enum ItemType : uint8_t {
    NO_ITEM, FOOD, HEALING, CONFUSION, POISON, HARMING,
    STRENGTH, DEXTERITY, PARALYSIS, SLOWING, EXPERIENCE,
    INVISIBILITY, SWORD, ARMOR, YENDOR_AMULET,
    RING_SEE_INVISIBLE, RING_STRENGTH, RING_DEXTERITY,
    RING_PROTECTION, RING_FIRE_IMMUNITY, RING_ATTACK,
    RING_SUSTENANCE, RING_INVISIBILITY,
    AMULET_SPEED, AMULET_CLARITY, AMULET_CONSERVATION,
    AMULET_REGENERATION, AMULET_VAMPIRE, AMULET_IRONBLOOD,
    AMULET_VITALITY, AMULET_WISDOM,
    SCROLL_IDENTIFY, SCROLL_ENCHANT, SCROLL_REMOVE_CURSE,
    SCROLL_TELEPORT, SCROLL_MAPPING, SCROLL_FEAR,
    SCROLL_TORMENT, SCROLL_MASS_CONFUSE, SCROLL_MASS_POISON
};
constexpr uint8_t POTION_COUNT = INVISIBILITY - HEALING + 1;
constexpr uint8_t RING_COUNT = RING_INVISIBILITY - RING_SEE_INVISIBLE + 1;
constexpr uint8_t AMULET_COUNT = AMULET_WISDOM - AMULET_SPEED + 1;
constexpr uint8_t SCROLL_COUNT = SCROLL_MASS_POISON - SCROLL_IDENTIFY + 1;
constexpr bool is_potion(uint8_t type)
{
    return type >= HEALING && type <= INVISIBILITY;
}
constexpr bool is_ring(uint8_t type)
{
    return type >= RING_SEE_INVISIBLE && type <= RING_INVISIBILITY;
}
constexpr bool is_amulet(uint8_t type)
{
    return type >= AMULET_SPEED && type <= AMULET_WISDOM;
}
constexpr bool is_scroll(uint8_t type)
{
    return type >= SCROLL_IDENTIFY && type <= SCROLL_MASS_POISON;
}

enum MonsterType : uint8_t {
    NO_MONSTER, BAT, SNAKE, RATTLESNAKE, ZOMBIE, GOBLIN,
    PHANTOM, ORC, TARANTULA, HOBGOBLIN, MIMIC, INCUBUS,
    TROLL, GRIFFIN, DRAGON, ANGEL, LORD
};

enum MonsterFlag : uint16_t {
    MON_MEAN = 1u << 0, MON_NOMOVE = 1u << 1,
    MON_REGENS = 1u << 2, MON_NATURAL_INVIS = 1u << 3,
    MON_POISON = 1u << 4, MON_VAMPIRE = 1u << 5,
    MON_CONFUSE_HIT = 1u << 6, MON_PARALYZE_HIT = 1u << 7,
    MON_FIRE_BREATH = 1u << 8, MON_OPENER = 1u << 9,
    MON_SEE_INVIS = 1u << 10
};
struct MonsterInfo {
    uint16_t flags;
    uint8_t strength, dexterity, speed, defense, health, xp;
};

struct Room { uint8_t x, y, w, h; };
struct Door { uint8_t x, y; };
// Four independent four-bit effects: confusion, slowing, invisibility, weakness.
// Paralysis uses stun. Two bytes per monster keep the image within AVM RAM.
struct Monster { uint8_t x, y, type, hp, stun, effects[2], state; };
constexpr uint8_t MON_AGGRO = 1;
constexpr uint8_t MON_AFRAID = 0x40;
enum MonsterEffect : uint8_t {
    MON_CONFUSED, MON_SLOWED, MON_INVISIBLE, MON_WEAKENED
};
struct Item { uint8_t type, info; };
constexpr uint8_t ITEM_VALUE_MASK = 0x3f;
constexpr uint8_t ITEM_CURSED = 0x40;
constexpr uint8_t ITEM_IDENTIFIED = 0x80;
constexpr uint8_t item_value(const Item& item)
{
    return item.info & ITEM_VALUE_MASK;
}
constexpr void set_item_value(Item& item, uint8_t value)
{
    item.info = static_cast<uint8_t>((item.info & ~ITEM_VALUE_MASK) |
                                     (value & ITEM_VALUE_MASK));
}
constexpr bool item_is_cursed(const Item& item)
{
    return (item.info & ITEM_CURSED) != 0;
}
constexpr bool item_is_identified(const Item& item)
{
    return (item.info & ITEM_IDENTIFIED) != 0;
}
static_assert(sizeof(Item) == 2, "Item must use two bytes");
struct GroundItem { uint8_t x, y; Item item; };
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
    uint8_t hp, max_hp, level, xp, attack, dexterity, speed;
    uint8_t defense, hunger, turns;
    uint8_t weakened, confused, paralyzed, slowed, invisible, vamp_drain;
    uint8_t has_amulet, door_count, weapon_slot, armor_slot;
    uint8_t amulet_slot, ring_slots[2];
    uint8_t potion_appearance[POTION_COUNT], scroll_appearance[SCROLL_COUNT];
    uint8_t ring_appearance[RING_COUNT], amulet_appearance[AMULET_COUNT];
    uint8_t identified_items[5];
};

enum RunResult : uint8_t {
    DEATH = 0,
    ESCAPED = 1,
    RETURNED_EMPTY = 2,
    ABANDONED = 3
};

struct Session {
    uint8_t repeat_slot;
    RunResult result;
    bool ended;
};

extern Game game;
extern Session session;

#if defined(__AVM__)
static_assert(sizeof(Game) == 967, "Game saved layout changed");
#endif
static_assert(sizeof(FloorMarks) == 7, "floor flags changed");
static_assert(sizeof(Monster) == 8, "monster layout changed");
static_assert(sizeof(GroundItem) == 4, "ground item layout changed");
static_assert(POTION_COUNT == 10 && SCROLL_COUNT == 9 &&
              RING_COUNT == 8 && AMULET_COUNT == 8, "item table counts changed");

} // namespace rogue
