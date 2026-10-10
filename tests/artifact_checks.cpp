#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "inventory_view.hpp"
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <cmath>

using namespace rogue;
void require(bool, const char*);

namespace {
// Rarer spawns produce smaller item-state samples. Allow five binomial standard
// deviations plus one count, rather than fixed bounds for the former 1/8 rate.
void binomial_frequency(unsigned count, unsigned total, double probability, const char* reason)
{
    require(total != 0, reason);
    const double expected = total * probability;
    require(std::abs(count - expected) <= 5 * std::sqrt(expected * (1 - probability)) + 1, reason);
}
void arena(uint8_t weapon = NO_ITEM, bool cursed = false)
{
    game = {}; session = {NONE, DEATH, false};
    game.player = {10, 10}; game.up = {1, 1}; game.down = {60, 28};
    game.hp = game.max_hp = 240; game.hunger = 240;
    game.strength = 5; game.dexterity = 30; game.speed = 1; game.level = 1;
    game.random_state = 1;
    game.weapon_slot = game.armor_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    if(is_weapon(weapon)) {
        game.inventory[0] = make_equipment(weapon, 0);
        if(cursed) game.inventory[0].info |= ITEM_CURSED;
        game.weapon_slot = 0;
    }
}
void monster(uint8_t i, Position pos, uint8_t type = ORC, uint8_t hp = 255)
{
    game.monsters[i] = {pos, type, hp, 15, {0, 0}, MON_AGGRO};
}
void ring(uint8_t type, bool cursed = false, uint8_t slot = 2)
{
    game.inventory[slot] = {type, static_cast<uint8_t>(63 | (cursed ? ITEM_CURSED : 0))};
    game.ring_slots[slot == 3 ? 1 : 0] = slot;
}
void armor(uint8_t type, bool cursed = false)
{
    game.inventory[4] = make_equipment(type, 0);
    if(cursed) game.inventory[4].info |= ITEM_CURSED;
    game.armor_slot = 4;
}
void amulet(uint8_t type, bool cursed = false)
{
    game.inventory[5] = {type, static_cast<uint8_t>(1 | (cursed ? ITEM_CURSED : 0))};
    game.amulet_slot = 5;
}
void wall(Position p)
{
    uint16_t cell = p.y * MAP_W + p.x;
    game.walls[cell >> 3] |= static_cast<uint8_t>(1u << (cell & 7));
}
uint16_t hit_seed(bool hit)
{
    for(unsigned seed = 1; seed <= 65535; ++seed) {
        game.random_state = static_cast<uint16_t>(seed);
        if(physical_attack_hits(player_accuracy(), monster_dexterity(game.monsters[0].type)) == hit)
            return game.random_state = static_cast<uint16_t>(seed);
    }
    require(false, "artifact hit/miss fixture unavailable"); return 0;
}
void rate(unsigned count, unsigned total, double low, double high, const char* reason)
{
    double value = double(count) / total;
    std::printf("%s: %u/%u (%.5f)\n", reason, count, total, value);
    require(value > low && value < high, reason);
}

void storm()
{
    for(unsigned seed = 1; seed <= 512; ++seed) {
        arena(STORMBRINGER); game.level = 20; game.dexterity = MAX_PHYSICAL_STAT; monster(0, {11, 10});
        game.random_state = static_cast<uint16_t>(seed); Game before = game;
        uint8_t primary_hp = 255;
        if(physical_attack_hits(player_accuracy(), monster_dexterity(ORC))) {
            uint8_t raw = physical_raw_damage(weapon_damage_roll(6, 10, 0), player_strength());
            primary_hp = static_cast<uint8_t>(255 - physical_damage_after_armor(raw, armor_absorption(monster_armor(ORC), 0)));
        }
        game = before; attack_monster(0);
        require(game.monsters[0].hp == primary_hp, "Stormbringer changed primary physical damage");
    }
    arena(STORMBRINGER, true); game.level = 20; monster(0, {11, 10}, ORC, 1);
    monster(1, {12, 10}, SNAKE, 1); monster(2, {11, 9}, SNAKE, 1);
    monster(3, {11, 11}, SNAKE, 1); monster(4, {12, 11}, SNAKE, 100);
    monster(5, {13, 10}, SNAKE, 100); hit_seed(true);
    attack_monster(0);
    require(!game.monsters[0].type && !game.monsters[1].type && !game.monsters[2].type &&
            !game.monsters[3].type && game.monsters[4].hp == 100 && game.monsters[5].hp == 100,
            "Stormbringer splash lost original position, included diagonals or recursed");
    require(game.score == 62 && game.xp == 14 && game.hp >= 238 && game.hp <= 239,
            "Stormbringer splash rewards or cursed life cost incorrect");
    for(bool cursed : {false, true}) {
        arena(STORMBRINGER, cursed); monster(0, {11, 10}); monster(1, {12, 10});
        hit_seed(false); Game before = game; attack_monster(0);
        require(game.hp == before.hp && game.monsters[0].hp == 255 && game.monsters[1].hp == 255,
                "Stormbringer miss caused damage or splash");
        unsigned hits = 0, costs = 0;
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            arena(STORMBRINGER, cursed); monster(0, {11, 10});
            game.random_state = static_cast<uint16_t>(seed); attack_monster(0);
            if(game.monsters[0].hp != 255) {
                ++hits; costs += game.hp < 240;
                require(game.hp >= 238, "Stormbringer life cost exceeded two HP");
            }
        }
        if(cursed) require(costs == hits, "cursed Stormbringer skipped a life cost");
        else rate(costs, hits, .057, .068, "Stormbringer normal cost frequency");
    }
    // Physical absorption applies independently to splash targets.
    arena(STORMBRINGER); monster(0, {11, 10}); monster(1, {12, 10}, DRAGON);
    uint16_t seed = hit_seed(true); attack_monster(0);
    require(game.monsters[1].hp >= 251 && game.monsters[1].hp <= 254,
            "Stormbringer splash did not use physical armor rules");
    arena(STORMBRINGER, true); monster(0, {11, 10}); amulet(AMULET_PHOENIX_HEART);
    game.hp = 1; game.random_state = seed; attack_monster(0);
    require(game.hp == 240 && !session.ended && game.amulet_slot == NONE,
            "Stormbringer self damage bypassed Phoenix Heart");
}

