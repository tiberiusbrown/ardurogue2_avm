#include "game.hpp"
#include "game_internal.hpp"
#include "features.hpp"
#include "persistence.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include <cstring>
#include <string>
using namespace rogue;
extern void require(bool, const char*);
extern void reset_feature_status();
extern const std::string& feature_status_text();
namespace {
void arena(uint16_t seed=71) {
    game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=seed;
    game.player={20,15}; game.up={2,2}; game.down={60,30};
    game.hp=game.max_hp=200; game.level=1; game.hunger=255; game.speed=1;
    game.weapon_slot=game.armor_slot=game.offhand_slot=game.amulet_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
    game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
    reset_feature_status();
}
void monster(uint8_t i,Position pos,uint8_t type=ORC) {
    game.monsters[i]={pos,type,monster_health(type),0,{0,0},0};
}
void wall(Position p) { auto n=uint16_t(p.y)*64+p.x; game.walls[n>>3]|=uint8_t(1u<<(n&7)); }
void drink(uint8_t type) {
    game.inventory[0]={type,2}; auto turns=game.turns;
    require(use_inventory(0) && game.turns==uint8_t(turns+1) && item_value(game.inventory[0])==1 && item_type_identified(type),"new potion turn/consumption/identification");
}
void scrolls() {
    bool durations[6]={};
    for(unsigned seed=1;seed<=256;++seed) {
        arena(uint16_t(seed)); monster(0,{23,18}); monster(1,{24,15});
        monster(2,{21,15},PHANTOM); monster(3,{22,15});
        set_monster_effect(game.monsters[3],MON_INVISIBLE,9);
        auto expected=game.random_state; uint8_t duration=3+next_random(expected)%3;
        apply_scroll_effect(SCROLL_HOLD_MONSTER);
        require(game.monsters[0].stun==duration && game.monsters[0].state&MON_AGGRO,"hold geometry/duration/aggression");
        require(!game.monsters[1].stun && !game.monsters[2].stun && !game.monsters[3].stun,"hold range/invisibility");
        durations[duration]=true;
        game.see_invisible=1; game.monsters[0].stun=9; apply_scroll_effect(SCROLL_HOLD_MONSTER);
        require(game.monsters[0].stun==9 && game.monsters[2].stun>=3 && game.monsters[3].stun>=3,"hold refresh/sight");
    }
    require(durations[3] && durations[4] && durations[5],"hold exact duration support");
    arena(); monster(0,{22,15}); wall({21,15}); apply_scroll_effect(SCROLL_HOLD_MONSTER);
    require(!game.monsters[0].stun && feature_status_text().find("Nothing happens")!=std::string::npos,"hold occlusion/zero targets");
    arena(); auto terrain=game;
    for(uint8_t i=0;i<10;++i) game.features[i]={{uint8_t(i*6),uint8_t(i*3)},uint8_t(2+2*i+MAP_FEATURE_HIDDEN)};
    game.features[10]={{62,31},FEATURE_FOUNTAIN}; game.features[11]={{63,31},FEATURE_SPIKE_TRAP};
    apply_scroll_effect(SCROLL_DETECT_TRAPS);
    for(uint8_t i=0;i<10;++i) require(game.features[i].id==2+2*i && explored(game.features[i].pos),"detect all traps and map positions");
    require(!explored({1,0}) && !explored({62,31}) && !explored({63,31}) && game.features[10].id==FEATURE_FOUNTAIN && !std::memcmp(game.walls,terrain.walls,sizeof game.walls) && game.hp==terrain.hp,"detect traps terrain/other features/already known");
    reset_feature_status(); apply_scroll_effect(SCROLL_DETECT_TRAPS);
    require(feature_status_text().find("No hidden traps")!=std::string::npos,"detect traps no-op message");
    arena(); game.floor=9;
    wall({20,14}); wall({19,15}); wall({20,16});
    require(summon_adjacent_monster() && game.monsters[0].pos==Position{21,15} && game.monsters[0].state&MON_SUMMONED && game.monsters[0].state&MON_AGGRO,"summon unique cardinal tile/flags");
    auto xp=game.xp; auto score=game.score; defeat_monster(0);
    require(game.xp==xp && game.score==score,"scroll summon reward exploit");
    wall({21,15}); require(!summon_adjacent_monster(),"summon blocked neighbors");
    arena(); for(uint8_t i=0;i<MONSTERS;++i) monster(i,{uint8_t(30+i),15});
    game.inventory[0]={SCROLL_SUMMONING,1}; require(use_inventory(0) && !game.inventory[0].type && game.turns==1,"full summons consume/turn");
    bool mimic=false;
    for(unsigned seed=1;seed<=128;++seed) {
        arena(uint16_t(seed)); game.floor=9; require(summon_adjacent_monster(),"open adjacency failed");
        if(game.monsters[0].type==MIMIC) { mimic=true; require(mimic_appearance(game.monsters[0])<MIMIC_APPEARANCE_COUNT,"summoned mimic appearance"); }
    }
    require(mimic,"summoning never chose floor mimic");
    for(Position edge: {Position{0,0},Position{63,31}}) { arena(); game.player=edge; require(summon_adjacent_monster(),"edge summon"); require(game.monsters[0].pos.x<64 && game.monsters[0].pos.y<32,"summon outside map"); }
    arena(); monster(0,{21,15}); monster(1,{19,15}); monster(2,{20,16},PHANTOM); monster(3,{22,15}); wall({21,15});
    Position old[4]; for(int i=0;i<4;++i) old[i]=game.monsters[i].pos;
    apply_scroll_effect(SCROLL_MASS_TELEPORT);
    require(game.monsters[0].pos!=old[0] && game.monsters[1].pos!=old[1] && game.monsters[0].pos!=game.monsters[1].pos,"mass teleports/collision");
    require(game.monsters[2].pos==old[2] && game.monsters[3].pos==old[3] && game.player==Position{20,15},"mass teleport hidden/occluded/player");
    arena(); std::memset(game.walls,255,sizeof game.walls); carve(20,15); carve(21,15); monster(0,{21,15});
    apply_scroll_effect(SCROLL_MASS_TELEPORT); require(game.monsters[0].pos==Position{21,15},"impossible teleport moved monster");
    arena(); game.see_invisible=20; monster(0,{21,15},PHANTOM); apply_scroll_effect(SCROLL_MASS_TELEPORT);
    require(game.monsters[0].pos!=Position{21,15} && monster_effect(game.monsters[0],MON_CONFUSED)==8,"mass teleport sight/shared side effects");
    arena(); monster(0,{21,16}); monster(1,{22,15}); monster(2,{19,15},DRAGON);
    auto hp=game.monsters[0].hp; apply_scroll_effect(SCROLL_FIRE);
    require(game.hp<200 && game.hp>=185 && (!game.monsters[0].type || game.monsters[0].hp<hp) && game.monsters[1].hp==monster_health(ORC) && game.monsters[2].hp==monster_health(DRAGON),"scroll fire splash/radius/immunity");
    arena(); game.fire_resistance=1; apply_scroll_effect(SCROLL_FIRE); require(game.hp==200,"scroll fire resistance");
    arena(); game.hp=1; apply_scroll_effect(SCROLL_FIRE); require(session.ended,"scroll fire can kill reader");
    arena(); game.ground[0]={{63,31},{POTION_FIRE,1}}; auto before=game;
    apply_scroll_effect(SCROLL_DETECT_OBJECTS);
    require(session.object_detection && !std::memcmp(&game,&before,sizeof game),"detect objects changed saved state");
    for(uint8_t type=SCROLL_HOLD_MONSTER;type<=SCROLL_LAST;++type) {
        arena(); game.inventory[0]={type,2}; game.fire_resistance=10;
        require(use_inventory(0) && game.turns==1 && item_value(game.inventory[0])==1 && item_type_identified(type),"scroll no-op use/stack/turn/identity");
    }
}
void potions() {
    for(unsigned seed=1;seed<=256;++seed) {
        arena(uint16_t(seed)); game.max_hp=100; game.hp=1; game.vamp_drain=20; game.confused=game.slowed=3;
        drink(POTION_EXTRA_HEALING); require(game.hp>=61 && game.hp<=80 && !game.confused && !game.slowed && game.max_hp==100,"extra healing effective maximum/statuses");
        arena(uint16_t(seed)); game.hp=1; game.max_hp=255;
        game.inventory[1]={AMULET_REGENERATION,uint8_t(ITEM_CURSED|1)}; game.amulet_slot=1;
        drink(POTION_EXTRA_HEALING); require(game.hp>=97 && game.hp<=129,"extra healing modifier/overflow");
        arena(uint16_t(seed)); drink(POTION_HASTE); require(game.haste>=14 && game.haste<=24,"haste bounds");
        arena(uint16_t(seed)); drink(POTION_LEVITATION); require(game.levitation>=14 && game.levitation<=24,"levitation bounds");
        arena(uint16_t(seed)); drink(POTION_FIRE_RESISTANCE); require(game.fire_resistance>=19 && game.fire_resistance<=29,"resistance bounds");
        arena(uint16_t(seed)); drink(POTION_SEE_INVISIBLE); require(game.see_invisible>=19 && game.see_invisible<=29,"sight bounds");
        arena(uint16_t(seed)); drink(POTION_FIRE); require(game.hp>=194 && game.hp<=197,"drunk fire bounds");
    }
    arena(); game.vamp_drain=99; game.weakened=3; game.hp=17; drink(POTION_RESTORE_VITALITY);
    require(!game.vamp_drain && !game.weakened && game.hp==17 && game.max_hp==200,"restore vitality permanent HP/current HP");
    arena(); monster(0,{21,15}); game.monsters[0].hp=1;
    set_monster_effect(game.monsters[0],MON_CONFUSED,8); set_monster_effect(game.monsters[0],MON_SLOWED,8);
    apply_monster_potion(POTION_EXTRA_HEALING,0);
    require(game.monsters[0].hp>monster_health(ORC)*3/4 && game.monsters[0].hp<=monster_health(ORC) && !monster_effect(game.monsters[0],MON_CONFUSED) && !monster_effect(game.monsters[0],MON_SLOWED),"thrown extra healing");
    set_monster_effect(game.monsters[0],MON_WEAKENED,8); auto hp=game.monsters[0].hp;
    apply_monster_potion(POTION_RESTORE_VITALITY,0); require(game.monsters[0].hp==hp && !monster_effect(game.monsters[0],MON_WEAKENED),"thrown vitality healed");
    set_monster_effect(game.monsters[0],MON_SLOWED,8); apply_monster_potion(POTION_HASTE,0); require(!monster_effect(game.monsters[0],MON_SLOWED),"thrown haste slowing");
    auto m=game.monsters[0]; for(auto type:{POTION_LEVITATION,POTION_FIRE_RESISTANCE,POTION_SEE_INVISIBLE}) apply_monster_potion(type,0);
    require(!std::memcmp(&m,&game.monsters[0],sizeof m),"no-effect throws changed monster");
    arena(); game.speed=4; game.haste=5; require(player_speed_cost()==3,"haste base speed");
    game.slowed=5; require(player_speed_cost()==7,"haste slow ordering");
    game.inventory[1]={AMULET_SPEED,3}; game.amulet_slot=1; require(player_speed_cost()==1,"haste amulet/clamp");
    game.inventory[1].info=ITEM_CURSED|2; require(player_speed_cost()==11,"haste cursed speed");
    game.inventory[1]={AMULET_PHOENIX_HEART,1}; require(player_speed_cost()==3,"haste artifact speed");
    arena(); game.haste=game.levitation=game.fire_resistance=game.see_invisible=200;
    for(auto type:{POTION_HASTE,POTION_LEVITATION,POTION_FIRE_RESISTANCE,POTION_SEE_INVISIBLE}) drink(type);
    require(game.haste==196 && game.levitation==196 && game.fire_resistance==196 && game.see_invisible==196,"timer refresh shortened duration");
    arena(); game.haste=game.levitation=game.fire_resistance=game.see_invisible=1;
    end_turn(); auto text=feature_status_text(); end_turn();
    require(!game.haste && !game.levitation && !game.fire_resistance && !game.see_invisible && text==feature_status_text(),"expiration repeated/missing");
    for(const char* phrase:{"no longer hasted","back to the floor","fire resistance fades","potion sight fades"}) require(text.find(phrase)!=std::string::npos,"expiration message absent");
    arena(); game.magic=SAVE_MAGIC; game.version=SAVE_VERSION; game.valid=1;
    game.haste=15; game.levitation=16; game.fire_resistance=20; game.see_invisible=21;
    Game saved=game; game={}; std::memcpy(&game,&saved,sizeof game);
    require(restore_startup_save(true) && game.haste==15 && game.levitation==16 && game.fire_resistance==20 && game.see_invisible==21,"new timers save roundtrip");
    make_floor(); require(game.haste==15 && game.levitation==16 && game.fire_resistance==20,"new timers floor persistence");
    game.version=SAVE_VERSION-1; require(!restore_startup_save(true) && !game.haste,"incompatible save accepted");
    arena(); game.fire_resistance=10; game.inventory[1]={RING_FIRE_IMMUNITY,uint8_t(ITEM_CURSED|1)}; game.ring_slots[0]=1;
    for(unsigned seed=1;seed<=128;++seed) { game.random_state=uint16_t(seed); player_take_fire_damage(255,255); }
    require(game.hp==200 && player_fire_effect()>0,"temporary fire vulnerability precedence");
    drink(POTION_FIRE); require(game.hp==200,"resistance drunk fire");
    game.features[0]={game.player,FEATURE_FLAME_TRAP|MAP_FEATURE_HIDDEN}; activate_entry_feature(0);
    require(game.hp==200,"resistance flame trap");
    game.inventory[0]={WAND_FIRE,2}; wall({21,15}); require(use_wand(0,1,0) && game.hp==200,"resistance wand explosion");
    bool breath=false;
    for(unsigned seed=1;seed<=128 && !breath;++seed) {
        arena(uint16_t(seed)); game.fire_resistance=10; monster(0,{23,15},DRAGON); end_turn();
        breath=feature_status_text().find("breathes fire")!=std::string::npos;
        if(breath) require(game.hp==200,"resistance dragon breath");
    }
    require(breath,"dragon fire fixture never breathed");
    arena(); game.hp=1; game.inventory[0]={POTION_FIRE,1}; require(use_inventory(0) && session.ended,"drunk fire not fatal");
    arena(); monster(0,{23,15}); monster(1,{23,16}); monster(2,{24,16},DRAGON); game.monsters[0].stun=game.monsters[1].stun=15;
    hp=game.monsters[0].hp; auto hp2=game.monsters[1].hp; game.inventory[0]={POTION_FIRE,2};
    require(throw_potion(0,1,0) && game.monsters[0].hp<hp && game.monsters[1].hp<hp2 && game.monsters[2].hp==monster_health(DRAGON) && game.hp==200 && game.turns==1 && potion_identified(POTION_FIRE),"fire throw area/identity/turn");
    arena(); wall({22,15}); monster(0,{21,16}); game.monsters[0].stun=15; game.inventory[0]={POTION_FIRE,1};
    hp=game.monsters[0].hp; require(throw_potion(0,1,0) && game.hp<200 && game.monsters[0].hp<hp,"missed fire throw explosion/splash");
    for(Position edge:{Position{0,0},Position{63,31}}) { arena(); game.player=edge; game.inventory[0]={POTION_FIRE,1}; require(throw_potion(0,edge.x ? 1 : -1,0) && game.hp>=192 && game.hp<=196 && potion_identified(POTION_FIRE),"immediate obstruction/edge fire"); }
    arena(); game.inventory[0]={POTION_FIRE,2}; auto rng=game.random_state;
    require(!throw_potion(0,0,0) && !throw_potion(0,1,1) && !game.turns && item_value(game.inventory[0])==2 && game.random_state==rng,"canceled/invalid throw consumed");
    for(uint8_t type=POTION_EXTRA_HEALING;type<=POTION_LAST;++type) {
        arena(); game.inventory[0]={type,1}; session.repeat_slot=0;
        require(use_inventory(0) && !game.inventory[0].type && session.repeat_slot==NONE,"potion deletion/repeat");
        arena(); game.inventory[0]={type,1}; session.repeat_slot=0;
        require(throw_potion(0,1,0) && !game.inventory[0].type && game.turns==1 && session.repeat_slot==NONE,"thrown no-op turn/repeat");
    }
    bool kept=false, doubled=false;
    for(unsigned seed=1;seed<=128;++seed) {
        for(bool cursed:{false,true}) {
            arena(uint16_t(seed)); game.inventory[1]={AMULET_CONSERVATION,uint8_t(1|(cursed ? ITEM_CURSED : 0))}; game.amulet_slot=1;
            game.inventory[0]={POTION_HASTE,3}; require(use_inventory(0) && potion_identified(POTION_HASTE),"new potion conservation use");
            auto left=item_value(game.inventory[0]); require(left>=1 && left<=3,"conservation stack corruption");
            kept|=left==3 && !cursed; doubled|=left==1 && cursed;
        }
    }
    require(kept && doubled,"new potion conservation branches missing");
}
void levitation() {
    for(uint8_t type=FEATURE_SPIKE_TRAP;type<=FEATURE_RUST_TRAP;type+=2) {
        for(bool forced:{false,true}) {
            arena(); game.levitation=5; game.features[0]={{21,15},uint8_t(type|MAP_FEATURE_HIDDEN)};
            game.inventory[1]={WAND_FIRE,10}; game.inventory[2]=make_equipment(PLATE_MAIL,1); game.armor_slot=2;
            auto before=game;
            if(forced) { wall({22,15}); force_player(1,0); } else move_player(1,0);
            if(forced) resolve_forced_entry();
            require(game.player==Position{21,15} && game.hp==200 && feature_is_hidden(game.features[0].id) && (forced ? game.paralyzed==4 : !game.paralyzed) && !game.confused && !game.slowed && !game.weakened && !std::memcmp(game.monsters,before.monsters,sizeof game.monsters) && !std::memcmp(game.inventory,before.inventory,sizeof game.inventory),"levitation trap/forced entry suppression");
        }
    }
    arena(); game.levitation=1; game.features[0]={game.player,FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN}; end_turn();
    require(game.hp==200 && feature_is_hidden(game.features[0].id),"levitation expiry retroactive trap");
    move_player(1,0); move_player(-1,0); require(game.hp<200 && !feature_is_hidden(game.features[0].id),"levitation subsequent entry normal trap");
    arena(); game.levitation=5; game.features[0]={{21,15},FEATURE_HEALING_SPRING}; game.hp=20;
    begin_player_move(1,0); complete_ground_pickups(); require(session.pending_feature==0,"levitation blocked benefit"); complete_player_move({true,NONE}); require(game.hp>20,"levitation spring");
    arena(); game.levitation=10; monster(0,{21,15}); game.features[0]={{21,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    auto hp=game.monsters[0].hp; activate_monster_trap(0); require(game.monsters[0].hp<hp,"levitation changed monster traps");
    arena(); game.levitation=5; std::memset(game.walls,255,sizeof game.walls); carve(20,15); carve(21,15);
    game.features[0]={{21,15},FEATURE_SUMMONING_TRAP|MAP_FEATURE_HIDDEN}; require(teleport_player() && game.player==Position{21,15},"levitation teleport landing"); resolve_forced_entry(); require(feature_is_hidden(game.features[0].id) && !game.monsters[0].type,"levitation teleport trap");
}
}
void check_new_consumables() {
    scrolls(); potions(); levitation();
    for(unsigned seed=1;seed<=256;++seed) {
        arena(uint16_t(seed));
        uint8_t count=0;
        for(auto group:{POTION_FIRST,SCROLL_FIRST,RING_FIRST,AMULET_FIRST,WAND_FIRST}) {
            bool seen[18]={}; auto size=is_potion(group)?POTION_COUNT:is_scroll(group)?SCROLL_COUNT:is_ring(group)?RING_COUNT:is_amulet(group)?AMULET_COUNT:WAND_COUNT;
            for(uint8_t i=0;i<size;++i) { auto t=uint8_t(group+i); auto a=item_appearance(t); require(a<size && !seen[a],"appearance collision/overrun"); seen[a]=true; identify_type(t); require(item_type_identified(t),"expanded identification"); ++count; }
        }
        require(count==62 && !(game.identified_items[7]&0xc0),"62-type identification bounds");
    }
}
