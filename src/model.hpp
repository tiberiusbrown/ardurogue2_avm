#pragma once

#include <stdint.h>
#include <stddef.h>
#include "combat_math.hpp"

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
constexpr uint8_t MAX_LIGHT_RADIUS = 6;
constexpr uint8_t clamp_light_radius(uint8_t radius)
{
    return radius > MAX_LIGHT_RADIUS ? MAX_LIGHT_RADIUS : radius;
}
constexpr bool in_light_radius(int16_t dx, int16_t dy, uint8_t radius)
{
    return dx >= -static_cast<int16_t>(radius) && dx <= radius &&
           dy >= -static_cast<int16_t>(radius) && dy <= radius &&
           dx * dx + dy * dy <= radius * radius;
}
constexpr uint8_t FLOORS = 16;
constexpr uint8_t DOORS = 11;
constexpr uint8_t MONSTERS = 12;
constexpr uint8_t GROUND_ITEMS = 16;
constexpr uint8_t INVENTORY = 16;
constexpr uint8_t NONE = 0xff;
constexpr uint8_t SAVE_MAGIC = 0xa7;
// Room descriptors are generation scratch, no longer part of the saved layout.
constexpr uint8_t SAVE_VERSION = 25;

enum ItemType : uint8_t {
    NO_ITEM, FOOD,
    AMMO_FIRST, ARROWS = AMMO_FIRST, AMMO_LAST = ARROWS,
    // Inclusive group boundaries alias real items without consuming IDs.
    POTION_FIRST,
    POTION_HEALING = POTION_FIRST, POTION_CONFUSION, POTION_POISON, POTION_HARMING,
    POTION_STRENGTH, POTION_DEXTERITY, POTION_PARALYSIS, POTION_SLOWING, POTION_EXPERIENCE,
    POTION_INVISIBILITY,
    POTION_LAST = POTION_INVISIBILITY,
    WEAPON_FIRST,
    LONG_SWORD = WEAPON_FIRST, DAGGER, SPEAR, MACE, TWO_HANDED_SWORD, SHORT_BOW, LONG_BOW,
    STORMBRINGER, GLASS_SWORD, HAMMER_OF_RUIN,
    WEAPON_LAST = HAMMER_OF_RUIN,
    ARMOR_FIRST,
    CHAIN_MAIL = ARMOR_FIRST, LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, SPLINT_MAIL, PLATE_MAIL,
    DRAGONHIDE, TITAN_PLATE,
    ARMOR_LAST = TITAN_PLATE,
    YENDOR_AMULET,
    RING_FIRST,
    RING_SEE_INVISIBLE = RING_FIRST, RING_STRENGTH, RING_DEXTERITY,
    RING_PROTECTION, RING_FIRE_IMMUNITY, RING_ATTACK,
    RING_SUSTENANCE, RING_INVISIBILITY,
    RING_REPRISAL, RING_HUNT,
    RING_LAST = RING_HUNT,
    AMULET_FIRST,
    AMULET_SPEED = AMULET_FIRST, AMULET_CLARITY, AMULET_CONSERVATION,
    AMULET_REGENERATION, AMULET_VAMPIRE, AMULET_IRONBLOOD,
    AMULET_VITALITY, AMULET_WISDOM,
    AMULET_PHOENIX_HEART, AMULET_HEART_OF_GIANT,
    AMULET_LAST = AMULET_HEART_OF_GIANT,
    SCROLL_FIRST,
    SCROLL_IDENTIFY = SCROLL_FIRST, SCROLL_ENCHANT, SCROLL_REMOVE_CURSE,
    SCROLL_TELEPORT, SCROLL_MAPPING, SCROLL_FEAR,
    SCROLL_TORMENT, SCROLL_MASS_CONFUSE, SCROLL_MASS_POISON,
    SCROLL_LAST = SCROLL_MASS_POISON,
    WAND_FIRST,
    WAND_FORCE = WAND_FIRST, WAND_TELEPORT, WAND_DIGGING, WAND_FIRE,
    WAND_STRIKING, WAND_ICE, WAND_POLYMORPH,
    WAND_LAST = WAND_POLYMORPH
};
constexpr uint8_t MAX_BOW_RANGE = MAX_LIGHT_RADIUS;
constexpr bool is_ammo(uint8_t type) { return type >= AMMO_FIRST && type <= AMMO_LAST; }
constexpr bool is_bow(uint8_t type) { return type == SHORT_BOW || type == LONG_BOW; }
constexpr uint8_t POTION_COUNT = POTION_LAST - POTION_FIRST + 1;
constexpr uint8_t RING_COUNT = RING_LAST - RING_FIRST + 1;
constexpr uint8_t AMULET_COUNT = AMULET_LAST - AMULET_FIRST + 1;
constexpr uint8_t SCROLL_COUNT = SCROLL_LAST - SCROLL_FIRST + 1;
constexpr uint8_t WAND_COUNT = WAND_LAST - WAND_FIRST + 1;
constexpr bool is_weapon(uint8_t type)
{
    return type >= WEAPON_FIRST && type <= WEAPON_LAST;
}
constexpr bool is_armor(uint8_t type)
{
    return type >= ARMOR_FIRST && type <= ARMOR_LAST;
}
constexpr bool is_equipment(uint8_t type) { return is_weapon(type) || is_armor(type); }
constexpr bool is_artifact(uint8_t type)
{
    return (type >= STORMBRINGER && type <= HAMMER_OF_RUIN) ||
           (type >= DRAGONHIDE && type <= TITAN_PLATE) ||
           (type >= RING_INVISIBILITY && type <= RING_HUNT) ||
           (type >= AMULET_PHOENIX_HEART && type <= AMULET_HEART_OF_GIANT);
}
constexpr uint8_t ARTIFACT_COUNT = 10;
constexpr bool is_potion(uint8_t type)
{
    return type >= POTION_FIRST && type <= POTION_LAST;
}
constexpr bool is_ring(uint8_t type)
{
    return type >= RING_FIRST && type <= RING_LAST;
}
constexpr bool is_amulet(uint8_t type)
{
    return type >= AMULET_FIRST && type <= AMULET_LAST;
}
constexpr bool is_scroll(uint8_t type)
{
    return type >= SCROLL_FIRST && type <= SCROLL_LAST;
}
constexpr bool is_wand(uint8_t type)
{
    return type >= WAND_FIRST && type <= WAND_LAST;
}