void glass()
{
    for(bool cursed : {false, true}) {
        unsigned hits = 0, breaks = 0;
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            arena(GLASS_SWORD, cursed); monster(0, {11, 10}); session.repeat_slot = 0;
            game.random_state = static_cast<uint16_t>(seed); attack_monster(0);
            bool hit = game.monsters[0].hp != 255;
            hits += hit;
            if(game.weapon_slot == NONE) {
                ++breaks;
                require(hit && game.inventory[0].type == NO_ITEM && game.inventory[0].info == 0 &&
                        session.repeat_slot == NONE && item_at(game.player) == NONE,
                        "Glass Sword destruction left inventory/equipment/ground state or broke on a miss");
                require(255 - game.monsters[0].hp >= 4, "breaking Glass Sword skipped its damage");
                uint8_t old_hp = game.monsters[0].hp; hit_seed(true); attack_monster(0);
                require(game.monsters[0].hp < old_hp && !session.ended,
                        "Glass Sword break corrupted later combat");
            }
        }
        rate(breaks, hits, cursed ? .057 : .0030, cursed ? .068 : .0048,
             cursed ? "Glass Sword cursed break frequency" : "Glass Sword normal break frequency");
        arena(GLASS_SWORD, cursed); monster(0, {11, 10}); hit_seed(false); attack_monster(0);
        require(game.weapon_slot == 0 && game.inventory[0].type == GLASS_SWORD,
                "Glass Sword miss broke weapon");
    }
}

