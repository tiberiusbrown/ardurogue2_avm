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

using namespace rogue;
void require(bool, const char*);
namespace {
void arena(uint8_t type = FEATURE_NONE) {
    game = {}; session = {NONE, DEATH, false};
    game.player = {10, 10}; game.up = {2, 2}; game.down = {40, 20};
    game.hp = game.max_hp = 100; game.hunger = 220; game.level = 1;
    game.strength = 5; game.dexterity = 4; game.speed = 4;
    game.run_seed = game.random_state = 123; game.valid = 1;
    game.magic = SAVE_MAGIC; game.version = SAVE_VERSION;
    game.weapon_slot = game.armor_slot = game.amulet_slot = game.ring_slots[0] = game.ring_slots[1] = NONE;
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
    arena(FEATURE_SUMMONING_TRAP); activate_entry_feature(0);
    unsigned count = 0;
    for(uint8_t i = 0; i < MONSTERS; ++i) if(game.monsters[i].type) {
        ++count; require(game.monsters[i].pos != game.player && (game.monsters[i].state & MON_TRAP_SUMMONED), "trap monster initialization");
        defeat_monster(i);
    }
    require(count >= 1 && count <= 2 && !game.score && !game.xp, "summoning XP farming");
    for(unsigned n = 0; n < 20; ++n) activate_entry_feature(0);
    count = 0; for(const Monster& m : game.monsters) count += bool(m.type);
    require(count == MONSTERS, "summoning capacity");
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
        if(outcome<3) require(game.hp==45,"altar healing weights");
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
        game = {}; std::memcpy(&game, saved.data(), sizeof game);
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
void generation_checks() {
    unsigned roster[FEATURE_TYPES + 1] = {}; unsigned archetypes[4] = {};
    for(uint16_t seed = 1; seed <= 512; ++seed) {
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
        require(count >= 4 && count <= 8 && holy <= 1, "feature count/holy cap");
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
            for(uint8_t i = 0; i < MONSTERS; ++i) game.monsters[i] = {{uint8_t(30 + i), 25}, uint8_t(unholy ? ZOMBIE : GOBLIN), 10, 0, {0, 0}, 0};
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
    std::printf("map features: generation roster and 512 floors; holy low=%u high=%u\n", low, high);
}
}
void check_map_features() {
    static_assert(sizeof(MapFeature) == 3 && sizeof(Game) == 810 && offsetof(Game, features) == 773, "map feature saved layout");
    static_assert(valid_feature_icons(), "feature atlas");
    trap_roster(); protections_and_death(); searching_and_movement(); summons_and_holy(); interactive_roster(); outcome_checks(); generation_checks();
}