constexpr bool is_throwable_or_shootable(uint8_t type) { return is_potion(type) || is_ammo(type); }
constexpr bool is_stackable(uint8_t type) { return type == FOOD || is_ammo(type) || is_potion(type) || is_scroll(type); }

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
    uint8_t strength, dexterity, speed, armor, health, xp;
};
static_assert(sizeof(MonsterInfo) == 8 && offsetof(MonsterInfo, armor) == 5,
              "monster combat table layout changed");

struct Position {
    uint8_t x;
    uint8_t y;
};
constexpr bool operator==(Position a, Position b)
{
    return a.x == b.x && a.y == b.y;
}
constexpr bool operator!=(Position a, Position b)
{
    return !(a == b);
}
static_assert(sizeof(Position) == 2, "position layout changed");
static_assert(alignof(Position) == 1, "position alignment changed");
static_assert(offsetof(Position, x) == 0 && offsetof(Position, y) == 1,
              "position byte order changed");

// Map y uses only five bits; its high bit records an open door.
struct Door { Position pos; };
// Four independent four-bit effects: confusion, slowing, invisibility, weakness.
// Paralysis uses stun. Two bytes per monster keep the image within AVM RAM.
struct Monster { Position pos; uint8_t type, hp, stun, effects[2], state; };
constexpr uint8_t MON_AGGRO = 1;
constexpr uint8_t MON_AFRAID = 0x40;
enum MimicAppearance : uint8_t {
    MIMIC_SCROLL, MIMIC_POTION, MIMIC_AMULET, MIMIC_RING, MIMIC_WAND,
    MIMIC_APPEARANCE_COUNT
};
constexpr uint8_t MIMIC_APPEARANCE_SHIFT = 1;
constexpr uint8_t MIMIC_APPEARANCE_MASK = 0x0e;
static_assert((MIMIC_APPEARANCE_MASK & (MON_AGGRO | MON_AFRAID)) == 0 &&
              MIMIC_APPEARANCE_COUNT <= (MIMIC_APPEARANCE_MASK >> MIMIC_APPEARANCE_SHIFT) + 1,
              "mimic appearance overlaps monster state flags");