void hammer()
{
    for(bool cursed : {false, true}) {
        unsigned hits = 0, heavy_hits = 0, recoils = 0;
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            arena(HAMMER_OF_RUIN, cursed); monster(0, {11, 10}, TROLL);
            game.random_state = static_cast<uint16_t>(seed);
            bool hit = physical_attack_hits(player_accuracy(), monster_dexterity(TROLL));
            uint8_t normal = 0; bool heavy = false;
            if(hit) {
                uint8_t raw = physical_raw_damage(weapon_damage_roll(6, 10, 0), 5);
                normal = physical_damage_after_armor(raw, armor_absorption(monster_armor(TROLL), 0));
                heavy = roll(3) == 0;
            }
            game.random_state = static_cast<uint16_t>(seed); attack_monster(0);
            require(game.monsters[0].hp == 255 - (heavy ? saturating_double(normal) : normal),
                    "Hammer heavy damage did not double the post-armor damage");
            require((game.monsters[0].pos != Position{11, 10}) == heavy,
                    "Hammer knockback did not follow a surviving heavy hit");
            hits += hit; heavy_hits += heavy; recoils += game.player != Position{10, 10};
            require(game.player.y == 10 && game.player.x <= 10 && !blocked(game.player.x, game.player.y) &&
                    monster_at(game.player) == NONE, "Hammer recoil entered obstacle or wrong direction");
        }
        rate(heavy_hits, hits, .323, .343, "Hammer heavy hit frequency");
        if(cursed) rate(recoils, hits, .24, .26, "Hammer cursed recoil frequency");
        else require(!recoils, "normal Hammer recoiled");
    }
    // Find a heavy/recoil fixture without hard-coding an implementation-dependent seed.
    uint16_t heavy_seed = 0, recoil_seed = 0;
    for(unsigned seed = 1; !heavy_seed || !recoil_seed; ++seed) {
        arena(HAMMER_OF_RUIN, true); monster(0, {11, 10});
        game.random_state = static_cast<uint16_t>(seed); attack_monster(0);
        if(game.monsters[0].pos.x > 11) heavy_seed = static_cast<uint16_t>(seed);
        if(game.player.x < 10) recoil_seed = static_cast<uint16_t>(seed);
    }
    arena(HAMMER_OF_RUIN); monster(0, {11, 10}, ORC, 1);
    game.random_state = heavy_seed; attack_monster(0);
    require(!game.monsters[0].type && game.monsters[0].pos == Position{11, 10},
            "dead Hammer target was knocked back");
    for(uint8_t obstacle : {0, 1, 2}) {
        arena(HAMMER_OF_RUIN); monster(0, {11, 10});
        if(obstacle == 0) wall({14, 10});
        if(obstacle == 1) { game.doors[0] = {{14, 10}}; game.door_count = 1; }
        if(obstacle == 2) monster(1, {14, 10});
        game.random_state = heavy_seed; attack_monster(0);
        require(game.monsters[0].pos == Position{13, 10} && game.monsters[0].stun == 4 &&
                (obstacle != 2 || game.monsters[1].stun == 4), "Hammer obstacle collision differs from force wand");
        // The same force path (including powerful mode) remains available to wands.
        arena(); monster(0, {11, 10}); wall({14, 10}); force_monster(0, 1, 0, true);
        require(game.monsters[0].pos == Position{13, 10} && game.monsters[0].stun == 8,
                "shared force helper changed powerful wand collision");
    }
    arena(HAMMER_OF_RUIN, true); monster(0, {11, 10}); monster(1, {8, 10});
    game.monsters[1].stun = 0;
    game.random_state = recoil_seed; attack_monster(0);
    require(game.player == Position{9, 10} && game.paralyzed == 4 && game.monsters[1].stun == 4,
            "cursed Hammer player collision consequences changed");
    arena(HAMMER_OF_RUIN, true); monster(0, {11, 10}); wall({9, 10});
    game.random_state = recoil_seed; attack_monster(0);
    require(game.player == Position{10, 10} && game.paralyzed == 4, "Hammer recoil passed through wall");
}

