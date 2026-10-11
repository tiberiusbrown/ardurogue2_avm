#include "bench.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "inventory_view.hpp"
#include "world.hpp"
#include "status_advances.hpp"
#if defined(__AVM__)
#include <avm.h>
#endif
#include <string.h>

using namespace rogue;

// One manifest drives compiled entry points and the profiler's case list.
// Name, description, UI preparation, final button, terrain, inventory item,
// unmeasured DOWN/UP presses within the inventory modal.
#define TURN_BENCHMARKS(X) \
    X(awareness_1, "Twelve unalerted enemies at radius one", wait, A, dense, NO_ITEM, 0, 0) \
    X(awareness_3, "Twelve unalerted enemies at radius three", wait, A, dense, NO_ITEM, 0, 0) \
    X(awareness_4, "Twelve unalerted enemies at radius four", wait, A, dense, NO_ITEM, 0, 0) \
    X(awareness_6, "Twelve unalerted enemies at radius six", wait, A, dense, NO_ITEM, 0, 0) \
    X(light_full_dense, "Radius six with twelve enemies", wait, A, dense, NO_ITEM, 0, 0) \
    X(light_blind_dense, "Blinded turn with twelve enemies", wait, A, dense, NO_ITEM, 0, 0) \
    X(fuel_out, "Torch exhaustion and reference cleanup", wait, A, room, NO_ITEM, 0, 0) \
    X(blind_recovery, "Blindness expires with equipment intact", wait, A, room, NO_ITEM, 0, 0) \
    X(equip_light, "Equip a full lantern", use, A, room, LANTERN, 0, 0) \
    X(equip_shield, "Equip an enchanted round shield", use, A, room, ROUND_SHIELD, 0, 0) \
    X(drink_blindness, "Drink a blindness potion", use, A, room, POTION_BLINDNESS, 0, 0) \
    X(throw_blindness, "Throw blindness at an orc", projectile, RIGHT, room, POTION_BLINDNESS, 0, 0) \
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
    X(drink_healing, "Confirm drinking a healing potion", use, A, room, POTION_HEALING, 0, 0) \
    X(equip_weapon, "Confirm equipping a long sword", use, A, room, LONG_SWORD, 0, 0) \
    X(equip_armor, "Confirm equipping chain mail", use, A, room, CHAIN_MAIL, 0, 0) \
    X(equip_ring, "Confirm equipping a dexterity ring", use, A, room, RING_DEXTERITY, 0, 0) \
    X(equip_cursed_amulet, "Equip an unidentified amulet of speed and discover its curse", use, A, room, AMULET_SPEED, 0, 0) \
    X(scroll_mapping, "Confirm a mapping scroll and reveal the map", use, A, room, SCROLL_MAPPING, 0, 0) \
    X(scroll_teleport, "Confirm a teleport scroll", use, A, room, SCROLL_TELEPORT, 0, 0) \
    X(drop_food, "Confirm dropping food", drop, A, room, FOOD, 0, 0) \
    X(wand_digging, "Submit digging direction; carve blocked terrain", wand, RIGHT, corridor, WAND_DIGGING, 0, 0) \
    X(arrow_hit, "Shoot an arrow that hits", projectile, RIGHT, room, ARROWS, 0, 0) \
    X(arrow_miss, "Shoot an arrow that misses", projectile, RIGHT, room, ARROWS, 0, 0) \
    X(arrow_kill, "Shoot an arrow that kills and awards XP", projectile, RIGHT, room, ARROWS, 0, 0) \
    X(arrow_empty, "Shoot six tiles into empty space", projectile, RIGHT, room, ARROWS, 0, 0) \
    X(equip_bow, "Equip a bow in the weapon slot", use, A, room, LONG_BOW, 0, 0) \
    X(projectile_picker, "Open Ammo before Potions in Throw/Shoot", projectile_browse, A, room, ARROWS, 0, 0) \
    X(pickup_food, "Confirm pickup after stepping onto food", pickup, A, room, NO_ITEM, 0, 0) \
    X(inventory_open_full, "Open a full pack spanning all eleven groups", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_full, "Scroll down across the last group in a full mixed pack", browse, DOWN, room, NO_ITEM, 14, 0) \
    X(inventory_up_full, "Scroll up near the bottom of a full mixed pack", browse, UP, room, NO_ITEM, 15, 5) \
    X(inventory_open_wands, "Open a full pack of identified wands with long names", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_wands, "Scroll down to the last of 16 identified wands", browse, DOWN, room, NO_ITEM, 14, 0) \
    X(inventory_up_wands, "Scroll up through seven visible long wand names", browse, UP, room, NO_ITEM, 15, 6) \
    X(inventory_open_singletons, "Open all eleven groups with one item each in late pack slots", browse, A, room, NO_ITEM, 0, 0) \
    X(inventory_down_singletons, "Scroll down across the last of eleven singleton groups", browse, DOWN, room, NO_ITEM, 7, 0) \
    X(inventory_up_singletons, "Scroll up across singleton group headers near the bottom", browse, UP, room, NO_ITEM, 8, 3) \
    X(storm_splash, "Stormbringer hits with three splash targets and cursed life cost", play, RIGHT, room, STORMBRINGER, 0, 0) \
    X(glass_break, "Glass Sword hits then shatters", play, RIGHT, room, GLASS_SWORD, 0, 0) \
    X(hammer_heavy, "Hammer heavy hit and force knockback", play, RIGHT, room, HAMMER_OF_RUIN, 0, 0) \
    X(reprisal, "Monster melee hit and turn-free counterattack", wait, A, room, RING_REPRISAL, 0, 0) \
    X(phoenix, "Lethal starvation consumes Phoenix Heart", wait, A, room, AMULET_PHOENIX_HEART, 0, 0) \
    X(titan_speed, "Titan Plate effective speed with an active monster", wait, A, room, TITAN_PLATE, 0, 0) \
    X(trap_spike, "Enter a hidden spike trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_poison, "Enter a hidden poison dart trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_flame, "Enter a hidden flame trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_frost, "Enter a hidden frost trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_snare, "Enter a hidden snare trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_teleport, "Enter a hidden teleport trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_confusion, "Enter a hidden confusion trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_drain, "Enter a hidden magic drain trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_summoning, "Enter a hidden summoning trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(trap_rust, "Enter a hidden rust trap", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(feature_holy, "Holy pulse against twelve unholy monsters", play, RIGHT, room, NO_ITEM, 0, 0) \
    X(feature_spring, "Accept a healing spring before the movement turn", feature, A, room, NO_ITEM, 0, 0) \
    X(feature_forge, "Forge item choice before the movement turn", feature_item, A, room, LONG_SWORD, 0, 0) \
    X(feature_search, "Search with eight hidden neighboring traps", wait, A, room, NO_ITEM, 0, 0) \
    X(feature_dense, "Move with twelve visible features and enemies", play, RIGHT, dense, NO_ITEM, 0, 0) \
    X(trap_phoenix, "Lethal spike trap consumes Phoenix Heart", play, RIGHT, room, AMULET_PHOENIX_HEART, 0, 0) \
    X(monster_trap_multi, "Four monsters enter damaging traps", wait, A, room, NO_ITEM, 0, 0) \
    X(monster_trap_kills, "Four environmental trap kills", wait, A, room, NO_ITEM, 0, 0) \
    X(monster_trap_teleport, "Monster enters a teleport trap", wait, A, room, NO_ITEM, 0, 0) \
    X(monster_trap_summon, "Monster summons near itself", wait, A, room, NO_ITEM, 0, 0) \
    X(monster_trap_force, "Force wand lands a monster on spikes", wand, RIGHT, room, WAND_FORCE, 2, 0) \
    X(monster_trap_hammer, "Hammer lands a monster on magic backlash", play, RIGHT, room, HAMMER_OF_RUIN, 0, 0) \
    X(monster_trap_busy, "Late floor with twelve monsters and visible traps", wait, A, dense, NO_ITEM, 0, 0) \
    X(monster_trap_spread, "Spreading force lands four monsters on traps", use, A, room, WAND_FORCE, 2, 0) \
    X(visibility_trap_hidden, "Invisible monster triggers an observable trap", wait, A, room, NO_ITEM, 0, 0) \
    X(visibility_trap_cursed, "Cursed sight flickers during monster trap entry", wait, A, room, NO_ITEM, 0, 0) \
    X(visibility_trap_blind, "Blind player hears no unseen monster trap message", wait, A, room, NO_ITEM, 0, 0)

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
    Item item = is_equipment(type) ? make_equipment(type, -3) : Item{type, is_light(type) ? light_initial_fuel(type) : uint8_t(15)};
    if(!uses_full_info(type)) item.info |= ITEM_IDENTIFIED;
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
    add_inventory(3, POTION_INVISIBILITY);
    add_inventory(4, RING_SEE_INVISIBLE);
    add_inventory(5, PLATE_MAIL);
    add_inventory(6, TWO_HANDED_SWORD);
    add_inventory(7, AMULET_REGENERATION);
    add_inventory(8, YENDOR_AMULET);
    add_inventory(9, WAND_POLYMORPH);
    add_inventory(10, SCROLL_REMOVE_CURSE);
    add_inventory(11, POTION_EXPERIENCE);
    add_inventory(12, RING_INVISIBILITY);
    add_inventory(13, SPLINT_MAIL);
    add_inventory(14, LANTERN);
    add_inventory(15, ARROWS);
}