constexpr MimicAppearance mimic_appearance(const Monster& monster)
{
    uint8_t appearance = (monster.state & MIMIC_APPEARANCE_MASK) >> MIMIC_APPEARANCE_SHIFT;
    return appearance < MIMIC_APPEARANCE_COUNT ? static_cast<MimicAppearance>(appearance) : MIMIC_SCROLL;
}
constexpr void set_mimic_appearance(Monster& monster, MimicAppearance appearance)
{
    if(appearance >= MIMIC_APPEARANCE_COUNT) appearance = MIMIC_SCROLL;
    monster.state = static_cast<uint8_t>((monster.state & ~MIMIC_APPEARANCE_MASK) |
        (static_cast<uint8_t>(appearance) << MIMIC_APPEARANCE_SHIFT));
}
enum MonsterEffect : uint8_t {
    MON_CONFUSED, MON_SLOWED, MON_INVISIBLE, MON_WEAKENED
};
struct Item { uint8_t type, info; };
constexpr uint8_t ITEM_VALUE_MASK = 0x3f;
constexpr uint8_t ITEM_CURSED = 0x40;
constexpr uint8_t ITEM_IDENTIFIED = 0x80;

// Inherent capability belongs to type definitions, never to instance info.
struct WeaponDefinition { uint8_t minimum_damage, maximum_damage; int8_t accuracy; };
struct ArmorDefinition { uint8_t rating; };
struct RangedWeaponDefinition { uint8_t minimum_damage, maximum_damage; int8_t accuracy; uint8_t range; };
constexpr RangedWeaponDefinition ranged_weapon_definition(uint8_t type)
{
    return type == SHORT_BOW ? RangedWeaponDefinition{4, 7, 1, 5} :
           type == LONG_BOW ? RangedWeaponDefinition{5, 8, 0, MAX_BOW_RANGE} :
                             RangedWeaponDefinition{1, 2, -2, 3};
}
static_assert(ranged_weapon_definition(SHORT_BOW).range <= MAX_BOW_RANGE &&
              ranged_weapon_definition(LONG_BOW).range <= MAX_BOW_RANGE, "bow exceeds viewport");
constexpr uint8_t maximum_stack(uint8_t type) { return is_ammo(type) ? 255 : ITEM_VALUE_MASK; }
constexpr WeaponDefinition weapon_definition(uint8_t type)
{
    switch(type) {
    case SHORT_BOW: case LONG_BOW: return {1, 2, -2};
    case DAGGER: return {1, 4, 2};
    case SPEAR: return {2, 5, 1};
    case LONG_SWORD: return {2, 6, 0};
    case MACE: return {3, 7, -1};
    case TWO_HANDED_SWORD: return {4, 8, -2};
    case STORMBRINGER: return {6, 10, 2};
    case GLASS_SWORD: return {8, 14, 4};
    case HAMMER_OF_RUIN: return {6, 10, 1};
    default: return {UNARMED_MIN_DAMAGE, UNARMED_MAX_DAMAGE, 0};
    }
}
constexpr ArmorDefinition armor_definition(uint8_t type)
{
    switch(type) {
    case LEATHER_ARMOR: return {1};
    case RING_MAIL: return {2};
    case SCALE_MAIL: return {3};
    case CHAIN_MAIL: return {4};
    case SPLINT_MAIL: return {5};
    case PLATE_MAIL: return {6};
    case DRAGONHIDE: return {7};
    case TITAN_PLATE: return {12};
    default: return {0};
    }
}