void dragonhide_and_speed()
{
    for(unsigned seed = 1; seed <= 2048; ++seed) {
        for(bool cursed : {false, true}) {
            arena(); armor(DRAGONHIDE, cursed); game.random_state = static_cast<uint16_t>(seed);
            game.magic_resistance = static_cast<uint8_t>(seed % 31);
            uint8_t expected = cursed ? magic_damage_after_save(20, magic_save(game.magic_resistance, 12)) : 0;
            uint16_t state = cursed ? game.random_state : static_cast<uint16_t>(seed);
            game.random_state = static_cast<uint16_t>(seed); player_take_fire_damage(10, 12);
            require(game.hp == 240 - expected && game.random_state == state,
                    "Dragonhide immunity/vulnerability or save/RNG order incorrect");
            ring(RING_FIRE_IMMUNITY, !cursed); player_take_fire_damage(255, 255);
            require(game.hp == 240 - expected, "positive immunity failed to override vulnerability");
        }
        arena(); armor(DRAGONHIDE, true); ring(RING_FIRE_IMMUNITY, true);
        game.random_state = static_cast<uint16_t>(seed); uint8_t expected = magic_damage_after_save(20, magic_save(0, 12));
        game.random_state = static_cast<uint16_t>(seed); player_take_fire_damage(10, 12);
        require(game.hp == 240 - expected, "multiple fire vulnerabilities multiplied more than once");
    }
    arena(); ring(RING_FIRE_IMMUNITY, true); ring(RING_FIRE_IMMUNITY, false, 3);
    require(player_fire_effect() == 1, "positive and cursed fire rings canceled immunity");
    arena(); armor(DRAGONHIDE); hurt_player(7); player_take_magic_damage(9, 8);
    require(game.hp < 233, "Dragonhide protected against non-fire damage");
    arena(); armor(DRAGONHIDE); fire_burst_damage(game.player, true);
    require(game.hp == 240, "Dragonhide failed against a player fire burst");
    arena(); armor(DRAGONHIDE); game.inventory[0] = {WAND_FIRE, 0};
    set_wand_charges(game.inventory[0], 2); set_wand_modifier(game.inventory[0], WAND_CURSED);
    use_wand(0, 0, 0); require(game.hp == 240, "Dragonhide failed against cursed fire wand");
    for(unsigned seed = 1; seed <= 100; ++seed) {
        arena(); armor(DRAGONHIDE); monster(0, {13, 10}, DRAGON); game.monsters[0].stun = 0;
        game.speed = monster_speed(DRAGON); game.random_state = static_cast<uint16_t>(seed);
        end_turn(); require(game.hp == 240, "Dragonhide failed against dragon breath");
    }
    require(armor_definition(TITAN_PLATE).rating > armor_definition(PLATE_MAIL).rating,
            "Titan Plate lacks intrinsic protection");
    for(bool cursed : {false, true}) {
        arena(); game.speed = 4; armor(TITAN_PLATE, cursed);
        require(player_speed_cost() == (cursed ? 8 : 4), "Titan Plate speed penalty incorrect");
        amulet(AMULET_SPEED); game.inventory[5].info = 2;
        require(player_speed_cost() == (cursed ? 6 : 2), "Titan Plate did not combine with speed amulet");
        game.slowed = 2; require(player_speed_cost() == (cursed ? 12 : 4), "Titan Plate slowing order incorrect");
        end_turn(); require(game.speed == 4, "Titan Plate permanently altered speed");
        game.armor_slot = NONE; require(player_speed_cost() == 4, "removing Titan Plate retained speed cost");
        game.speed = 255; armor(TITAN_PLATE, cursed); game.inventory[5].info = ITEM_CURSED | 63;
        require(player_speed_cost() == 255, "effective speed overflowed");
    }
}