void prepare_singleton_inventory()
{
    // Ten is the maximum group count. Empty leading slots increase scanning.
    add_inventory(5, LANTERN);
    add_inventory(6, ARROWS);
    add_inventory(7, YENDOR_AMULET);
    add_inventory(8, FOOD);
    add_inventory(9, SCROLL_MASS_CONFUSE);
    add_inventory(10, POTION_INVISIBILITY);
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
    game.offhand_slot = NONE;
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
        game.inventory[0] = is_equipment(item) ? make_equipment(item, 0) : Item{item, is_light(item) ? light_initial_fuel(item) : uint8_t(1)};
        if(!uses_full_info(item)) game.inventory[0].info |= ITEM_IDENTIFIED;
        if(is_wand(item)) {
            set_wand_charges(game.inventory[0], 3);
            set_wand_modifier(game.inventory[0], WAND_NORMAL);
        }
    }
    if(scenario >= Case::trap_spike && scenario <= Case::trap_rust)
        game.features[0] = {{33,16}, static_cast<uint8_t>(FEATURE_SPIKE_TRAP + 2 *
            (static_cast<uint8_t>(scenario) - static_cast<uint8_t>(Case::trap_spike)) + MAP_FEATURE_HIDDEN)};
    switch(scenario) {
    case Case::light_full_dense: break;
    case Case::light_blind_dense: game.blindness = 4; break;
    case Case::blind_recovery: game.blindness = 1; break;
    case Case::fuel_out:
        game.inventory[0] = {TORCH, 1}; game.offhand_slot = session.repeat_slot = 0;
        for(uint16_t seed = 1; ; ++seed) {
            uint16_t state = seed;
            if(next_random(state) % 2 == 0) { game.random_state = seed; break; }
        }
        break;
    case Case::throw_blindness:
        add_monster(0, {33,16}, ORC); game.monsters[0].stun = 15;
        break;

    case Case::trap_rust:
        game.inventory[1]=make_equipment(LONG_SWORD,0); game.weapon_slot=1;
        break;
    case Case::trap_phoenix:
        game.amulet_slot=0; game.hp=1;
        game.features[0]={{33,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        break;
    case Case::feature_holy:
        game.features[0]={{33,16},FEATURE_HOLY_SYMBOL};
        for(uint8_t i=0;i<MONSTERS;++i) {
            add_monster(i,{static_cast<uint8_t>(32+i%4),static_cast<uint8_t>(i<4 ? 14 : 16+i/4)},i==11 ? LORD : ZOMBIE);
            game.monsters[i].stun=15;
        }
        break;
    case Case::feature_spring: game.hp=10; game.features[0]={{33,16},FEATURE_HEALING_SPRING}; break;
    case Case::feature_forge: game.features[0]={{33,16},FEATURE_ENCHANTED_FORGE}; break;
    case Case::feature_search: {
        uint8_t i=0;
        for(int8_t dy=-1;dy<=1;++dy) for(int8_t dx=-1;dx<=1;++dx) {
            if(!dx && !dy) continue;
            game.features[i++]={{static_cast<uint8_t>(32+dx),static_cast<uint8_t>(16+dy)},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        }
        break;
    }
    case Case::monster_trap_multi:
    case Case::monster_trap_kills: {
        add_monster(0,{34,16}); add_monster(1,{30,16}); add_monster(2,{32,14}); add_monster(3,{32,18});
        for(uint8_t i=0;i<4;++i) {
            Position p=game.monsters[i].pos;
            if(p.x!=32) p.x=p.x>32 ? p.x-1:p.x+1;
            else p.y=p.y>16 ? p.y-1:p.y+1;
            game.features[i]={p, uint8_t((i%2 ? FEATURE_FLAME_TRAP:FEATURE_MAGIC_DRAIN_TRAP)|MAP_FEATURE_HIDDEN)};
            if(scenario==Case::monster_trap_kills) game.monsters[i].hp=1;
        }
        game.floor=15;
        break;
    }
    case Case::monster_trap_teleport:
    case Case::monster_trap_summon:
        add_monster(0,{34,16}); game.floor=15;
        game.features[0]={{33,16},uint8_t((scenario==Case::monster_trap_teleport ? FEATURE_TELEPORT_TRAP:FEATURE_SUMMONING_TRAP)|MAP_FEATURE_HIDDEN)};
        break;
    case Case::visibility_trap_hidden:
    case Case::visibility_trap_cursed:
    case Case::visibility_trap_blind:
        add_monster(0,{34,16});
        game.features[0]={{33,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        if(scenario==Case::visibility_trap_hidden) set_monster_effect(game.monsters[0],MON_INVISIBLE,6);
        if(scenario==Case::visibility_trap_blind) game.blindness=6;
        if(scenario==Case::visibility_trap_cursed) {
            add_inventory(1,RING_SEE_INVISIBLE);
            game.inventory[1].info=ITEM_IDENTIFIED|ITEM_CURSED|1; game.ring_slots[0]=1;
        }
        break;
    case Case::monster_trap_force:
        add_monster(0,{33,16}); game.speed=1;
        game.features[0]={{37,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        break;
    case Case::monster_trap_spread:
        add_monster(0,{33,16}); add_monster(1,{31,16}); add_monster(2,{32,15}); add_monster(3,{32,17});
        game.speed=1; set_wand_modifier(game.inventory[0],WAND_SPREADING);
        game.features[0]={{37,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        game.features[1]={{27,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        game.features[2]={{32,11},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        game.features[3]={{32,21},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
        break;
    case Case::monster_trap_busy:
        game.floor=15;
        for(uint8_t i=0;i<MONSTERS;++i) {
            Position p=game.monsters[i].pos;
            if(p.x!=32) p.x=p.x>32 ? p.x-1:p.x+1;
            else p.y=p.y>16 ? p.y-1:p.y+1;
            game.features[i]={p,uint8_t(FEATURE_SPIKE_TRAP+2*(i%5))};
        }
        break;
    case Case::feature_dense:
        for(uint8_t i=0;i<MAP_FEATURES;++i)
            game.features[i]={{static_cast<uint8_t>(28+i%4),static_cast<uint8_t>(12+i/4)},static_cast<uint8_t>(FEATURE_FOUNTAIN+2*(i%9))};
        break;
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
    case Case::storm_splash:
    case Case::glass_break:
    case Case::hammer_heavy:
    case Case::monster_trap_hammer: {
        game.weapon_slot = 0; game.dexterity = MAX_PHYSICAL_STAT; game.speed = 1;
        if(scenario == Case::hammer_heavy)
            for(uint8_t x = 38; x <= 42; ++x) carve(x, 16);
        add_monster(0, {33, 16}, SNAKE, 100); game.monsters[0].stun = 15;
        if(scenario == Case::monster_trap_hammer)
            game.features[0]={{37,16},FEATURE_MAGIC_DRAIN_TRAP|MAP_FEATURE_HIDDEN};
        if(scenario == Case::storm_splash) {
            game.inventory[0].info |= ITEM_CURSED;
            add_monster(1, {34, 16}, SNAKE); add_monster(2, {33, 15}, SNAKE); add_monster(3, {33, 17}, SNAKE);
            game.monsters[1].stun = game.monsters[2].stun = game.monsters[3].stun = 15;
        }
        for(uint16_t seed = 1;; ++seed) {
            game.random_state = seed;
            if(!physical_attack_hits(player_accuracy(), monster_dexterity(SNAKE))) continue;
            WeaponDefinition w = weapon_definition(item);
            weapon_damage_roll(w.minimum_damage, w.maximum_damage, 0);
            if(scenario == Case::glass_break && static_cast<uint8_t>(next_random(game.random_state))) continue;
            if((scenario == Case::hammer_heavy || scenario == Case::monster_trap_hammer) && roll(3)) continue;
            game.random_state = seed; break;
        }
        break;
    }
    case Case::reprisal: {
        game.ring_slots[0] = 0; game.inventory[1] = make_equipment(DAGGER, 0); game.weapon_slot = 1;
        game.speed = monster_speed(PHANTOM); add_monster(0, {33, 16}, PHANTOM); game.monsters[0].stun = 0;
        for(uint16_t seed = 1;; ++seed) {
            game.random_state = seed;
            if(!physical_attack_hits(monster_dexterity(PHANTOM), player_dexterity())) continue;
            roll(3);
            armor_absorption(player_armor_rating(), 0);
            if(roll(2) || !physical_attack_hits(player_accuracy(), monster_dexterity(PHANTOM))) continue;
            game.random_state = seed; break;
        }
        break;
    }
    case Case::phoenix:
        game.amulet_slot = 0; game.hp = 1; game.hunger = 0; game.turns = 255;
        break;
    case Case::titan_speed:
        game.armor_slot = 0; add_monster(0, {38, 16});
        break;
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
    case Case::projectile_picker:
        game.inventory[0]={ARROWS,255}; game.inventory[1]={POTION_POISON,1};
        break;
    case Case::arrow_hit: case Case::arrow_miss: case Case::arrow_kill: case Case::arrow_empty: {
        game.inventory[0]={ARROWS,255}; game.inventory[1]=make_equipment(LONG_BOW,0); game.weapon_slot=1;
        game.dexterity=scenario==Case::arrow_miss ? 0 : MAX_PHYSICAL_STAT;
        if(scenario!=Case::arrow_empty) {
            add_monster(0,{36,16},GOBLIN,scenario==Case::arrow_kill ? 1 : 100);
            for(uint16_t seed=1;;++seed) {
                uint16_t state=seed;
                uint8_t accuracy=player_ranged_accuracy(LONG_BOW);
                if((next_random(state) % (accuracy*2+monster_dexterity(GOBLIN)+1) >= monster_dexterity(GOBLIN)) ==
                   (scenario!=Case::arrow_miss)) { game.random_state=seed; break; }
            }
        }
        break;
    }
    case Case::wand_digging:
        game.walls[(16 * MAP_W + 35) >> 3] |= static_cast<uint8_t>(1u << (35 & 7));
        break;
    case Case::pickup_food: add_ground(0, {33, 16}); break;
    default: break;
    }
    if(scenario == Case::awareness_1 || scenario == Case::awareness_3 ||
       scenario == Case::awareness_4 || scenario == Case::awareness_6) {
        for(auto& m:game.monsters) if(m.type) m.state=0;
        game.offhand_slot=game.amulet_slot=NONE;
        if(scenario != Case::awareness_1) {
            game.inventory[15]={uint8_t(scenario==Case::awareness_3 ? TORCH : LANTERN),255};
            game.offhand_slot=15;
        }
        if(scenario==Case::awareness_6) {
            game.inventory[14]={AMULET_BRILLIANCE,uint8_t(1|ITEM_IDENTIFIED)};
            game.amulet_slot=14;
        }
        return;
    }
    // Preserve radius-six coverage for existing play and ranged benchmarks.
    if((item == NO_ITEM || scenario == Case::monster_trap_force || scenario == Case::monster_trap_hammer ||
        scenario == Case::monster_trap_spread) &&
       game.inventory[14].type == NO_ITEM && game.inventory[15].type == NO_ITEM &&
       scenario != Case::fuel_out) {
        game.inventory[15] = {GLOWING_ORB, 0}; game.offhand_slot = 15;
        game.inventory[14] = {AMULET_BRILLIANCE, uint8_t(1 | ITEM_IDENTIFIED)}; game.amulet_slot = 14;
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
#if defined(__AVM__)
    // Detect SDK/font drift before measuring any case; no runtime game cost.
    for(uint8_t n=0;n<95;++n)
        if(AVM_FONT_BR5D->glyph_first!=32 || AVM_FONT_BR5D->num_glyphs!=95 ||
           STATUS_ADVANCES[n]!=AVM_FONT_BR5D->data[uint16_t(n)*sizeof(avm_font_glyph_t)+6]) {
            game.valid=0; return;
        }
#endif
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