// All equipment stores only enchant + MAX_EQUIPMENT_ENCHANT in value bits.
static_assert(2 * MAX_EQUIPMENT_ENCHANT <= ITEM_VALUE_MASK,
              "enchantment exceeds the item value bits");
constexpr int8_t equipment_enchant(const Item& item)
{
    if(!is_equipment(item.type)) return 0;
    uint8_t value = item.info & ITEM_VALUE_MASK;
    if(value > 2 * MAX_EQUIPMENT_ENCHANT) value = 2 * MAX_EQUIPMENT_ENCHANT;
    return static_cast<int8_t>(value - MAX_EQUIPMENT_ENCHANT);
}

constexpr void set_equipment_enchant(Item& item, int8_t enchant)
{
    if(!is_equipment(item.type)) return;
    if(enchant > MAX_EQUIPMENT_ENCHANT) enchant = MAX_EQUIPMENT_ENCHANT;
    if(enchant < -MAX_EQUIPMENT_ENCHANT) enchant = -MAX_EQUIPMENT_ENCHANT;
    uint8_t value = static_cast<uint8_t>(enchant + MAX_EQUIPMENT_ENCHANT);
    item.info = static_cast<uint8_t>((item.info & ~ITEM_VALUE_MASK) | value);
}

constexpr Item make_equipment(uint8_t type, int8_t enchant)
{
    Item item{type, 0};
    set_equipment_enchant(item, enchant);
    return item;
}
enum WandModifier : uint8_t {
    WAND_NORMAL, WAND_CURSED, WAND_UNRELIABLE,
    WAND_SPREADING, WAND_POWERFUL, WAND_OVERPOWERED
};
constexpr uint8_t WAND_CHARGE_MASK = 0x0f;
constexpr uint8_t WAND_MODIFIER_MASK = 0x70;
constexpr uint8_t WAND_MODIFIER_SHIFT = 4;
constexpr uint8_t wand_charges(const Item& item)
{
    return item.info & WAND_CHARGE_MASK;
}
constexpr void set_wand_charges(Item& item, uint8_t charges)
{
    item.info = static_cast<uint8_t>((item.info & ~WAND_CHARGE_MASK) |
                                     (charges & WAND_CHARGE_MASK));
}
constexpr WandModifier wand_modifier(const Item& item)
{
    return static_cast<WandModifier>((item.info & WAND_MODIFIER_MASK) >>
                                     WAND_MODIFIER_SHIFT);
}
constexpr void set_wand_modifier(Item& item, WandModifier modifier)
{
    item.info = static_cast<uint8_t>((item.info & ~WAND_MODIFIER_MASK) |
        ((static_cast<uint8_t>(modifier) << WAND_MODIFIER_SHIFT) &
         WAND_MODIFIER_MASK));
}
constexpr bool wand_spreads(const Item& item)
{
    return wand_modifier(item) == WAND_SPREADING ||
           wand_modifier(item) == WAND_OVERPOWERED;
}
constexpr bool wand_powerful(const Item& item)
{
    return wand_modifier(item) == WAND_POWERFUL ||
           wand_modifier(item) == WAND_OVERPOWERED;
}
constexpr bool wand_afflicted(const Item& item)
{
    return wand_modifier(item) == WAND_CURSED ||
           wand_modifier(item) == WAND_UNRELIABLE;
}
constexpr bool wand_needs_direction(const Item& item)
{
    return wand_modifier(item) == WAND_NORMAL ||
           wand_modifier(item) == WAND_POWERFUL;
}
// Quantities, accessory magnitudes and wand charges only. Equipment callers
// must use equipment_enchant/set_equipment_enchant; this is not a combat API.
constexpr uint8_t item_value(const Item& item)
{
    return is_ammo(item.type) ? item.info : is_wand(item.type) ? wand_charges(item) :
           item.info & ITEM_VALUE_MASK;
}
constexpr void set_item_value(Item& item, uint8_t value)
{
    if(is_ammo(item.type)) item.info = value;
    else if(is_wand(item.type)) set_wand_charges(item, value);
    else item.info = static_cast<uint8_t>((item.info & ~ITEM_VALUE_MASK) |
                                          (value & ITEM_VALUE_MASK));
}
constexpr bool item_is_cursed(const Item& item)
{
    if(is_ammo(item.type)) return false;
    return is_wand(item.type) ? wand_modifier(item) == WAND_CURSED :
           (item.info & ITEM_CURSED) != 0;
}
constexpr bool item_is_identified(const Item& item)
{
    return is_ammo(item.type) || (item.info & ITEM_IDENTIFIED) != 0;
}
static_assert(sizeof(Item) == 2, "Item must use two bytes");
struct GroundItem { Position pos; Item item; };
struct Game {
    uint8_t walls[MAP_W * MAP_H / 8];
    uint8_t explored[MAP_W * MAP_H / 8]; // One bit per tile.
    Door doors[DOORS];
    Monster monsters[MONSTERS];
    GroundItem ground[GROUND_ITEMS];
    Item inventory[INVENTORY];
    uint16_t run_seed, random_state, score, best_score;
    uint8_t magic, version, valid, floor;
    Position player, up, down;
    uint8_t hp, max_hp, level, xp, strength, dexterity, speed;
    uint8_t magic_resistance, hunger, turns;
    uint8_t weakened, confused, paralyzed, slowed, invisible, vamp_drain;
    uint8_t has_amulet, door_count, weapon_slot, armor_slot;
    uint8_t amulet_slot, ring_slots[2];
    uint8_t identified_items[6];
};