void hunt_and_reprisal()
{
    static_assert(is_artifact(RING_INVISIBILITY), "invisibility ring must be an artifact");
    arena(); ring(RING_INVISIBILITY);
    require(player_is_invisible(), "invisibility artifact lost its effect");
    ring(RING_INVISIBILITY, true);
    require(!player_is_invisible(), "cursed invisibility artifact granted invisibility");
    for(uint8_t type : {static_cast<uint8_t>(RING_REPRISAL), static_cast<uint8_t>(RING_HUNT)}) {
        arena(); ring(type);
        require(player_armor_rating() == 2 && !player_is_invisible(), "artifact ring lost passive protection or granted invisibility");
        ring(type, true);
        require(player_armor_rating() == 0 && !player_is_invisible(), "cursed artifact ring gained normal passive benefits");
    }
    for(bool cursed : {false, true}) {
        arena(); ring(RING_HUNT, cursed); monster(0, {12, 10});
        game.inventory[0] = {WAND_STRIKING, 3}; Game before = game;
        game.ring_slots[0] = NONE; use_wand(0, 1, 0); Game control = game;
        game = before; use_wand(0, 1, 0);
        require(game.monsters[0].hp == control.monsters[0].hp && game.random_state == control.random_state,
                "Hunt changed wand damage or RNG");
        arena(); ring(RING_HUNT, cursed); monster(0, {12, 10}); game.inventory[0] = {POTION_HARMING, 1};
        before = game; game.ring_slots[0] = NONE; throw_potion(0, 1, 0); control = game;
        game = before; throw_potion(0, 1, 0);
        require(game.monsters[0].hp == control.monsters[0].hp && game.random_state == control.random_state,
                "Hunt changed thrown potion damage or RNG");
    }
    for(uint8_t bow : {static_cast<uint8_t>(NO_ITEM), static_cast<uint8_t>(SHORT_BOW), static_cast<uint8_t>(LONG_BOW)}) {
        for(bool cursed : {false, true}) {
            for(unsigned seed = 1; seed <= 512; ++seed) {
                arena(bow); ring(RING_HUNT, cursed); ring(RING_ATTACK, false, 3);
                game.inventory[3].info = 1; game.dexterity = 8;
                auto def = ranged_weapon_definition(bow);
                require(player_ranged_accuracy(bow) == clamp_combat_stat(9 + def.accuracy + (cursed ? -6 : 8)),
                        "Hunt accuracy magnitude depends on info, or lost attack ring/bow modifier");
                uint8_t melee = player_accuracy(); game.ring_slots[0] = NONE;
                require(player_accuracy() == melee, "Hunt affected melee accuracy"); game.ring_slots[0] = 2;
                monster(0, {12, 10}); game.inventory[1] = {ARROWS, 1};
                if(is_bow(bow)) set_equipment_enchant(game.inventory[0], 2);
                game.random_state = static_cast<uint16_t>(seed);
                bool hit = physical_attack_hits(player_ranged_accuracy(bow), monster_dexterity(ORC));
                uint8_t damage = 0;
                if(hit) {
                    int raw = physical_raw_damage(weapon_damage_roll(def.minimum_damage, def.maximum_damage,
                        is_bow(bow) ? 2 : 0), player_strength()) + (cursed ? -4 : 6);
                    damage = physical_damage_after_armor(static_cast<uint8_t>(raw < 1 ? 1 : raw), armor_absorption(3, 0));
                }
                game.random_state = static_cast<uint16_t>(seed); throw_or_shoot(1, 1, 0);
                require(game.monsters[0].hp == 255 - damage, "Hunt arrow damage/clamping/enchantment order incorrect");
            }
        }
    }
    for(bool cursed : {false, true}) {
        unsigned hits = 0, effects = 0;
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            arena(LONG_SWORD); ring(RING_REPRISAL, cursed); monster(0, {11, 10}, MIMIC);
            game.monsters[0].stun = 0; game.speed = monster_speed(MIMIC);
            game.random_state = static_cast<uint16_t>(seed);
            Game before = game;
            if(cursed) game.ring_slots[0] = NONE;
            else game.inventory[2] = {RING_PROTECTION, 2}; // Same passive armor and absorption RNG.
            end_turn(); Game control = game;
            game = before; session = {NONE, DEATH, false}; end_turn();
            bool hit = control.hp < 240; hits += hit;
            effects += cursed ? game.hp < control.hp : game.monsters[0].hp < control.monsters[0].hp;
            require(game.turns == 1 && !session.ended && (!cursed || game.monsters[0].hp == 255) &&
                    (hit || (game.hp == 240 && game.monsters[0].hp == 255)),
                    "Reprisal triggered on miss, retaliated when cursed, recursed or spent a turn");
            if(cursed) require(game.hp == control.hp || game.hp + 1 == control.hp,
                               "cursed Reprisal damage exceeded one HP");
        }
        rate(effects, hits, cursed ? .24 : .45, cursed ? .26 : .49,
             cursed ? "cursed Reprisal frequency" : "Reprisal damaging counter frequency (includes misses)");
    }
    bool killed = false;
    for(unsigned seed = 1; seed <= 1000 && !killed; ++seed) {
        arena(GLASS_SWORD); game.level = 20; ring(RING_REPRISAL); monster(0, {11, 10}, MIMIC, 1);
        game.monsters[0].stun = 0; game.speed = monster_speed(MIMIC); game.random_state = static_cast<uint16_t>(seed);
        end_turn(); killed = !game.monsters[0].type;
        if(killed) require(game.score == 38 && game.turns == 1 && game.xp == 11,
                           "Reprisal kill did not award rewards or used an extra turn");
    }
    require(killed, "Reprisal never killed an attacker");
    bool troll_killed = false;
    for(unsigned seed = 1; seed <= 1000 && !troll_killed; ++seed) {
        arena(GLASS_SWORD); game.level = 20; ring(RING_REPRISAL); monster(0, {11, 10}, TROLL, 1);
        game.monsters[0].stun = 0; game.speed = monster_speed(TROLL); game.random_state = static_cast<uint16_t>(seed);
        end_turn(); troll_killed = !game.monsters[0].type;
        if(troll_killed) require(game.monsters[0].hp == 1, "attacker regenerated after Reprisal killed it");
    }
    require(troll_killed, "Reprisal failed against regenerating attacker");
    arena(); ring(RING_REPRISAL, true); uint16_t seed = game.random_state;
    hurt_player(1); player_take_fire_damage(1, 1);
    require(game.hp == 238 && game.random_state != seed, "Reprisal non-melee fixture failed");
    // No ring roll on direct physical/self damage; magic consumes only its save.
    arena(); ring(RING_REPRISAL, true); seed = game.random_state; hurt_player(1);
    require(game.hp == 239 && game.random_state == seed, "Reprisal triggered outside monster melee");
}

