#include "features.hpp"
#include "feature_icons.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "persistence.hpp"
#include <cstring>
#include <array>
#include <cstdio>
#include <string>

using namespace rogue;
void require(bool, const char*);
void reset_feature_status();
unsigned feature_monster_names();
const std::string& feature_status_text();
namespace {
void arena(uint8_t type = FEATURE_NONE) {
    game = {}; game.offhand_slot = NONE; session = {NONE, DEATH, false};
    game.player = {10, 10}; game.up = {2, 2}; game.down = {40, 20};
    game.floor = 4; game.hp = game.max_hp = 100; game.hunger = 220; game.level = 1;
    game.strength = 5; game.dexterity = 4; game.speed = 4;
    game.run_seed = game.random_state = 123; game.valid = 1;
    game.magic = SAVE_MAGIC; game.version = SAVE_VERSION;
    game.weapon_slot = game.armor_slot = game.amulet_slot = game.ring_slots[0] = game.ring_slots[1] = NONE;
    game.inventory[15] = {GLOWING_ORB, 0}; game.offhand_slot = 15;
    game.inventory[14] = {AMULET_BRILLIANCE, uint8_t(1 | ITEM_IDENTIFIED)}; game.amulet_slot = 14;
    if(type) game.features[0] = {game.player, static_cast<uint8_t>(type | (feature_is_trap(type) ? MAP_FEATURE_HIDDEN : 0))};
}
void trap_roster() {
    for(uint8_t type = FEATURE_SPIKE_TRAP; type <= FEATURE_RUST_TRAP; type += 2) {
        arena(type); uint8_t hp = game.hp;
        require(feature_is_hidden(game.features[0].id) && game.features[0].id == type + 1, "trap initial encoding");
        activate_entry_feature(0);
        require(game.features[0].id == type && !feature_is_hidden(game.features[0].id), "entry must reveal and retain trap");
        require(game.turns == 0, "trap resolver consumed turn");
        if(type == FEATURE_SPIKE_TRAP) require(hp - game.hp >= 3 && hp - game.hp <= 7, "spike range");
        if(type == FEATURE_POISON_DART_TRAP) require(hp - game.hp >= 1 && hp - game.hp <= 3 && game.weakened == 1, "poison dart effects");
        if(type == FEATURE_FLAME_TRAP) require(hp - game.hp >= 5 && hp - game.hp <= 9, "flame range");
        if(type == FEATURE_FROST_TRAP) require(hp - game.hp >= 3 && hp - game.hp <= 6 && game.slowed >= 4 && game.slowed <= 7, "frost effects");
        if(type == FEATURE_SNARE_TRAP) require(game.paralyzed >= 2 && game.paralyzed <= 3, "snare duration");
        if(type == FEATURE_CONFUSION_TRAP) require(game.confused >= 5 && game.confused <= 8, "confusion duration");
        activate_entry_feature(0);
        require(game.features[0].id == type, "repeat activation exhausted trap");
    }
    arena(FEATURE_POISON_DART_TRAP); game.weakened = 3; activate_entry_feature(0);
    require(game.weakened == 3, "weakness exceeded cap");
    arena(FEATURE_SPIKE_TRAP); game.inventory[0] = make_equipment(TITAN_PLATE, 5); game.armor_slot = 0;
    activate_entry_feature(0); require(game.hp == 99, "trap failed physical armor absorption");
    arena(FEATURE_MAGIC_DRAIN_TRAP); game.inventory[0] = {WAND_FIRE, ITEM_IDENTIFIED};
    set_wand_modifier(game.inventory[0], WAND_OVERPOWERED); set_wand_charges(game.inventory[0], 1);
    uint8_t flags = game.inventory[0].info & ~WAND_CHARGE_MASK;
    activate_entry_feature(0);
    require(wand_charges(game.inventory[0]) == 0 && game.hp == 100 &&
        (game.inventory[0].info & ~WAND_CHARGE_MASK) == flags, "drain corrupted wand");
    activate_entry_feature(0); require(game.hp < 100, "empty wand drain fallback");
    arena(FEATURE_RUST_TRAP); game.inventory[0] = make_equipment(LONG_SWORD, -5);
    game.inventory[0].info |= ITEM_CURSED | ITEM_IDENTIFIED; game.weapon_slot = 0;
    flags = game.inventory[0].info; activate_entry_feature(0);
    require(game.inventory[0].info == flags, "rust cap or flags");
    set_equipment_enchant(game.inventory[0], 0); activate_entry_feature(0);
    require(equipment_enchant(game.inventory[0]) == -1 && item_is_cursed(game.inventory[0]) && item_is_identified(game.inventory[0]), "rust helpers");
}
void protections_and_death() {
    arena(FEATURE_FLAME_TRAP); game.inventory[0] = make_equipment(DRAGONHIDE, 0); game.armor_slot = 0;
    activate_entry_feature(0); require(game.hp == 100, "Dragonhide fire immunity");
    arena(FEATURE_FLAME_TRAP); game.inventory[0] = {RING_FIRE_IMMUNITY, 1}; game.ring_slots[0] = 0;
    activate_entry_feature(0); require(game.hp == 100, "ring fire immunity");
    game.inventory[0].info |= ITEM_CURSED; activate_entry_feature(0);
    require(game.hp <= 90 && game.hp >= 82, "fire vulnerability");
    arena(FEATURE_SNARE_TRAP); game.inventory[0] = {AMULET_IRONBLOOD, 1}; game.amulet_slot = 0;
    activate_entry_feature(0); require(!game.paralyzed, "Ironblood snare prevention");
    arena(FEATURE_CONFUSION_TRAP); game.inventory[0] = {AMULET_CLARITY, 1}; game.amulet_slot = 0;
    activate_entry_feature(0); require(!game.confused, "Clarity trap protection");
    arena(); game.hp = 1; game.features[0] = {{11, 10}, FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    game.monsters[0] = {{12, 10}, GOBLIN, 100, 0, {0, 0}, MON_AGGRO};
    move_player(1, 0); require(session.ended && !game.turns && game.monsters[0].pos == Position{12, 10}, "lethal trap ran enemies");
    arena(); game.hp = 1; game.inventory[0] = {AMULET_PHOENIX_HEART, 1}; game.amulet_slot = 0;
    game.features[0] = {{11, 10}, FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    move_player(1, 0); require(!session.ended && game.hp > 0 && game.inventory[0].type == NO_ITEM && game.turns == 1, "trap Phoenix lifecycle");
    unsigned saved = 0;
    for(uint16_t seed = 1; seed <= 100; ++seed) {
        arena(FEATURE_FROST_TRAP); game.random_state = seed; game.magic_resistance = 127;
        activate_entry_feature(0); saved += !game.slowed;
        require(100 - game.hp <= 6, "magic damage bounds");
    }
    require(saved > 80, "frost failed magic saves");
}
void searching_and_movement() {
    arena(); uint16_t rng = game.random_state; search_traps();
    require(game.random_state == rng, "empty search used RNG");
    game.features[0] = {{11, 10}, FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    game.features[1] = {{12, 10}, FEATURE_FLAME_TRAP | MAP_FEATURE_HIDDEN};
    for(unsigned n = 0; n < 64 && feature_is_hidden(game.features[0].id); ++n) search_traps();
    require(game.features[0].id == FEATURE_SPIKE_TRAP && feature_is_hidden(game.features[1].id) && game.hp == 100, "search radius or activation");
    rng = game.random_state; search_traps(); require(game.random_state == rng, "discovered search used RNG");
    wait_search(); require(game.turns == 1 && game.hp == 100, "Wait/Search turn");
    arena(); game.features[0] = {{11, 10}, FEATURE_HEALING_SPRING}; game.hp = 20;
    begin_player_move(1, 0); require(session.pending_move && game.turns == 0 && game.hp == 20, "interaction must precede enemy turn");
    complete_player_move({false, NONE}); require(game.features[0].id && game.turns == 1, "decline movement turn");
    complete_player_move({true, NONE}); require(game.hp == 20 && game.turns == 1, "duplicate completion");
    move_player(-1, 0); begin_player_move(1, 0); complete_player_move({true, NONE});
    require(!game.features[0].id && game.turns == 3 && game.hp == 70, "accepted movement turn");
    arena(); game.features[0] = {{18, 10}, FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    force_player(1, 0); resolve_forced_entry();
    require(game.player == Position{18, 10} && game.features[0].id == FEATURE_SPIKE_TRAP && game.hp < 100 && !game.turns, "forced final trap landing");
    arena(FEATURE_TELEPORT_TRAP);
    for(uint8_t i = 1; i < MAP_FEATURES; ++i) game.features[i] = {{uint8_t(i + 15), 10}, FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    for(unsigned n = 0; n < 32; ++n) {
        Position origin = game.player; activate_entry_feature(0);
        require(game.player != origin && !blocked(game.player.x, game.player.y) && monster_at(game.player) == NONE &&
            game.player != game.up && game.player != game.down && game.hp == 100, "teleport placement or recursive activation");
    }
}
void summons_and_holy() {
    for(bool hidden: {false,true}) {
        arena(FEATURE_TELEPORT_TRAP); game.floor=15; game.features[0].id=FEATURE_TELEPORT_TRAP | (hidden ? MAP_FEATURE_HIDDEN : 0);
        game.monsters[0]={game.player,LORD,1,0,{0,0},0};
        defeat_monster(0);
        require(game.ground[GROUND_ITEMS - 1].item.type==YENDOR_AMULET && game.ground[GROUND_ITEMS - 1].pos==game.player &&
            game.features[0].id==(FEATURE_TELEPORT_TRAP | (hidden ? MAP_FEATURE_HIDDEN : 0)),"Lord drop or trap changed");
        game.player={9,10};
        begin_player_move(1,0);
        require(session.pending_pickups && !game.has_amulet && game.player==Position{10,10} && !game.turns,
            "teleport fired before pickup offer");
        require(take_item(GROUND_ITEMS - 1)==PICKUP_TAKEN && game.has_amulet && !game.turns,
            "Yendor pickup ran enemies before feature");
        complete_ground_pickups(); complete_player_move();
        require(!session.pending_move && !session.pending_pickups && game.turns==2 &&
            game.features[0].id==FEATURE_TELEPORT_TRAP,"pickup/teleport turn completion");
        arena(); game.floor=15; game.monsters[0]={{12,10},LORD,1,0,{0,0},0};
        damage_monster(0,1,false);
        require(game.ground[GROUND_ITEMS - 1].pos==Position{12,10},"ordinary Lord drop moved");
    }
    arena(FEATURE_SUMMONING_TRAP); activate_entry_feature(0);
    unsigned count = 0;
    for(uint8_t i = 0; i < HOSTILE_MONSTERS; ++i) if(game.monsters[i].type) {
        ++count; require(game.monsters[i].pos != game.player && (game.monsters[i].state & MON_TRAP_SUMMONED), "trap monster initialization");
        defeat_monster(i);
    }
    require(count >= 1 && count <= 2 && !game.score && !game.xp, "summoning XP farming");
    for(unsigned n = 0; n < 20; ++n) activate_entry_feature(0);
    count = 0; for(const Monster& m : game.monsters) count += bool(m.type);
    require(count == HOSTILE_MONSTERS, "summoning capacity");
    auto monsters = std::array<Monster, MONSTERS>{}; std::memcpy(monsters.data(), game.monsters, sizeof game.monsters);
    activate_entry_feature(0); require(!std::memcmp(monsters.data(), game.monsters, sizeof game.monsters), "full monster array overwritten");
    arena(FEATURE_HOLY_SYMBOL);
    const uint8_t types[] = {ZOMBIE, PHANTOM, INCUBUS, ANGEL, LORD, GOBLIN};
    for(unsigned i = 0; i < 6; ++i) game.monsters[i] = {{uint8_t(9 + i % 3), uint8_t(9 + i / 3)}, types[i], 100, 0, {0, 0}, 0};
    game.monsters[6] = {{13, 10}, ZOMBIE, 100, 0, {0, 0}, 0};
    activate_entry_feature(0);
    for(unsigned i = 0; i < 5; ++i) require(game.monsters[i].hp >= 95 && game.monsters[i].hp <= 97 &&
        bool(game.monsters[i].state & MON_AFRAID) == (types[i] != LORD), "holy target damage/fear");
    require(game.monsters[5].hp == 100 && game.monsters[6].hp == 100, "holy ordinary monster/radius");
    uint8_t hp = game.monsters[0].hp; game.speed = 1; for(auto& m : game.monsters) m.stun = 15;
    wait_search(); require(game.monsters[0].hp == hp, "stationary holy pulse");
    activate_entry_feature(0); require(game.monsters[0].hp < hp && game.features[0].id == FEATURE_HOLY_SYMBOL, "holy repeat");
    game.monsters[0].hp = 1; activate_entry_feature(0);
    require(!game.monsters[0].type && game.score > 0 && game.xp > 0, "holy normal kill rewards");
    require(undead_or_demonic(ZOMBIE) && undead_or_demonic(PHANTOM) && undead_or_demonic(INCUBUS) &&
        undead_or_demonic(ANGEL) && undead_or_demonic(LORD) && !undead_or_demonic(TROLL), "unholy classification");
}
void pickup_order() {
    arena(); game.hp=1; game.features[0]={{11,10},FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    game.ground[0]={{11,10},{FOOD,1}};
    begin_player_move(1,0);
    require(session.pending_pickups && !session.ended && game.features[0].id&MAP_FEATURE_HIDDEN,
        "lethal trap fired before pickup offer");
    require(take_item(0)==PICKUP_TAKEN && game.inventory[0].type==FOOD && !game.turns,"pickup before lethal trap");
    complete_ground_pickups(); complete_player_move();
    require(session.ended && !game.turns && !session.pending_move,"lethal entry ran queued turns");
    arena(); game.features[0]={{11,10},FEATURE_HEALING_SPRING}; game.ground[0]={{11,10},{FOOD,1}};
    begin_player_move(1,0); complete_ground_pickups(); complete_player_move();
    require(game.ground[0].item.type==FOOD && game.turns==1 && game.features[0].id,
        "declined pickup/feature consumed item or extra turn");
    arena(); game.features[0]={{11,10},FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
    for(auto& i:game.inventory) i={FOOD,1};
    game.ground[0]={{11,10},{POTION_HEALING,1}};
    begin_player_move(1,0);
    require(take_item(0)==PICKUP_NEEDS_SWAP && swap_ground_item(0,0) && game.hp==100 && !game.turns,
        "full pack swap resolved trap before pickup");
    complete_ground_pickups(); complete_player_move();
    require(game.inventory[0].type==POTION_HEALING && game.hp<100 && game.turns==2,"swap/feature turn order");
}
uint16_t seed_for(uint8_t limit, uint8_t value) {
    for(uint32_t seed = 1; seed <= 65535; ++seed) {
        uint16_t state = static_cast<uint16_t>(seed);
        if(next_random(state) % limit == value) return static_cast<uint16_t>(seed);
    }
    require(false, "unreachable outcome seed"); return 0;
}
void outcome_checks() {
    // Force each weighted fountain interval through the production resolver.
    for(uint8_t chance : {uint8_t(0), uint8_t(25), uint8_t(40), uint8_t(55), uint8_t(70), uint8_t(80), uint8_t(85), uint8_t(90), uint8_t(95)}) {
        arena(FEATURE_FOUNTAIN); game.hp=20; game.hunger=100; game.random_state=seed_for(100,chance);
        game.inventory[0]=make_equipment(LONG_SWORD,0);
        if(chance==40) game.confused=game.slowed=game.weakened=3;
        require(resolve_feature(0,{true,NONE}) && !game.features[0].id,"fountain outcome consumption");
        if(chance==0) require(game.hp==45,"fountain quarter healing");
        if(chance==25) require(game.hunger==140,"fountain hunger");
        if(chance==40) require(!game.confused && !game.weakened && !game.slowed,"fountain cure");
        if(chance==55) require(game.confused>=5 && game.confused<=8,"fountain confusion");
        if(chance==70) require(game.weakened==1,"fountain weakness");
        if(chance==80) require(game.strength+game.dexterity==10,"fountain stat gain");
        if(chance==85) require(game.monsters[0].type && !(game.monsters[0].state & MON_TRAP_SUMMONED),"fountain hostile summon");
        if(chance==90) require(item_is_identified(game.inventory[0]),"fountain item identification");
        if(chance==95) require(game.player!=Position{10,10},"fountain teleport");
    }
    for(uint8_t outcome=0;outcome<8;++outcome) {
        arena(FEATURE_ALTAR); game.hp=20; game.random_state=seed_for(8,outcome);
        game.inventory[0]=make_equipment(LONG_SWORD,0); game.inventory[0].info|=ITEM_CURSED;
        resolve_feature(0,{true,NONE});
        if(outcome==0) require(game.monsters[12].type>=HOUND && game.monsters[12].type<=WISP && game.hp==20,"altar companion outcome");
        else if(outcome<3) require(game.hp==45,"altar healing weights");
        else if(outcome<5) require(!item_is_cursed(game.inventory[0]),"altar uncurse weights");
        else if(outcome==5) require(item_is_identified(game.inventory[0]),"altar identify");
        else if(outcome==6) require(game.weakened || game.confused,"altar affliction");
        else require(game.monsters[0].type,"altar summon");
    }
    for(uint8_t outcome=0;outcome<4;++outcome) {
        arena(FEATURE_RUNESTONE); game.hp=20; game.random_state=seed_for(4,outcome);
        game.confused=game.weakened=game.slowed=3;
        resolve_feature(0,{true,NONE});
        if(outcome==0) require(game.hp==45,"runestone healing");
        if(outcome==1) require(!game.confused && !game.weakened && !game.slowed,"runestone cure");
        if(outcome==2) require(game.magic_resistance==1,"runestone resistance");
        if(outcome==3) require(game.hp>=14 && game.hp<=17,"runestone damage");
        arena(FEATURE_SARCOPHAGUS); game.random_state=seed_for(4,outcome); game.floor=6;
        resolve_feature(0,{true,NONE});
        if(outcome<2) require(game.monsters[0].type==PHANTOM && game.monsters[0].pos!=game.player,"sarcophagus undead");
        if(outcome==2) require(game.ground[0].item.type && game.ground[0].pos==game.player,"sarcophagus item");
        if(outcome==3) require(game.score==25,"sarcophagus treasure");
    }
    arena(FEATURE_TELEPORT_TRAP); std::memset(game.walls,0xff,sizeof game.walls);
    carve(10,10); carve(11,10); game.up={11,10}; game.down={10,10};
    activate_entry_feature(0); require(game.player==Position{11,10},"sparse teleport fallback");
}
void interactive_roster() {
    for(uint8_t type = FEATURE_FOUNTAIN; type <= FEATURE_ALCHEMICAL_POOL; type += 2) {
        arena(type); Game before = game;
        require(!resolve_feature(0, {}) && !std::memcmp(&game, &before, sizeof game), "decline mutated feature/state/RNG");
        game.inventory[0] = make_equipment(LONG_SWORD, 0);
        if(type == FEATURE_SACRIFICIAL_ALTAR || type == FEATURE_ENCHANTED_FORGE || type == FEATURE_ALCHEMICAL_POOL) {
            before = game;
            require(!resolve_feature(0, {true, NONE}) && !std::memcmp(&game, &before, sizeof game), "item cancellation consumed feature/RNG");
        }
        require(resolve_feature(0, {true, 0}) && !game.features[0].id && game.features[0].pos == Position{0, 0}, "successful feature consumption");
        before = game; require(!resolve_feature(0, {true, 0}) && !std::memcmp(&before, &game, sizeof game), "duplicate feature reward");
        std::array<uint8_t, sizeof(Game)> saved{}; std::memcpy(saved.data(), &game, sizeof game);
        game = {}; game.offhand_slot = NONE; std::memcpy(&game, saved.data(), sizeof game);
        require(restore_startup_save(true) && !game.features[0].id, "consumption save/load");
    }
    arena(FEATURE_HEALING_SPRING); game.hp = 90; game.weakened = game.confused = game.slowed = 3;
    resolve_feature(0, {true, NONE}); require(game.hp == 100 && game.max_hp == 100 && !game.weakened && !game.confused && !game.slowed, "healing spring cap/cures");
    arena(FEATURE_SACRIFICIAL_ALTAR); game.inventory[0] = make_equipment(LONG_SWORD, 0); game.weapon_slot = 0;
    game.inventory[0].info |= ITEM_CURSED;
    require(!resolve_feature(0, {true, 0}) && game.features[0].id, "cursed equipped sacrifice");
    game.inventory[0].info &= ~ITEM_CURSED; session.repeat_slot = 0;
    require(resolve_feature(0, {true, 0}) && game.weapon_slot == NONE && session.repeat_slot == NONE && !game.inventory[0].type, "sacrifice slot cleanup");
    arena(FEATURE_SACRIFICIAL_ALTAR); game.inventory[0] = {YENDOR_AMULET, 1};
    require(!resolve_feature(0, {true, 0}), "Yendor sacrificed");
    arena(FEATURE_ALCHEMICAL_POOL); game.inventory[0] = {ARROWS, 255};
    require(!resolve_feature(0, {true, 0}) && game.inventory[0].info == 255, "pool corrupted ammo");
    for(uint16_t seed = 1; seed <= 200; ++seed) {
        arena(FEATURE_ENCHANTED_FORGE); game.random_state = seed; game.inventory[0] = make_equipment(PLATE_MAIL, 5);
        game.inventory[0].info |= ITEM_IDENTIFIED; resolve_feature(0, {true, 0});
        require(equipment_enchant(game.inventory[0]) <= 5 && equipment_enchant(game.inventory[0]) >= 4 && item_is_identified(game.inventory[0]), "forge enchant bounds/flags");
        arena(FEATURE_ALCHEMICAL_POOL); game.random_state = seed; game.inventory[0] = {WAND_ICE, ITEM_IDENTIFIED};
        set_wand_modifier(game.inventory[0], WAND_OVERPOWERED); set_wand_charges(game.inventory[0], 14);
        resolve_feature(0, {true, 0}); require(wand_charges(game.inventory[0]) <= 15 && item_is_identified(game.inventory[0]), "pool wand bounds/identification");
        arena(FEATURE_STATUE); game.random_state = seed;
        for(auto& g : game.ground) g = {{20, 20}, {FOOD, 1}};
        std::array<GroundItem, GROUND_ITEMS> before; std::memcpy(before.data(), game.ground, sizeof game.ground);
        resolve_feature(0, {true, NONE}); require(!std::memcmp(before.data(), game.ground, sizeof game.ground), "statue full ground overwrite");
    }
}
void teleport_topology() {
    auto layout=[](std::initializer_list<const char*> rows) {
        arena(); std::memset(game.walls,0xff,sizeof game.walls);
        unsigned y=8;
        for(auto row:rows) { for(unsigned x=0;row[x];++x) if(row[x]=='.') carve(uint8_t(8+x),uint8_t(y)); ++y; }
    };
    layout({"...#...",".......","...#..."});
    require(!generation::teleport_tile_safe({11,9}),"room connector accepted");
    layout({"##.##","##.##",".....","##.##","##.##"});
    require(!generation::teleport_tile_safe({10,10}),"severed junction accepted");
    layout({".....",".....",".....",".....","....."});
    require(generation::teleport_tile_safe({10,10}),"open room rejected");
    layout({".....",".###.","....."});
    require(generation::teleport_tile_safe({10,8}),"loop corridor rejected");
    layout({"....."});
    require(generation::teleport_tile_safe({8,8}),"dead end rejected");
    require(!generation::teleport_tile_safe({10,8}),"corridor choke accepted");
    game.door_count=1; game.doors[0].pos={10,8};
    require(!generation::teleport_tile_safe({10,8}),"closed doorway choke accepted");
    layout({".....",".###.","....."});
    game.door_count=1; game.doors[0].pos={8,9};
    require(generation::teleport_tile_safe({10,8}),"openable door omitted from loop topology");
    // Every interior cell is essential; endpoints are stairs/spawn.
    layout({"........................................"});
    game.player=game.up={8,8}; game.down={47,8};
    for(unsigned seed=1;seed<300;++seed) {
        std::memset(game.features,0,sizeof game.features);
        generation::populate_features(uint16_t(seed));
        for(auto f:game.features) require(feature_type(f.id)!=FEATURE_TELEPORT_TRAP,"unsafe teleport fallback");
    }
    for(unsigned seed=1;seed<=256;++seed) for(uint8_t floor: {uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(3),uint8_t(4)}) {
        arena(FEATURE_SPIKE_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(100-game.hp >= (floor<2 ? 2 : 3) && 100-game.hp <= (floor<2 ? 4 : 7),"depth spike damage");
        arena(FEATURE_POISON_DART_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(100-game.hp<= (floor<2 ? 2 : 3) && game.weakened==1,"depth poison damage/weakness");
        arena(FEATURE_CONFUSION_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(game.confused>= (floor<2 ? 3 : 5) && game.confused<= (floor<2 ? 5 : 8),"depth confusion duration");
        arena(FEATURE_FLAME_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(100-game.hp>= (floor<4 ? 3 : 5) && 100-game.hp<= (floor<4 ? 5 : 9),"depth flame damage");
        arena(FEATURE_FROST_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(100-game.hp>= (floor<4 ? 2 : 3) && 100-game.hp<= (floor<4 ? 4 : 6),"depth frost damage");
        arena(FEATURE_SNARE_TRAP); game.floor=floor; game.random_state=uint16_t(seed); activate_entry_feature(0);
        require(game.paralyzed>= (floor<4 ? 1 : 2) && game.paralyzed<= (floor<4 ? 2 : 3),"depth snare duration");
    }
}

void monster_traps() {
    auto setup=[](uint8_t trap, uint8_t kind=GOBLIN, Position pos={12,10}) {
        arena(); game.monsters[0]={pos,kind,100,0,{0,0},0};
        game.features[0]={pos,uint8_t(trap|MAP_FEATURE_HIDDEN)};
    };
    for(unsigned seed=1;seed<=256;++seed) for(uint8_t depth: {uint8_t(0),uint8_t(1),uint8_t(3),uint8_t(4),uint8_t(15)})
        for(uint8_t trap=FEATURE_SPIKE_TRAP;trap<=FEATURE_RUST_TRAP;trap+=2) {
            setup(trap); game.floor=depth; game.random_state=uint16_t(seed);
            Monster before=game.monsters[0]; activate_monster_trap(0); const auto& m=game.monsters[0];
            require(game.features[0].id==trap && game.turns==0 && game.hp==100 && !game.score && !game.xp,
                    "monster trap altered player/turn/feature state");
            require(!(m.state&MON_AGGRO),"environmental damage provoked aggression");
            unsigned damage=100-m.hp;
            if(trap==FEATURE_SPIKE_TRAP || trap==FEATURE_POISON_DART_TRAP || trap==FEATURE_RUST_TRAP) {
                game.random_state=uint16_t(seed);
                uint8_t raw=trap==FEATURE_SPIKE_TRAP ? (depth<2 ? 2+roll(3) : 3+roll(5)) :
                    trap==FEATURE_POISON_DART_TRAP ? 1+roll(depth<2 ? 2:3) : 2+roll(3);
                require(damage==physical_damage_after_armor(raw,armor_absorption(monster_armor(GOBLIN),0)),"monster physical trap armor/roll");
            }
            if(trap==FEATURE_POISON_DART_TRAP) require(monster_effect(m,MON_WEAKENED)==15,"dart weakness packing");
            if(trap==FEATURE_FLAME_TRAP) require(damage>=(depth<4 ? 3:5) && damage<=(depth<4 ? 5:9),"monster flame range");
            if(trap==FEATURE_FROST_TRAP) require(damage>=(depth<4 ? 2:3) && damage<=(depth<4 ? 4:6) &&
                monster_effect(m,MON_SLOWED)>=6 && monster_effect(m,MON_SLOWED)<=10,"monster frost damage/slow");
            if(trap==FEATURE_MAGIC_DRAIN_TRAP) require(damage>=3 && damage<=6,"monster backlash range");
            if(trap==FEATURE_CONFUSION_TRAP) require(monster_effect(m,MON_CONFUSED)>=5 && monster_effect(m,MON_CONFUSED)<=8,"monster confusion packing");
            if(trap==FEATURE_SNARE_TRAP) require(m.stun>=2 && m.stun<=3 && !damage && m.state==before.state,"monster snare");
            if(trap==FEATURE_TELEPORT_TRAP) require(m.pos!=before.pos && !blocked(m.pos.x,m.pos.y) && m.pos!=game.player &&
                m.pos!=game.up && m.pos!=game.down && m.hp==before.hp && m.effects[0]==0 && m.effects[1]==0,"monster teleport invariants");
            if(trap==FEATURE_SUMMONING_TRAP) {
                unsigned count=0;
                for(unsigned i=1;i<HOSTILE_MONSTERS;++i) if(game.monsters[i].type) {
                    ++count; const auto& spawn=game.monsters[i];
                    require(square_contains(before.pos,spawn.pos,2) && spawn.pos!=game.player && !blocked(spawn.pos.x,spawn.pos.y) &&
                        (spawn.state&MON_TRAP_SUMMONED) && monster_at(spawn.pos)==i,"monster-origin summon");
                }
                require(count>=1 && count<=2 && !damage,"monster summon count");
            }
        }
    setup(FEATURE_FLAME_TRAP,DRAGON); activate_monster_trap(0); require(game.monsters[0].hp==100,"dragon trap immunity");
    for(uint8_t trap: {uint8_t(FEATURE_CONFUSION_TRAP),uint8_t(FEATURE_FROST_TRAP),uint8_t(FEATURE_SNARE_TRAP)}) {
        setup(trap); game.monsters[0].effects[0]=255; game.monsters[0].effects[1]=255; game.monsters[0].stun=15;
        activate_monster_trap(0); require(game.monsters[0].effects[0]==255 && game.monsters[0].effects[1]==255 &&
            game.monsters[0].stun==15,"trap shortened packed status/stun");
    }
    for(uint8_t trap: {uint8_t(FEATURE_SPIKE_TRAP),uint8_t(FEATURE_POISON_DART_TRAP),uint8_t(FEATURE_FLAME_TRAP),
        uint8_t(FEATURE_FROST_TRAP),uint8_t(FEATURE_MAGIC_DRAIN_TRAP),uint8_t(FEATURE_RUST_TRAP)}) {
        for(uint8_t hp: {uint8_t(0),uint8_t(1)}) {
            setup(trap,LORD); game.monsters[0].hp=hp; activate_monster_trap(0);
            require(!game.monsters[0].type && !game.score && !game.xp && game.ground[GROUND_ITEMS - 1].item.type==YENDOR_AMULET &&
                game.ground[GROUND_ITEMS - 1].pos==Position{12,10},"environmental Lord death/Yendor/underflow");
            auto ground=game.ground[GROUND_ITEMS - 1]; activate_monster_trap(0);
            require(!std::memcmp(&ground,&game.ground[GROUND_ITEMS - 1],sizeof ground),"duplicate Yendor");
        }
    }
    setup(FEATURE_SPIKE_TRAP); game.monsters[0].hp=1; game.monsters[0].state=MON_TRAP_SUMMONED; activate_monster_trap(0);
    require(!game.monsters[0].type && !game.score && !game.xp,"summoned trap kill credit");
    setup(FEATURE_SUMMONING_TRAP);
    for(uint8_t i=1;i<HOSTILE_MONSTERS;++i) game.monsters[i]={{uint8_t(20+i),20},GOBLIN,42,7,{255,255},MON_AGGRO};
    Game full=game; activate_monster_trap(0);
    require(!std::memcmp(full.monsters,game.monsters,sizeof game.monsters),"full summon overwrote monster");
    game.monsters[1].type=0; activate_monster_trap(0);
    require(game.monsters[1].type && game.monsters[1].hp==monster_health(game.monsters[1].type) &&
        !game.monsters[1].stun && !game.monsters[1].effects[0] && !game.monsters[1].effects[1],"summon reused stale monster state");
    setup(FEATURE_TELEPORT_TRAP); std::memset(game.walls,255,sizeof game.walls);
    carve(12,10); carve(13,10); carve(14,10); game.doors[0].pos={13,10}; game.door_count=1;
    game.features[1]={{14,10},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    game.monsters[0].stun=9; game.monsters[0].effects[0]=0xa7; game.monsters[0].effects[1]=0xc5;
    activate_monster_trap(0);
    require(game.monsters[0].pos==Position{14,10} && game.monsters[0].hp==100 && game.monsters[0].stun==9 &&
        game.monsters[0].effects[0]==0xa7 && game.monsters[0].effects[1]==0xc5 && feature_is_hidden(game.features[1].id),"sparse teleport/destination recursion");
    game.up={12,10}; game.down={14,10}; game.monsters[0].pos={12,10};
    require(teleport_monster(0,false) && game.monsters[0].pos==Position{14,10},"teleport stair fallback");
    game.monsters[1]={{12,10},GOBLIN,10,0,{0,0},0};
    require(!teleport_monster(0,false) && game.monsters[0].pos==Position{14,10},"sealed teleport overlap/origin");
    setup(FEATURE_SPIKE_TRAP,GOBLIN,{40,20}); reset_feature_status(); activate_monster_trap(0);
    require(feature_status_text().empty() && !feature_monster_names(),"offscreen trap message");
    require(game.monsters[0].hp<100 && feature_is_hidden(game.features[0].id),"offscreen monster revealed trap");
    setup(FEATURE_SPIKE_TRAP,PHANTOM); reset_feature_status(); activate_monster_trap(0);
    require(!feature_monster_names() && feature_status_text().find("A trap activates!")!=std::string::npos,"invisible monster identity leak");
    require(!feature_is_hidden(game.features[0].id) && !player_can_see_monster(0),"invisible monster must reveal visible tile");
    setup(FEATURE_SPIKE_TRAP); reset_feature_status(); activate_monster_trap(0);
    require(feature_monster_names()==1,"visible trap monster feedback");
    game.monsters[0].hp=1; reset_feature_status(); activate_monster_trap(0);
    require(feature_status_text().find("dies in a trap!")!=std::string::npos && feature_status_text().find("You defeat")==std::string::npos,"trap death feedback");
    game.player={40,20}; activate_monster_trap(0); require(game.features[0].id==FEATURE_SPIKE_TRAP,"revealed trap rehidden");
    Game invalid=game; activate_monster_trap(NONE); activate_monster_trap(MONSTERS);
    require(!std::memcmp(&invalid,&game,sizeof game),"invalid monster trap index");
    setup(FEATURE_HOLY_SYMBOL); Game holy=game; activate_monster_trap(0);
    require(!std::memcmp(&holy,&game,sizeof game),"monster activated nontrap");
}
void monster_entry_checks() {
    auto setup=[](Position pos={12,10},uint8_t kind=GOBLIN) {
        arena(); game.monsters[0]={pos,kind,100,0,{0,0},MON_AGGRO};
        game.features[0]={{11,10},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    };
    setup(); end_turn(); require(game.monsters[0].pos==Position{11,10} && game.monsters[0].hp<100 &&
        game.features[0].id==FEATURE_SPIKE_TRAP && game.turns==1,"ordinary pursuit trap entry");
    setup(); game.features[0].id=FEATURE_SPIKE_TRAP; end_turn(); require(game.monsters[0].pos==Position{11,10},"discovered trap avoidance");
    setup({11,10}); end_turn(); require(game.monsters[0].hp==100 && feature_is_hidden(game.features[0].id),"stationary attack trap");
    setup({11,10}); game.monsters[0].stun=2; end_turn(); require(game.monsters[0].hp==100 && game.monsters[0].stun==1,"stationary stun trap");
    setup({11,10},MIMIC); game.monsters[0].state=0; end_turn(); require(game.monsters[0].hp==100,"immobile monster trap");
    setup(); game.walls[(10*MAP_W+11)>>3]|=uint8_t(1u<<(11&7)); end_turn();
    require(game.monsters[0].hp==100 && game.monsters[0].pos==Position{12,10},"blocked movement trap");
    setup(); game.doors[0].pos={11,10}; game.door_count=1; end_turn();
    require(door_open(0) && game.monsters[0].hp==100 && game.monsters[0].pos==Position{12,10},"door opening trap");
    for(bool confused: {false,true}) {
        bool tested=false;
        for(unsigned seed=1;seed<200 && !tested;++seed) {
            setup({12,10},BAT); game.monsters[0].state=0; game.speed=6; game.random_state=uint16_t(seed);
            if(confused) set_monster_effect(game.monsters[0],MON_CONFUSED,8);
            end_turn(); if(game.monsters[0].pos==Position{11,10}) {
                require(game.monsters[0].hp<100 && game.features[0].id==FEATURE_SPIKE_TRAP,"wandering/confused trap entry"); tested=true;
            }
        }
        require(tested,"random entry seed coverage");
    }
    setup({12,10}); game.monsters[0].state=MON_AFRAID; game.features[0].pos={13,10}; end_turn();
    require(game.monsters[0].pos==Position{13,10} && game.monsters[0].hp<100,"fleeing trap entry");
    uint8_t hp=game.monsters[0].hp; game.player={20,10}; game.monsters[0].state=MON_AGGRO;
    end_turn(); require(game.monsters[0].pos==Position{14,10},"trap exit"); game.player={10,10}; end_turn();
    require(game.monsters[0].pos==Position{13,10} && game.monsters[0].hp<hp,"trap reentry");
    setup({12,10}); game.features[0].pos={15,10}; game.features[1]={{13,10},FEATURE_MAGIC_DRAIN_TRAP|MAP_FEATURE_HIDDEN};
    game.walls[(10*MAP_W+16)>>3]|=uint8_t(1u<<(16&7)); force_monster(0,1,0);
    require(game.monsters[0].pos==Position{15,10} && game.monsters[0].hp>=93 && game.monsters[0].hp<=97 &&
        game.features[0].id==FEATURE_SPIKE_TRAP && feature_is_hidden(game.features[1].id) && game.monsters[0].stun==4,"knockback final tile only");
    setup({12,10}); game.features[0].pos={12,10}; game.walls[(10*MAP_W+13)>>3]|=uint8_t(1u<<(13&7)); force_monster(0,1,0);
    require(game.monsters[0].hp==100 && feature_is_hidden(game.features[0].id),"failed knockback trap");
    setup(); game.speed=24; game.monsters[0].hp=1; end_turn();
    require(!game.monsters[0].type && game.hp==100 && game.turns==1 && !game.score && !game.xp,"dead fast monster continued actions");
    setup({12,10},TROLL); game.speed=24; game.monsters[0].hp=1; end_turn(); require(!game.monsters[0].type,"trap-killed troll regenerated");
    setup(); game.speed=24; game.features[0].id=FEATURE_SNARE_TRAP|MAP_FEATURE_HIDDEN; end_turn();
    require(game.monsters[0].pos==Position{11,10} && game.monsters[0].hp==100,
        "snared fast monster continued moving");
    // Exercise the public wand and melee entry points, including deferred rays.
    setup(); game.monsters[0].stun=15; game.speed=1;
    game.walls[(10*MAP_W+17)>>3]|=uint8_t(1u<<(17&7));
    game.features[0].pos={16,10}; game.features[1]={{13,10},FEATURE_MAGIC_DRAIN_TRAP|MAP_FEATURE_HIDDEN};
    game.inventory[0]={WAND_FORCE,0}; set_wand_charges(game.inventory[0],3);
    require(use_wand(0,1,0) && game.monsters[0].pos==Position{16,10} && game.monsters[0].hp<100 &&
        game.features[0].id==FEATURE_SPIKE_TRAP && feature_is_hidden(game.features[1].id),"wand landing trap integration");
    bool hammer=false;
    for(unsigned seed=1;seed<500 && !hammer;++seed) {
        setup({11,10},SNAKE); game.monsters[0].stun=15; game.features[0].pos={15,10};
        game.walls[(10*MAP_W+16)>>3]|=uint8_t(1u<<(16&7));
        game.inventory[0]=make_equipment(HAMMER_OF_RUIN,0); game.weapon_slot=0; game.random_state=uint16_t(seed);
        attack_monster(0);
        if(game.monsters[0].pos==Position{15,10}) {
            require(game.features[0].id==FEATURE_SPIKE_TRAP,"Hammer landing trap integration"); hammer=true;
        }
    }
    require(hammer,"Hammer heavy hit coverage");
    setup({11,10}); game.monsters[0].stun=15; game.features[0].pos={11,10}; game.speed=1;
    game.inventory[0]={WAND_POLYMORPH,0}; set_wand_charges(game.inventory[0],3);
    require(use_wand(0,1,0) && feature_is_hidden(game.features[0].id),"stationary polymorph triggered trap");

    // A summoned monster waits until the next enemy turn, even in a reused
    // higher slot. Its fresh packed duration remains untouched this turn.
    setup(); game.speed=4; game.features[0].id=FEATURE_SUMMONING_TRAP|MAP_FEATURE_HIDDEN;
    for(uint8_t i=1;i<MAP_FEATURES;++i) game.features[i]={{uint8_t(8+i%5),uint8_t(8+i/5)},FEATURE_SUMMONING_TRAP|MAP_FEATURE_HIDDEN};
    end_turn(); unsigned count=0;
    for(uint8_t i=1;i<HOSTILE_MONSTERS;++i) if(game.monsters[i].type) {
        ++count; require(game.monsters[i].hp==monster_health(game.monsters[i].type) && (game.monsters[i].state&MON_TRAP_SUMMONED),"new summon action/state");
    }
    require(count>=1 && count<=2,"new summon recursively acted this turn");
    // Assigning/generated monsters is not an entry event.
    setup({11,10},MIMIC); game.monsters[0].state=MON_TRAP_SUMMONED; end_turn(); require(game.monsters[0].hp==100,"spawn triggered trap");
    // No-feature and unrelated feature movement are byte-identical, including RNG.
    for(unsigned seed=1;seed<=256;++seed) {
        setup(); game.features[0]={}; game.random_state=uint16_t(seed); Game start=game;
        end_turn(); Game empty=game; game=start; game.features[0]={{40,20},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN}; end_turn();
        game.features[0]={}; require(!std::memcmp(&empty,&game,sizeof game),"unrelated trap changed movement/status/RNG");
    }
}
void generation_checks() {
    unsigned roster[FEATURE_TYPES + 1] = {}; unsigned archetypes[4] = {};
    for(uint16_t seed = 1; seed <= 4096; ++seed) {
        arena(); game.run_seed = seed; game.floor = seed % FLOORS; make_floor();
        Game first = game; uint16_t rng = game.random_state;
        ++archetypes[generation::archetype(generation::floor_seed(generation::LAYOUT))];
        make_floor(); require(!std::memcmp(&first, &game, sizeof game) && game.random_state == rng, "feature generation determinism/gameplay RNG isolation");
        // Host reachability scratch only; doors are traversable topology.
        std::array<bool, MAP_W * MAP_H> visited{}; std::array<Position, MAP_W * MAP_H> queue{};
        unsigned head = 0, tail = 0; queue[tail++] = game.player; visited[game.player.y * MAP_W + game.player.x] = true;
        while(head < tail) {
            Position pos = queue[head++];
            const int8_t dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
            for(unsigned d = 0; d < 4; ++d) {
                int x = pos.x + dx[d], y = pos.y + dy[d]; if(wall_at(x, y)) continue;
                unsigned cell = y * MAP_W + x; if(visited[cell]) continue;
                visited[cell] = true; queue[tail++] = {uint8_t(x), uint8_t(y)};
            }
        }
        unsigned count = 0, holy = 0;
        for(uint8_t i = 0; i < MAP_FEATURES; ++i) {
            const MapFeature& f = game.features[i]; if(!f.id) continue; ++count;
            uint8_t type = feature_type(f.id); ++roster[type / 2]; holy += type == FEATURE_HOLY_SYMBOL;
            require(type && type <= FEATURE_HOLY_SYMBOL && (type & 1) == 0 &&
                feature_is_hidden(f.id) == feature_is_trap(f.id), "generated feature ID");
            require(!wall_at(f.pos.x, f.pos.y) && door_at(f.pos) == NONE && f.pos != game.up && f.pos != game.down &&
                f.pos != game.player && item_at(f.pos) == NONE && monster_at(f.pos) == NONE && visited[f.pos.y * MAP_W + f.pos.x], "generated feature collision/reachability");
            if(feature_is_trap(f.id)) {
                int dx = f.pos.x > game.player.x ? f.pos.x - game.player.x : game.player.x - f.pos.x;
                int dy = f.pos.y > game.player.y ? f.pos.y - game.player.y : game.player.y - f.pos.y;
                require(dx + dy > 3, "spawn trap safety");
            }
            for(uint8_t j = 0; j < i; ++j) require(!game.features[j].id || game.features[j].pos != f.pos, "features overlap");
        }
        unsigned minimum = game.floor < 2 ? 2 : game.floor < 4 ? 3 : 4;
        unsigned maximum = game.floor < 2 ? 4 : game.floor < 4 ? 5 : 8;
        require(count >= minimum - 1 && count <= maximum && holy <= 1, "feature count/holy cap");
        for(const auto& trap : game.features) if(feature_type(trap.id) == FEATURE_TELEPORT_TRAP) {
            require(generation::teleport_tile_safe(trap.pos), "generated unsafe teleport");
            std::array<bool, MAP_W * MAP_H> without{};
            head=tail=0; queue[tail++]=game.player; without[game.player.y*MAP_W+game.player.x]=true;
            while(head<tail) {
                auto pos=queue[head++];
                const int8_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
                for(unsigned d=0;d<4;++d) {
                    int x=pos.x+dx[d],y=pos.y+dy[d];
                    if(wall_at(x,y) || Position{uint8_t(x),uint8_t(y)}==trap.pos) continue;
                    unsigned c=y*MAP_W+x; if(without[c]) continue;
                    without[c]=true; queue[tail++]={uint8_t(x),uint8_t(y)};
                }
            }
            for(unsigned c=0;c<visited.size();++c) if(c!=unsigned(trap.pos.y*MAP_W+trap.pos.x))
                require(!visited[c] || without[c], "teleport removal disconnected reachable region");
        }
        for(const auto& f:game.features) if(feature_is_trap(f.id)) {
            auto t=feature_type(f.id);
            if(game.floor<2) require(t==FEATURE_SPIKE_TRAP || t==FEATURE_POISON_DART_TRAP || t==FEATURE_CONFUSION_TRAP,"opening trap roster");
            else if(game.floor<4) require(t<=FEATURE_CONFUSION_TRAP,"middle trap roster");
        }
        std::memset(game.features, 0, sizeof game.features);
        generation::populate_features(generation::floor_seed(generation::MAP_FEATURE_DOMAIN));
        require(!std::memcmp(game.features, first.features, sizeof game.features) &&
            !std::memcmp(game.monsters, first.monsters, sizeof game.monsters) &&
            !std::memcmp(game.ground, first.ground, sizeof game.ground) && game.random_state == rng, "feature domain changed ordinary content");
    }
    for(unsigned i = 1; i <= FEATURE_TYPES; ++i) require(roster[i] > 0, "feature never generated");
    for(unsigned count : archetypes) require(count > 0, "archetype coverage");
    unsigned low = 0, high = 0;
    for(uint16_t seed = 1; seed <= 1000; ++seed) {
        for(bool unholy : {false, true}) {
            arena(); game.run_seed = seed;
            for(uint8_t i = 0; i < HOSTILE_MONSTERS; ++i) game.monsters[i] = {{uint8_t(30 + i), 25}, uint8_t(unholy ? ZOMBIE : GOBLIN), 10, 0, {0, 0}, 0};
            generation::populate_features(generation::floor_seed(generation::MAP_FEATURE_DOMAIN));
            for(const MapFeature& f : game.features) if(f.id == FEATURE_HOLY_SYMBOL) { if(unholy) ++high; else ++low; }
        }
    }
    require(high > low * 2, "holy weighting ignored actual population");
    // A small open room has no DEAD_END/CORRIDOR geometry; fallback must work.
    arena(); std::memset(game.walls, 0xff, sizeof game.walls);
    for(uint8_t y = 9; y < 15; ++y) for(uint8_t x = 9; x < 18; ++x) carve(x, y);
    generation::populate_features(123); unsigned count = 0; for(const auto& f : game.features) count += bool(f.id);
    require(count >= 4, "geometry fallback rejected valid floor");
    std::printf("map features: generation roster and 4096 floors; holy low=%u high=%u\n", low, high);
}
}
void check_map_features() {
    static_assert(sizeof(MapFeature) == 3 && offsetof(Game, see_invisible) == 846 + 4*(GROUND_ITEMS-16) + 2*(INVENTORY-16) && offsetof(Game, features) == 808 + 4*(GROUND_ITEMS-16) + 2*(INVENTORY-16), "map feature saved layout");
    static_assert(valid_feature_icons(), "feature atlas");
    monster_traps(); monster_entry_checks(); teleport_topology(); trap_roster(); protections_and_death(); searching_and_movement(); summons_and_holy(); pickup_order(); interactive_roster(); outcome_checks(); generation_checks();
}