enum RunResult : uint8_t {
    DEATH = 0,
    ESCAPED = 1,
    ABANDONED = 2
};

struct Session {
    uint8_t repeat_slot;
    RunResult result;
    bool ended;
};

extern Game game;
extern Session session;

#if defined(__AVM__)
static_assert(sizeof(Game) == 773, "Game saved layout changed");
#endif
static_assert(sizeof(Monster) == 8, "monster layout changed");
static_assert(sizeof(GroundItem) == 4, "ground item layout changed");
static_assert(sizeof(Door) == 2, "door layout changed");
static_assert(offsetof(Door, pos) == 0, "door save offset changed");
static_assert(offsetof(Monster, pos) == 0 && offsetof(Monster, type) == 2 &&
              offsetof(Monster, state) == 7, "monster save offsets changed");
static_assert(offsetof(GroundItem, pos) == 0 &&
              offsetof(GroundItem, item) == 2, "ground save offsets changed");
static_assert(offsetof(Game, doors) == 512 &&
              offsetof(Game, monsters) == 534 &&
              offsetof(Game, ground) == 630 &&
              offsetof(Game, inventory) == 694 &&
              offsetof(Game, player) == 738 &&
              offsetof(Game, up) == 740 && offsetof(Game, down) == 742 &&
              offsetof(Game, hp) == 744 &&
              offsetof(Game, strength) == 748 &&
              offsetof(Game, magic_resistance) == 751 &&
              offsetof(Game, identified_items) == 767,
              "game save offsets changed");
static_assert(POTION_COUNT == 10 && SCROLL_COUNT == 9 &&
              RING_COUNT == 10 && AMULET_COUNT == 10 && WAND_COUNT == 7,
              "item table counts changed");
static_assert(POTION_COUNT + SCROLL_COUNT + RING_COUNT + AMULET_COUNT + WAND_COUNT == 46 &&
              POTION_COUNT + SCROLL_COUNT + RING_COUNT + AMULET_COUNT + WAND_COUNT <=
                  sizeof(Game::identified_items) * 8, "identification bitset capacity exceeded");

} // namespace rogue