void hearts()
{
    for(uint8_t type : {static_cast<uint8_t>(AMULET_PHOENIX_HEART), static_cast<uint8_t>(AMULET_HEART_OF_GIANT)}) {
        arena(); game.speed = 4; game.max_hp = 40; game.hp = 20; amulet(type);
        require(player_speed_cost() == 2 && player_max_hp() == (type == AMULET_PHOENIX_HEART ? 60 : 80) && game.hp == 20,
                "artifact amulet passive bonuses changed or healed current HP");
        game.slowed = 2; require(player_speed_cost() == 4, "artifact amulet speed/slowing interaction changed");
        amulet(type, true); require(player_speed_cost() == 8 &&
            player_max_hp() == (type == AMULET_PHOENIX_HEART ? 40 : 28), "cursed amulet gained normal passive bonuses");
    }
    for(uint8_t source = 0; source < 7; ++source) {
        arena(source == 6 ? STORMBRINGER : NO_ITEM); game.max_hp = 81; game.hp = 1;
        amulet(AMULET_PHOENIX_HEART); session.repeat_slot = 5;
        switch(source) {
        case 0: hurt_player(1); break;
        case 1: player_take_magic_damage(10, 12); break;
        case 2: player_take_fire_damage(10, 12); break;
        case 3: game.inventory[1] = {POTION_HARMING, 1}; use_inventory(1); break;
        case 4: game.hunger = 0; game.turns = 3; end_turn(); break;
        case 5:
            monster(0, {11, 10}, MIMIC); game.monsters[0].stun = 0; game.speed = monster_speed(MIMIC) + 2;
            game.dexterity = 0; game.random_state = 1; end_turn(); break;
        case 6:
            game.inventory[0].info |= ITEM_CURSED; monster(0, {11, 10}); hit_seed(true); attack_monster(0); break;
        }
        require(game.hp == 81 && !session.ended && game.amulet_slot == NONE &&
                game.inventory[5].type == NO_ITEM && game.inventory[5].info == 0 && session.repeat_slot != 5,
                "Phoenix Heart failed damage source, precise healing or destruction");
        hurt_player(81); require(session.ended && game.hp == 0, "Phoenix Heart resurrected twice");
    }
    for(uint8_t max_hp : {1, 2, 4, 5, 254, 255}) {
        arena(); game.hp = 1; game.max_hp = max_hp; amulet(AMULET_PHOENIX_HEART); hurt_player(255);
        require(game.hp == max_hp, "Phoenix Heart full restoration or saturation incorrect");
    }
    arena(); amulet(AMULET_PHOENIX_HEART); hurt_player(1);
    require(game.amulet_slot == 5 && game.hp == 239, "Phoenix Heart activated on nonlethal damage");
    arena(); amulet(AMULET_PHOENIX_HEART, true); game.hp = 1;
    require(!use_inventory(5) && !drop_inventory(5), "cursed Phoenix Heart was removable");
    hurt_player(1); require(session.ended && game.inventory[5].type == AMULET_PHOENIX_HEART,
                            "cursed Phoenix Heart resurrected");
    arena(); game.max_hp = 40; game.hp = 20; game.inventory[5] = {AMULET_HEART_OF_GIANT, 63};
    use_inventory(5); require(player_max_hp() == 80 && game.hp == 20, "Giant Heart healed or scaled with info");
    heal_player(255); require(game.hp == 80, "Giant Heart restoration did not use effective maximum");
    use_inventory(5); require(game.hp == 40 && player_max_hp() == 40, "Giant Heart removal failed to clamp HP");
    use_inventory(5); game.vamp_drain = 10; require(player_max_hp() == 70, "Giant Heart lost life-force drain");
    game.inventory[5].info |= ITEM_CURSED;
    require(player_max_hp() == 18, "cursed Giant Heart max HP penalty incorrect");
    game.max_hp = 5; require(player_max_hp() == 1, "cursed Giant Heart maximum underflow");
    game.max_hp = 255; game.inventory[5].info &= ~ITEM_CURSED; game.vamp_drain = 0;
    require(player_max_hp() == 255, "Giant Heart max HP overflow");
    arena(); game.max_hp = 40; game.hp = 20; amulet(AMULET_HEART_OF_GIANT); gain_xp(24);
    require(game.max_hp > 40 && player_max_hp() == game.max_hp + 40 && game.hp < player_max_hp(),
            "Giant Heart level-up interaction failed");
    amulet(AMULET_VITALITY); game.inventory[5].info = 3;
    require(player_max_hp() == game.max_hp + 15, "existing vitality interaction changed");
}

void artifact_generation()
{
    using namespace rogue::generation;
    static_assert(sizeof(Item) == 2 && sizeof(Game) == 774 && SAVE_VERSION == 25 &&
                  WEAPON_LAST + 1 == ARMOR_FIRST && ARMOR_LAST + 1 == YENDOR_AMULET &&
                  RING_LAST + 1 == AMULET_FIRST && AMULET_LAST + 1 == SCROLL_FIRST,
                  "artifact groups or saved layout changed");
    unsigned selected[ARTIFACT_COUNT] = {};
    for(unsigned seed = 1; seed <= 65535; ++seed)
        for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) {
            uint8_t type = artifact_type(a), floor = artifact_floor(static_cast<uint16_t>(seed), type);
            require(is_artifact(type) && (is_equipment(type) || is_ring(type) || is_amulet(type)) &&
                    (floor == NONE || (floor >= 4 && floor <= 14)), "artifact classification/schedule invalid");
            selected[a] += floor != NONE;
        }
    for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a)
        binomial_frequency(selected[a], 65535, 1.0 / ARTIFACT_SELECTION_DENOMINATOR,
                           "artifact run selection frequency");
    for(unsigned seed = 1; seed <= 2048; ++seed) {
        bool found[ARTIFACT_COUNT] = {};
        for(uint8_t floor = 4; floor <= 14; ++floor) {
            arena(); game.run_seed = static_cast<uint16_t>(seed); game.floor = floor; make_floor();
            Game generated = game;
            require(game.random_state == 1, "artifact generation consumed gameplay RNG");
            make_floor(); require(!std::memcmp(&generated, &game, sizeof game), "artifact generation was not deterministic");
            std::memset(game.ground, 0, sizeof game.ground);
            populate_ordinary_items(floor_seed(SUPPLIES), floor_seed(EQUIPMENT));
            bool alternative = false;
            for(const auto& ground : game.ground) {
                require(!is_artifact(ground.item.type), "ordinary generation produced an artifact");
                alternative |= ground.item.type != FOOD && ground.item.type != ARROWS && ground.item.type != NO_ITEM;
            }
            for(uint8_t slot = 0; slot < GROUND_ITEMS; ++slot) {
                const GroundItem& item = generated.ground[slot];
                require(item.pos == game.ground[slot].pos, "artifact moved an ordinary accessible placement");
                if(!is_artifact(item.item.type)) {
                    require(item.item.type == game.ground[slot].item.type && item.item.info == game.ground[slot].item.info,
                            "artifact generation disturbed ordinary item stream"); continue;
                }
                require(!blocked(item.pos.x, item.pos.y) && item.pos != game.up && item.pos != game.down &&
                        (!alternative || (game.ground[slot].item.type != FOOD && game.ground[slot].item.type != ARROWS)),
                        "artifact placement inaccessible or unnecessarily replaced supplies");
                for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) if(artifact_type(a) == item.item.type) {
                    require(!found[a] && artifact_floor(static_cast<uint16_t>(seed), item.item.type) == floor,
                            "duplicate artifact or wrong generation floor");
                    found[a] = true;
                    if(is_equipment(item.item.type)) require(equipment_enchant(item.item) >= -2 &&
                        equipment_enchant(item.item) <= 2, "artifact enchantment out of generation range");
                }
            }
        }
        for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a)
            require(found[a] == (artifact_floor(static_cast<uint16_t>(seed), artifact_type(a)) != NONE),
                    "selected artifact lost in placement collision");
    }
    // Full seed-population item-state frequencies, with cheap open terrain.
    // Real generated floor accessibility/stream equality is tested above.
    unsigned full_placed[ARTIFACT_COUNT] = {}, full_curses[ARTIFACT_COUNT] = {};
    unsigned enchants[5][5] = {}, cursed_enchants[5][5] = {};
    for(unsigned seed = 1; seed <= 65535; ++seed) {
        unsigned floor_mask = 0;
        for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) {
            uint8_t floor = artifact_floor(static_cast<uint16_t>(seed), artifact_type(a));
            if(floor != NONE) floor_mask |= 1u << floor;
        }
        for(uint8_t floor = 4; floor <= 14; ++floor) {
            if(!(floor_mask & (1u << floor))) continue;
            arena(); game.run_seed = static_cast<uint16_t>(seed); game.floor = floor;
            populate_items(floor_seed(SUPPLIES), floor_seed(EQUIPMENT));
            for(const auto& ground : game.ground) {
                if(!is_artifact(ground.item.type)) continue;
                for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) if(artifact_type(a) == ground.item.type) {
                    ++full_placed[a]; full_curses[a] += item_is_cursed(ground.item);
                    if(a < 5) {
                        ++enchants[a][equipment_enchant(ground.item) + 2];
                        cursed_enchants[a][equipment_enchant(ground.item) + 2] += item_is_cursed(ground.item);
                    }
                }
            }
        }
    }
    const double probabilities[] = {.05, .10, .70, .10, .05};
    for(uint8_t a = 0; a < ARTIFACT_COUNT; ++a) {
        require(full_placed[a] == selected[a], "full-population artifact schedule lost a placement");
        binomial_frequency(full_curses[a], full_placed[a], 1.0 / ARTIFACT_CURSE_DENOMINATOR,
                           "full-population artifact curse frequency");
        if(a < 5) for(uint8_t e = 0; e < 5; ++e) {
            binomial_frequency(enchants[a][e], full_placed[a], probabilities[e],
                               "artifact equipment enchantment distribution");
            binomial_frequency(cursed_enchants[a][e], enchants[a][e], 1.0 / ARTIFACT_CURSE_DENOMINATOR,
                               "artifact enchant/curse independence");
        }
    }
    arena(); game.floor = 15; make_floor();
    require(!game.ground[15].item.type && game.monsters[MONSTERS - 1].type == LORD,
            "artifact generation overwrote Yendor reservation");
    defeat_monster(MONSTERS - 1); require(game.ground[15].item.type == YENDOR_AMULET, "Yendor drop lost");
    game.has_amulet = 1; game.floor = 7; make_floor();
    for(const auto& item : game.ground) require(!item.item.type, "artifacts regenerated on ascent");
    for(unsigned seed = 0; seed < 256; ++seed) {
        arena(); game.run_seed = static_cast<uint16_t>(seed);
        for(uint8_t first : {static_cast<uint8_t>(RING_FIRST), static_cast<uint8_t>(AMULET_FIRST)}) {
            unsigned mask = 0; uint8_t count = first == RING_FIRST ? RING_COUNT : AMULET_COUNT;
            for(uint8_t i = 0; i < count; ++i) {
                uint8_t type = first + i, appearance = item_appearance(type);
                require(appearance < count && !(mask & (1u << appearance)), "jewelry appearances not bijective");
                mask |= 1u << appearance; identify_type(type);
                require(item_type_identified(type), "artifact identification bit not addressable");
            }
        }
        for(uint8_t type = WAND_FIRST; type <= WAND_LAST; ++type) identify_type(type);
        require((game.identified_items[5] & 0xc0) == 0, "identification exceeded 46 bits");
    }
}
}

void check_artifacts()
{
    storm(); glass(); hammer(); dragonhide_and_speed(); hunt_and_reprisal(); hearts(); artifact_generation();
}
