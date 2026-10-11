#include "explorer_observation.hpp"
#include "simulator.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "status.hpp"
#include "policy_hash.hpp"
#include "explorer_events.hpp"
#include "tuning.hpp"
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
using namespace rogue;
using namespace sim;
namespace {
unsigned checks=0;
void check(bool c,const char* message) { ++checks; if(!c) throw std::runtime_error(message); }
int cell(int x,int y) { return y*64+x; }
void arena() {
    game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=123;
    game.hp=game.max_hp=100; game.level=1; game.strength=5; game.dexterity=4; game.speed=4; game.hunger=220;
    game.player={10,10}; game.up={10,10}; game.down={50,20};
    game.weapon_slot=game.armor_slot=game.offhand_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
    std::memset(game.walls,255,sizeof game.walls);
    for(int y=8;y<=12;++y) for(int x=8;x<=12;++x) carve(uint8_t(x),uint8_t(y));
}
void item_text(std::ostream& s,const SeenItem& i) {
    s<<int(i.category)<<':'<<int(i.type)<<':'<<int(i.appearance)<<':'<<int(i.quantity)<<':'<<int(i.magnitude)<<':'<<int(i.modifier)<<':'<<int(i.enchant)<<':'<<i.individual<<':'<<i.cursed<<';';
}
std::string observation_text(const ExplorerObservation& o) {
    std::ostringstream s;
    s<<int(o.player.x)<<':'<<int(o.player.y)<<':'<<int(o.floor)<<':'<<int(o.hp)<<':'<<int(o.max_hp)<<':'<<int(o.level)<<':'<<int(o.xp)<<':'<<int(o.strength)<<':'<<int(o.dexterity)<<':'<<int(o.speed)<<':'<<int(o.hunger)<<':'<<int(o.radius)<<':'<<o.yendor<<':'<<o.pending_pickups<<':'<<o.actions<<':'<<o.turns<<':'<<o.attacked<<':'<<o.incoming_attack<<':'<<int(o.hp_loss);
    s<<':'<<int(o.reported_attacker)<<':'<<int(o.reported_species.strength)<<':'<<o.reported_species.traits;
    for(auto b:o.equipped) s<<int(b)<<',';
    for(auto b:{o.weakened,o.confused,o.paralyzed,o.slowed,o.invisible,o.blindness}) s<<int(b)<<',';
    for(auto& i:o.inventory) item_text(s,i);
    for(int n=0;n<2048;++n) s<<int(o.terrain[n])<<':'<<int(o.features[n])<<':'<<o.visible[n]<<',';
    for(int g=0;g<o.ground_count;++g) { auto& a=o.ground[g]; s<<int(a.pos.x)<<':'<<int(a.pos.y)<<':'<<int(a.handle); item_text(s,a.item); }
    for(int m=0;m<o.monster_count;++m) { auto& a=o.monsters[m]; s<<int(a.pos.x)<<':'<<int(a.pos.y)<<':'<<int(a.type)<<':'<<int(a.strength)<<':'<<int(a.speed)<<':'<<int(a.armor)<<':'<<a.traits; }
    for(auto& group:o.identities) for(auto t:group) s<<int(t)<<',';
    return s.str();
}
std::string action_text(const ExplorerAction& a) {
    std::ostringstream s; s<<int(a.kind)<<':'<<int(a.dx)<<':'<<int(a.dy)<<':'<<int(a.slot)<<':'<<int(a.target)<<':'<<a.discard<<':'<<a.no_path<<':'<<a.goal<<':'<<int(a.destination.x)<<':'<<int(a.destination.y); return s.str();
}
void identical(const Game& a,const Game& b,const char* message) {
    game=a; Game before=game; auto oa=observe_explorer(0,0);
    check(std::memcmp(&before,&game,sizeof game)==0,"observer mutated Game/RNG");
    game=b; before=game; auto ob=observe_explorer(0,0);
    check(std::memcmp(&before,&game,sizeof game)==0,"observer mutated second Game/RNG");
    check(observation_text(oa)==observation_text(ob),message);
    auto pa=std::make_unique<ExplorerAgent>(),pb=std::make_unique<ExplorerAgent>(); pa->reset(); pb->reset();
    check(action_text(pa->choose_action(oa))==action_text(pb->choose_action(ob)),"identical observation yielded different action");
    auto fa=pa->choose_feature(oa),fb=pb->choose_feature(ob);
    check(fa.accept==fb.accept && fa.slot==fb.slot,"feature choice leaked hidden state");
    check(std::memcmp(&before,&game,sizeof game)==0,"policy mutated production state");
}
void noninterference() {
    arena(); Game a=game,b=a;
    b.inventory[0].info=uint8_t(ITEM_IDENTIFIED|ITEM_CURSED|7);
    identical(a,b,"empty inventory slot leaked stale item information");
    a.monsters[0]={{11,10},ORC,9,0,{0,0},MON_AGGRO};
    a.ground[0]={{11,10},{FOOD,1}}; b=a;
    b.ground[0].item=make_equipment(LONG_SWORD,5);
    identical(a,b,"monster-covered item sprite leaked its category");
    arena(); a=game; b=a;
    a.confused=a.paralyzed=a.slowed=a.invisible=a.blindness=2; b=a;
    b.confused=b.paralyzed=b.slowed=b.invisible=b.blindness=7;
    identical(a,b,"hidden player status durations leaked");
    arena(); a=game; b=a;
    game=b; carve(40,20); b=game; identical(a,b,"hidden corridor leaked");
    b=a; b.down={45,25}; identical(a,b,"hidden stairs leaked");
    a.ground[0]={{40,20},{POTION_HEALING,1}}; b=a; b.ground[0]={{35,20},make_equipment(LONG_SWORD,5)};
    identical(a,b,"hidden loot leaked");
    a.monsters[0]={{40,20},DRAGON,80,0,{0,0},MON_AGGRO}; b=a; b.monsters[0]={{35,20},GOBLIN,5,0,{0,0},0};
    identical(a,b,"hidden monster leaked");
    a.features[0]={{11,10},uint8_t(FEATURE_TELEPORT_TRAP|MAP_FEATURE_HIDDEN)}; b=a;
    b.features[0]={{10,11},uint8_t(FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN)};
    identical(a,b,"hidden trap leaked");
    b=a; b.random_state=555; identical(a,b,"hidden RNG leaked");
    arena(); a=game; a.inventory[0]={POTION_HEALING,1}; game=a;
    uint8_t appearance=item_appearance(POTION_HEALING); b=a; b.inventory[0]={POTION_POISON,1};
    bool found=false;
    for(int seed=1;seed<65536;++seed) { game=b; game.run_seed=uint16_t(seed); if(item_appearance(POTION_POISON)==appearance) { b=game; found=true; break; } }
    check(found,"could not construct equal appearances"); identical(a,b,"unidentified potion type or run seed leaked");
    arena(); a=game; a.inventory[0]=make_equipment(LONG_SWORD,-3); b=a;
    b.inventory[0]=make_equipment(LONG_SWORD,5); b.inventory[0].info|=ITEM_CURSED;
    identical(a,b,"unknown enchantment/curse leaked");
    arena(); a=game; a.monsters[0]={{11,10},ORC,9,0,{0,0},MON_AGGRO}; b=a;
    b.monsters[0].hp=1; b.monsters[0].stun=4; b.monsters[0].effects[0]=2; b.monsters[0].state|=MON_AFRAID;
    identical(a,b,"monster live HP/status leaked");
    arena(); a=game; a.monsters[0]={{11,10},MIMIC,20,0,{0,0},0}; set_mimic_appearance(a.monsters[0],MIMIC_POTION);
    b=game; b.ground[0]={{11,10},{POTION_HEALING,1}};
    identical(a,b,"unrevealed mimic differs from item category");
    arena(); game.features[0]={game.player,FEATURE_STATUE}; a=game; b=a;
    b.monsters[0]={{11,10},PHANTOM,20,0,{0,0},MON_AGGRO};
    identical(a,b,"invisible nearby enemy leaked into feature choice");
}
void perception_and_memory() {
    arena(); auto p=std::make_unique<ExplorerAgent>(); p->reset();
    ExplorerObserver observer;
    game.monsters[0]={{11,10},PHANTOM,10,0,{0,0},0};
    check(observer.observe(0,0).monster_count==0,"natural invisibility ignored");
    game.inventory[0]={RING_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[0]=0; identify_type(RING_SEE_INVISIBLE);
    check(observer.observe(0,0).monster_count==1,"see invisible did not expose phantom");
    game.blindness=5; auto blind=observer.observe(0,0);
    check(blind.visible.count()==1 && blind.monster_count==0,"blind radius zero reveals neighbors/monster");
    game.ground[0]={game.player,{POTION_HEALING,1}};
    check(observer.observe(0,0).ground_count==1,"blind occupied-cell pickup unavailable");
    game.blindness=0; game.monsters[0]={}; auto seen=observer.observe(0,0); p->choose_action(seen);
    auto old=p->memory(0).tiles[cell(11,10)].terrain;
    game.blindness=3; p->choose_action(observer.observe(1,1));
    check(p->memory(0).tiles[cell(11,10)].terrain==old,"blindness erased memory");
    game.blindness=0; game.player={11,10}; p->choose_action(observer.observe(2,2));
    check(p->memory(0).tiles[cell(10,10)].terrain==Terrain::Up,"leaving cell erased terrain");
    game.ground[1]={{12,10},{POTION_POISON,1}}; p->choose_action(observer.observe(3,3));
    check(p->memory(0).tiles[cell(12,10)].has_item,"sprite not remembered");
    game.monsters[1]={{12,10},ORC,8,0,{0,0},MON_AGGRO}; p->choose_action(observer.observe(4,4));
    check(p->memory(0).tiles[cell(12,10)].has_item && !p->stats.missing_items,"covering monster erased legitimately remembered loot");
    game.monsters[1]={};
    game.ground[1]={}; p->choose_action(observer.observe(4,4));
    check(!p->memory(0).tiles[cell(12,10)].has_item && p->stats.missing_items==1,"missing item not invalidated");
    game.monsters[0]={{12,10},ORC,8,0,{0,0},MON_AGGRO}; p->choose_action(observer.observe(5,5));
    auto history=observer.observe(6,6); auto q=std::make_unique<ExplorerAgent>(); q->reset(); q->choose_action(seen); q->choose_action(blind);
    game.monsters[0].pos={40,20}; auto a=observer.observe(7,7); game.monsters[0].pos={35,25}; auto b=observe_explorer(7,7);
    check(observation_text(a)==observation_text(b),"offscreen movement leaked after sighting");
    auto p2=std::make_unique<ExplorerAgent>(); p2->reset(); p2->choose_action(seen); p2->choose_action(history);
    auto p3=std::make_unique<ExplorerAgent>(); p3->reset(); p3->choose_action(seen); p3->choose_action(history);
    check(action_text(p2->choose_action(a))==action_text(p3->choose_action(b)),"last-seen memory tracked live positions");
    p->choose_action(a); p->choose_action(observer.observe(20,20));
    bool alive=false; for(auto& e:p->memory(0).enemies) alive|=e.type!=0;
    check(!alive,"monster memory did not expire");
    arena(); game.monsters[0]={{11,10},MIMIC,10,0,{0,0},0};
    check(observe_explorer(0,0).monster_count==0,"mimic was identified before revealing");
    game.monsters[0].state|=MON_AGGRO; check(observe_explorer(0,0).monster_count==1,"revealed mimic not observable");
    arena(); game.player={0,0}; carve(0,0); game.blindness=2;
    check(observe_explorer(0,0).visible.count()==1,"radius-zero map boundary leak");
}
void identification_and_mapping() {
    arena(); game.inventory[0]={POTION_HEALING,1}; game.inventory[1]={RING_PROTECTION,2}; game.inventory[2]={WAND_FIRE,5};
    auto o=observe_explorer(0,0); check(!o.inventory[0].type && !o.inventory[1].type && !o.inventory[2].type,"unidentified items exposed");
    auto appearance=o.inventory[0].appearance; use_inventory(0);
    o=observe_explorer(1,1); check(o.identities[0][appearance]==POTION_HEALING,"consumed potion knowledge lost");
    use_inventory(1); o=observe_explorer(2,2);
    check(o.inventory[1].type==RING_PROTECTION && o.inventory[1].individual,"equipment interaction did not identify");
    check(o.inventory[1].magnitude==0,"unprinted jewelry magnitude exposed");
    game.inventory[3]={SCROLL_IDENTIFY,1}; use_inventory(3,2); o=observe_explorer(3,3);
    check(o.inventory[2].type==WAND_FIRE && o.inventory[2].quantity==5 && o.inventory[2].individual,"identify scroll did not disclose wand");
    arena(); ExplorerObserver observer; auto p=std::make_unique<ExplorerAgent>(); p->reset();
    p->choose_action(observer.observe(0,0)); carve(50,20); game.down={50,20};
    game.ground[0]={{50,20},{POTION_HEALING,1}}; game.monsters[0]={{50,20},DRAGON,80,0,{0,0},0};
    game.inventory[0]={SCROLL_MAPPING,1}; use_inventory(0); o=observer.observe(1,1);
    check(o.terrain[cell(50,20)]==Terrain::Down && !o.ground_count && !o.monster_count,"mapping revealed entities or missed stairs");
    p->choose_action(o); check(p->memory(0).down==SeenPosition{50,20},"mapping knowledge not retained");
    // A remote door opened AFTER mapping is not fresh knowledge.
    game.door_count=1; game.doors[0].pos={50,20}; open_door(0);
    check(observer.observe(2,2).terrain[cell(50,20)]==Terrain::Unknown,"mapping became remote live subscription");
    arena(); game.floor=15; game.monsters[0]={{12,12},LORD,80,0,{0,0},MON_AGGRO};
    game.inventory[0]={SCROLL_MAPPING,1}; use_inventory(0); p->reset(); o=observe_explorer(0,0);
    check(!o.monster_count && o.terrain[cell(12,12)]==Terrain::Floor,"mapped arena leaked unseen Lord");
    auto action=p->choose_action(o);
    check(action.kind==ExplorerActionKind::Move && !action.no_path,"mapping exhausted final-floor search for unseen entities");
    check(!p->memory(15).tiles[cell(12,12)].surveyed,"mapping claimed remote entities had been surveyed");
    Collector metrics; metrics.data.agent="explorer-v1"; metrics.enter_floor(); metrics.observe_exploration(o);
    check(metrics.data.floors.back().exploration.floor_cells>metrics.data.floors.back().exploration.surveyed_floor_cells,"telemetry equated mapped terrain with surveyed entities");
    game.ground[0]={game.player,{FOOD,1}}; metrics.observe_exploration(observe_explorer(1,1));
    auto observed_count=metrics.data.floors.back().exploration.items_observed;
    game.ground[0]={game.player,{YENDOR_AMULET,1}};
    metrics.handle(EventKind::GeneratedItem,0,YENDOR_AMULET,1,0);
    metrics.observe_exploration(observe_explorer(2,2));
    check(metrics.data.floors.back().exploration.items_observed==observed_count+1,"reused ground slot concealed a newly generated observed item from telemetry");
    arena(); p->reset(); game.inventory[0]={AMULET_VAMPIRE,uint8_t(ITEM_IDENTIFIED|1)}; game.amulet_slot=0; identify_type(AMULET_VAMPIRE);
    game.inventory[1]={AMULET_SPEED,1};
    int quiet=ExplorerParameters{}.jewelry_quiet_turns+1;
    for(int turn=0;turn<quiet;++turn) { o=observe_explorer(turn,turn); p->choose_action(o); }
    auto experiment=p->choose_action(observe_explorer(quiet,quiet));
    check(experiment.kind==ExplorerActionKind::Use && experiment.slot==1,"known mediocre amulet permanently blocked unknown jewelry identification");
    arena(); ExplorerParameters prefs; prefs.see_invisible_score=200; auto rings=std::make_unique<ExplorerAgent>(prefs);
    game.inventory[0]={RING_PROTECTION,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[0]=0; identify_type(RING_PROTECTION);
    game.inventory[1]={RING_STRENGTH,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[1]=1; identify_type(RING_STRENGTH);
    game.inventory[2]={RING_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(RING_SEE_INVISIBLE);
    auto remove=rings->choose_action(observe_explorer(0,0));
    check(remove.kind==ExplorerActionKind::Use && remove.slot==1 && use_inventory(remove.slot,remove.target),"weaker ring removal failed");
    auto wear=rings->choose_action(observe_explorer(1,1));
    check(wear.kind==ExplorerActionKind::Use && wear.slot==2 && use_inventory(wear.slot,wear.target),"removed ring was re-equipped instead of its replacement");
    check(game.ring_slots[0]==0 && game.ring_slots[1]==2 && rings->choose_action(observe_explorer(2,2)).kind!=ExplorerActionKind::Use,"ring replacement did not remain stable");
    arena(); prefs=ExplorerParameters{}; prefs.protection_ring_score=137; prefs.illumination_ring_score=100; prefs.illumination_dark_bonus=80;
    rings=std::make_unique<ExplorerAgent>(prefs);
    game.inventory[0]={RING_PROTECTION,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[0]=0; identify_type(RING_PROTECTION);
    game.inventory[1]={RING_DEXTERITY,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[1]=1; identify_type(RING_DEXTERITY);
    game.inventory[2]={RING_ILLUMINATION,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(RING_ILLUMINATION);
    remove=rings->choose_action(observe_explorer(0,0)); check(remove.slot==1 && use_inventory(remove.slot,remove.target),"permanent sight did not replace weaker ring");
    wear=rings->choose_action(observe_explorer(1,1)); check(wear.slot==2 && use_inventory(wear.slot,wear.target),"planned illumination ring was not worn");
    check(player_light_radius()==3 && rings->choose_action(observe_explorer(2,2)).kind!=ExplorerActionKind::Use,"illumination value fell after equip and caused an equipment loop");
    arena(); prefs.brilliance_score=243; prefs.brilliance_dark_bonus=200; prefs.speed_amulet_score=347;
    rings=std::make_unique<ExplorerAgent>(prefs);
    game.inventory[0]={AMULET_SPEED,uint8_t(ITEM_IDENTIFIED|1)}; game.amulet_slot=0; identify_type(AMULET_SPEED);
    game.inventory[1]={AMULET_BRILLIANCE,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(AMULET_BRILLIANCE);
    wear=rings->choose_action(observe_explorer(0,0)); check(wear.slot==1 && use_inventory(wear.slot,wear.target),"darkness value did not favor permanent amulet sight");
    check(player_light_radius()==4 && rings->choose_action(observe_explorer(1,1)).kind!=ExplorerActionKind::Use,"brilliance value fell after equip and caused an equipment loop");
}
void navigation_and_tactics() {
    arena(); auto p=std::make_unique<ExplorerAgent>(); p->reset();
    auto o=observe_explorer(0,0); auto a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && o.terrain[cell(10+a.dx,10+a.dy)]!=Terrain::Unknown,"path used unknown terrain");
    check(p->memory(0).down.x==255,"unknown down stair destination memorized");
    game.hp=40; game.down={11,10}; o=observe_explorer(1,1); a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && a.dx==1,"observed stairs not approached");
    game.player=game.down; a=p->choose_action(observe_explorer(2,2)); check(a.kind==ExplorerActionKind::Stairs,"known stairs not used");
    // Retain same-generation floors; ascent generation preserves only history.
    game.floor=1; p->choose_action(observe_explorer(3,3)); game.floor=0; p->choose_action(observe_explorer(4,4));
    check(p->memory(0).tiles[cell(10,10)].terrain==Terrain::Up,"same-layout revisit erased learned terrain");
    game.floor=1; p->choose_action(observe_explorer(5,5)); game.floor=0; game.has_amulet=1; game.player={40,20}; carve(40,20); game.up={39,20}; carve(39,20);
    p->choose_action(observe_explorer(6,6));
    check(p->memory(0).tiles[cell(10,10)].historical==Terrain::Up && p->memory(0).tiles[cell(10,10)].terrain==Terrain::Unknown,"ascent geometry did not distinguish history");
    a=p->choose_action(observe_explorer(7,7)); check(a.kind==ExplorerActionKind::Move && a.dx==-1,"ascent did not approach newly discovered up stair");
    game.player=game.up; check(p->choose_action(observe_explorer(8,8)).kind==ExplorerActionKind::Stairs,"surface return not selected");
    arena(); p->reset(); game.has_amulet=1; game.hunger=0; game.ground[0]={game.player,{FOOD,1}};
    a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Take,"ascent ignored food on occupied square");
    arena(); p->reset(); game.has_amulet=1; game.hunger=0; game.up={9,10}; game.ground[0]={{11,10},{FOOD,1}};
    a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Move && a.dx==1,"starving ascent refused short observed food detour");
    // Isolate stack matching from the tunable decision to eat carried food first.
    ExplorerParameters food_preferences; food_preferences.food_threshold=45;
    p=std::make_unique<ExplorerAgent>(food_preferences);
    arena(); game.has_amulet=1; game.hunger=100; game.up={9,10}; game.ground[0]={{11,10},{FOOD,2}};
    for(auto& i:game.inventory) i={POTION_POISON,63}; identify_type(POTION_POISON); game.inventory[15]={FOOD,1};
    a=p->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Move && a.dx==1,"full pack prevented generic food from matching a carried stack on ascent");
    game.player={11,10}; a=p->choose_action(observe_explorer(1,1));
    check(a.kind==ExplorerActionKind::Take && take_item(a.target)==PICKUP_TAKEN && item_value(game.inventory[15])==3,"observed ascent food did not stack through production pickup");
    p=std::make_unique<ExplorerAgent>();
    arena(); game.hunger=55; game.ground[0]={game.player,{FOOD,1}};
    for(auto& i:game.inventory) i={POTION_POISON,63}; identify_type(POTION_POISON);
    game.inventory[15]={WAND_FORCE,uint8_t(ITEM_IDENTIFIED|15)}; identify_type(WAND_FORCE);
    a=p->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Take && a.slot==15 && swap_ground_item(a.target,a.slot),"hungry food replacement did not execute");
    a=p->choose_action(observe_explorer(1,1));
    check(a.kind!=ExplorerActionKind::Take,"pickup/retention food values caused an immediate reverse swap");
    arena(); p->reset(); game.hp=10; game.inventory[0]={POTION_HEALING,2}; identify_type(POTION_HEALING);
    check(p->choose_action(observe_explorer(0,0)).kind==ExplorerActionKind::Use,"identified emergency healing ignored");
    arena(); p->reset(); game.inventory[0]={POTION_POISON,1}; game.monsters[0]={{11,10},ORC,10,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Use,"unknown potion experimented in combat");
    game.monsters[0]={}; identify_type(POTION_POISON);
    for(int i=1;i<30;++i) { a=p->choose_action(observe_explorer(i,i)); check(a.kind!=ExplorerActionKind::Use,"known poison drunk for experimentation"); }
    arena(); p->reset(); game.hp=10; game.inventory[0]={WAND_STRIKING,uint8_t(ITEM_IDENTIFIED|4)}; identify_type(WAND_STRIKING);
    game.monsters[0]={{11,10},ORC,10,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Wand && a.dx==1,"visible ranged tactic missing");
    game.monsters[0].pos={40,20}; p->reset(); a=p->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Wand,"unseen monster targeted");
    arena(); p->reset(); game.inventory[0]={WAND_STRIKING,4}; identify_type(WAND_STRIKING);
    carve(13,10); game.inventory[1]={GLOWING_ORB,0}; game.offhand_slot=1;
    game.monsters[0]={{13,10},ORC,10,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(10,10));
    check(a.kind==ExplorerActionKind::Wand && a.slot==0,"known wand type with unknown individual properties never tested");
    check(use_wand(a.slot,a.dx,a.dy) && item_is_identified(game.inventory[0]),"wand experiment did not reveal individual properties through production action");
    arena(); p->reset(); game.inventory[0]={WAND_STRIKING,4}; identify_type(WAND_STRIKING);
    game.monsters[0]={{11,10},ORC,10,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(10,10));
    check(a.kind==ExplorerActionKind::Wand && a.slot==0,"unexamined known offensive wand unusable at equipment sight radius one");
    arena(); p->reset(); game.inventory[0]=make_equipment(SHORT_BOW,0); game.weapon_slot=0;
    game.inventory[1]=make_equipment(TWO_HANDED_SWORD,0); game.inventory[2]={ARROWS,20};
    game.monsters[0]={{11,10},LORD,100,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(10,10));
    check(a.kind==ExplorerActionKind::Use && a.slot==1,"bow remained equipped for adjacent melee contact");
    arena(); p->reset(); game.inventory[0]=make_equipment(LONG_SWORD,0); game.monsters[0]={{11,10},SNAKE,5,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"carried weapon not equipped before adjacent melee");
    arena(); p->reset(); game.inventory[0]={SCROLL_TORMENT,2}; identify_type(SCROLL_TORMENT);
    game.monsters[0]={{11,10},TROLL,25,0,{0,0},MON_AGGRO};
    a=p->choose_action(observe_explorer(10,10)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"known mass scroll ignored against visible late threat");
    a=p->choose_action(observe_explorer(11,11)); check(a.kind!=ExplorerActionKind::Use,"control inference allowed immediate mass-scroll spam");
    arena(); p->reset(); game.inventory[0]={TORCH,1}; game.offhand_slot=0; burn_offhand_fuel(); game.inventory[0].info=0;
    game.inventory[1]={LANTERN,100}; a=p->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Use && a.slot==1,"exhausted light not replaced");
    arena(); p->reset(); game.features[0]={{11,10},FEATURE_TELEPORT_TRAP}; game.down={12,10};
    game.explored[0]=0; std::memset(game.explored,255,sizeof game.explored); a=p->choose_action(observe_explorer(0,0));
    check(!(a.dx==1 && a.dy==0),"known teleport trap preferred to safe route");
    arena(); p->reset(); game.features[0]={{11,10},FEATURE_SUMMONING_TRAP}; game.down={12,10};
    a=p->choose_action(observe_explorer(0,0)); check(!(a.dx==1 && a.dy==0),"known summoning trap preferred to safe route");
    arena(); p->reset(); for(auto& i:game.inventory) i={POTION_POISON,1}; identify_type(POTION_POISON);
    game.inventory[0]=make_equipment(LONG_SWORD,0); game.weapon_slot=0;
    game.ground[0]={game.player,make_equipment(TITAN_PLATE,0)}; session.pending_pickups=true;
    a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Take && a.slot!=0 && a.slot<16,"full inventory replacement missing/protected weapon discarded");
}
void stationary_contact_detour() {
    arena(); auto o=observe_explorer(0,0); Game before=game;
    ExplorerParameters prefs; prefs.stationary_contact_turns=32; prefs.invisible_response=0;
    auto p=std::make_unique<ExplorerAgent>(prefs);
    o.contact_reported=true; o.contact={11,10}; o.contact_type=PHANTOM;
    for(unsigned turn=0;turn<32;++turn) {
        o.actions=o.turns=turn; auto a=p->choose_action(o);
        check(a.kind==ExplorerActionKind::Move && a.dx==1 && a.dy==0,"fresh melee contact abandoned before its configured retry budget");
    }
    o.actions=o.turns=32; auto a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && !(a.dx==1 && a.dy==0),"stationary contact did not take an observed alternate route");
    check(o.terrain[cell(10+a.dx,10+a.dy)]!=Terrain::Unknown,"contact detour invented an unknown escape route");
    check(std::memcmp(&before,&game,sizeof game)==0,"contact detour mutated production state");
}
void controlled_roundtrip() {
    arena(); game.floor=15; game.down={50,20}; game.up=game.player;
    game.level=40; game.hp=game.max_hp=200; game.strength=game.dexterity=12; game.speed=1;
    game.inventory[0]=make_equipment(HAMMER_OF_RUIN,5); game.weapon_slot=0;
    game.inventory[1]=make_equipment(DRAGONHIDE,5); game.armor_slot=1;
    game.inventory[2]={GLOWING_ORB,0}; game.offhand_slot=2;
    game.inventory[3]={AMULET_BRILLIANCE,uint8_t(ITEM_IDENTIFIED|1)}; game.amulet_slot=3;
    game.inventory[4]={RING_INVISIBILITY,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[0]=4;
    game.inventory[5]={RING_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[1]=5;
    game.inventory[6]={FOOD,63};
    for(int i=0;i<=5;++i) identify_item(uint8_t(i));
    game.monsters[11]={{11,10},LORD,1,0,{0,0},MON_AGGRO};
    auto p=std::make_unique<ExplorerAgent>(); auto r=run_started(24,*p);
    check(r.result=="escaped" && r.has_yendor && r.floors_exited==16,"controlled Lord/Yendor/full ascent did not escape");
    check(r.monsters[LORD].killed==1 && r.items[YENDOR_AMULET].picked_up==1,"controlled roundtrip bypassed production Lord/Yendor");
}
void replay() {
    auto p=std::make_unique<ExplorerAgent>(); Options o; o.agent=p->name();
    std::ostringstream t1,t2; o.trace=&t1; auto a=run(42,*p,o); auto final=game;
    o.trace=&t2; auto b=run(42,*p,o);
    check(a.action_hash==b.action_hash && a.result==b.result && t1.str()==t2.str() && !std::memcmp(&final,&game,sizeof game),"explorer replay changed");
    o.trace=nullptr; o.telemetry=false; auto c=run(42,*p,o);
    check(a.action_hash==c.action_hash && !std::memcmp(&final,&game,sizeof game),"telemetry changed explorer gameplay");
    for(uint16_t seed=1;seed<=32;++seed) { auto r=run(seed,*p); check(!r.stuck,"constructed competence seeds stuck"); }
}
void parameters_and_messages() {
    check(fitness_better({1,0,0,10},{0,0,0,1000000}),"escapes do not outrank total score");
    check(fitness_better({1,0,0,20},{1,0,0,10}),"score does not break escape ties");
    check(fitness_better({0,0,0,10},{100,1,0,1000000}),"simulator failure can win tuning");
    TuningFitness deep,grinder; deep.progression=progression_credit(14,false,false,14); grinder.score=100000000; grinder.progression=progression_credit(0,false,false,0);
    check(fitness_better(deep,grinder),"weak-monster farming outranks progression");
    deep.lord=1; grinder.escaped=1; check(fitness_better(grinder,deep),"progression outranks escape");
    check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA256");
    check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc");
    check(sha256(std::string(1000,'a'))=="41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3","multi-block SHA256");
    auto defaults=ExplorerParameters{}; auto p=parse_parameters(parameter_json(defaults));
    check(parameter_json(p)==parameter_json(defaults),"parameter roundtrip");
    check(parameter_hash(parse_parameters("{}"))==parameter_hash(defaults),"partial defaults change policy identity");
    for(auto text:{"[]","{\"missing\":1}","{\"healing_percent\":99}","{\"healing_percent\":50.1}","{\"healing_percent\":50,\"healing_percent\":60}","{}extra","{\"healing_percent\":050}"}) {
        bool rejected=false; try { parse_parameters(text); } catch(const std::exception&) { rejected=true; }
        check(rejected,"malformed parameter file accepted");
    }
    p.healing_percent=80;
    check(explorer_policy_hash("code",p)!=explorer_policy_hash("code",defaults),"different preferences share a policy hash");
    arena(); game.hp=60; game.inventory[0]={POTION_HEALING,1}; identify_type(POTION_HEALING);
    auto a=std::make_unique<ExplorerAgent>(defaults),b=std::make_unique<ExplorerAgent>(p);
    auto obs=observe_explorer(0,0);
    check(a->choose_action(obs).kind!=ExplorerActionKind::Use && b->choose_action(obs).kind==ExplorerActionKind::Use,"runtime healing threshold has no effect");
    b->reset(); check(b->parameters.healing_percent==80,"reset lost runtime preferences");
    ExplorerMessages messages; ExplorerMessageScope scope(&messages);
    ExplorerAction attempted; attempted.kind=ExplorerActionKind::Move; attempted.dx=1;
    messages.prepare(attempted,{10,10},false);
    messages.text("You miss"); messages.monster(PHANTOM);
    obs=observe_explorer(1,1);
    check(obs.contact_reported && obs.contact==SeenPosition{11,10},"displayed melee contact not retained");
    messages.prepare(attempted,{10,10},false); messages.monster(PHANTOM); messages.text("hits you!");
    obs=observe_explorer(2,2);
    check(obs.attacked && obs.reported_attacker==PHANTOM && !obs.contact_reported && obs.contact.x==255,"incoming message leaked attacker bearing");
    arena(); game.hp=1; game.inventory[0]={SCROLL_TELEPORT,1}; identify_type(SCROLL_TELEPORT);
    obs=observe_explorer(2,2); a->reset(); auto action=a->choose_action(obs);
    check(action.kind==ExplorerActionKind::Use && action.slot==0,"reported invisible attacker did not trigger emergency escape");
    messages.prepare(attempted,{10,10},true); messages.text("You miss"); messages.monster(PHANTOM);
    check(!observe_explorer(3,3).contact_reported,"confused action falsely inferred contact location");
}
void competence() {
    ExplorerParameters prefs;
    auto p=std::make_unique<ExplorerAgent>(prefs);
    arena(); game.hp=60; game.inventory[0]={POTION_HEALING,2}; identify_type(POTION_HEALING);
    auto o=observe_explorer(10,10);
    check(p->choose_action(o).kind!=ExplorerActionKind::Use,"safe health uses combat healing threshold");
    game.monsters[0]={{11,10},SNAKE,3,0,{0,0},MON_AGGRO}; p->reset();
    check(p->choose_action(observe_explorer(10,10)).slot==0,"combat healing threshold remains redundant");
    arena(); game.hp=25; game.monsters[0]={{11,10},LORD,112,0,{0,0},MON_AGGRO}; p->reset();
    p->choose_action(observe_explorer(10,10));
    check(p->risk.danger>=Danger::Serious,"lone high-damage monster recognized too late");
    arena(); game.monsters[0]={{11,10},SNAKE,3,0,{0,0},MON_AGGRO}; p->reset(); p->choose_action(observe_explorer(10,10));
    check(p->risk.danger==Danger::Caution,"trivial healthy encounter panics");
    game.monsters[1]={{10,11},SNAKE,3,0,{0,0},MON_AGGRO}; game.hp=6; p->reset(); p->choose_action(observe_explorer(10,10));
    check(p->risk.danger>=Danger::Serious && p->risk.attackers==2,"multiple weak enemies underestimated");
    arena(); game.hp=10; game.inventory[0]={POTION_HEALING,1}; game.inventory[1]={SCROLL_TELEPORT,1};
    identify_type(POTION_HEALING); identify_type(SCROLL_TELEPORT);
    game.monsters[0]={{11,10},DRAGON,48,0,{0,0},MON_AGGRO}; p->reset(); auto a=p->choose_action(observe_explorer(10,10));
    check(p->risk.fire && a.kind==ExplorerActionKind::Use && a.slot==1,"lethal dragon breath did not prefer teleport to inadequate heal");
    game.inventory[1]={}; game.inventory[2]={POTION_PARALYSIS,1}; identify_type(POTION_PARALYSIS); p->reset();
    a=p->choose_action(observe_explorer(10,10));
    check(a.kind==ExplorerActionKind::Throw && a.slot==2 && a.dx==1,"lethal encounter did not prefer disabling to inadequate heal");
    arena(); game.hp=30; game.inventory[0]={POTION_HEALING,1}; identify_type(POTION_HEALING); p->reset();
    check(p->choose_action(observe_explorer(10,10)).slot==0,"safe low HP did not heal");
    arena(); game.hp=20; game.confused=4; game.inventory[0]={SCROLL_TELEPORT,1}; identify_type(SCROLL_TELEPORT);
    game.monsters[0]={{11,10},ANGEL,24,0,{0,0},MON_AGGRO}; p->reset(); a=p->choose_action(observe_explorer(10,10));
    check(a.kind==ExplorerActionKind::Use && a.slot==0,"confused defense ignored escape");
    game.paralyzed=2; p->reset(); check(p->choose_action(observe_explorer(10,10)).kind==ExplorerActionKind::Wait,"paralysis attempted unavailable action");
    arena(); game.inventory[0]={LANTERN,150}; game.inventory[1]=make_equipment(BUCKLER,0); game.offhand_slot=1;
    p->reset(); a=p->choose_action(observe_explorer(10,10)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"exploration undervalues lantern");
    game.inventory[2]={AMULET_BRILLIANCE,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(AMULET_BRILLIANCE); game.amulet_slot=2;
    p->reset(); a=p->choose_action(observe_explorer(10,10)); check(!(a.kind==ExplorerActionKind::Use && a.slot==0),"permanent sight did not preserve shield option");
    game.blindness=4; p->reset(); a=p->choose_action(observe_explorer(10,10)); check(!(a.kind==ExplorerActionKind::Use && a.slot==0),"blindness incorrectly values light");
    // Constructed memory has unknown cells behind a known closed barrier.
    ExplorerFloor f; for(auto& tile:f.tiles) { tile.terrain=Terrain::Wall; tile.surveyed=true; }
    f.tiles[cell(10,10)].terrain=Terrain::Floor;
    f.tiles[cell(11,10)].terrain=Terrain::ClosedDoor;
    for(int x=12;x<17;++x) { f.tiles[cell(x,10)].terrain=Terrain::Unknown; f.tiles[cell(x,10)].surveyed=false; }
    check(expected_information(f,{10,10},6,true)==0 && expected_information(f,{10,10},6,false)>0,"frontier gain sees through known closed doors");
    f.tiles[cell(11,10)].terrain=Terrain::OpenDoor;
    check(expected_information(f,{10,10},3,true)==4,"circular optimistic unknown gain incorrect");
    f.tiles[cell(12,10)].terrain=Terrain::Floor; f.tiles[cell(12,10)].surveyed=false;
    check(expected_information(f,{10,10},3,true)==3,"mapped unsurveyed terrain counted as unknown");
    arena(); o=observe_explorer(0,0); o.attacked=o.incoming_attack=true; o.reported_attacker=PHANTOM;
    auto info=monster_info(PHANTOM); o.reported_species={{},PHANTOM,info.strength,info.speed,info.armor,info.flags};
    p->reset(); bool moved=false;
    for(int turn=0;turn<12;++turn) { o.turns=o.actions=turn; o.hp=uint8_t(40-turn*2); o.contact_reported=true; o.contact={11,10}; a=p->choose_action(o); if(a.kind==ExplorerActionKind::Move && a.dx!=1) { moved=true; break; } }
    check(moved,"invisible attrition not reassessed before old 128-turn fallback");
    check(o.reported_species.pos.x==255,"public attacker definition includes bearing");
    ExplorerMessages messages; ExplorerMessageScope scope(&messages);
    ExplorerAction attempted; attempted.kind=ExplorerActionKind::Move; attempted.dx=1;
    for(uint8_t type:{uint8_t(PHANTOM),uint8_t(ORC)}) {
        arena(); game.monsters[0]={{11,10},type,20,0,{0,0},MON_AGGRO};
        if(type==ORC) set_monster_effect(game.monsters[0],MON_INVISIBLE,6);
        messages.prepare(attempted,{10,10},false);
        status(game.monsters[0]); status("hits you!");
        auto hidden=observe_explorer(1,1);
        check(hidden.incoming_attack && hidden.reported_attacker==0 && hidden.reported_species.type==0 && hidden.contact.x==255,"something hit message disclosed species or bearing");
        messages.prepare(attempted,{10,10},false); status("You miss"); status(game.monsters[0],'.');
        hidden=observe_explorer(2,2);
        check(hidden.contact_reported && hidden.contact==SeenPosition{11,10} && hidden.contact_type==0,"anonymous melee contact lost confirmation or disclosed identity");
    }
    arena(); game.monsters[0]={{11,10},PHANTOM,20,0,{0,0},MON_AGGRO};
    game.inventory[0]={RING_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; game.ring_slots[0]=0; identify_type(RING_SEE_INVISIBLE);
    messages.prepare(attempted,{10,10},false); status(game.monsters[0]); status("hits you!");
    check(observe_explorer(1,1).reported_attacker==PHANTOM,"see-invisible did not reveal legitimately printed identity");
}
}
int main() {
    try { noninterference(); perception_and_memory(); identification_and_mapping(); navigation_and_tactics(); stationary_contact_detour(); controlled_roundtrip(); replay(); parameters_and_messages(); competence();
        std::cout<<checks<<" explorer checks passed; policy bytes="<<sizeof(ExplorerAgent)<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<"explorer check failed: "<<e.what()<<'\n'; return 1; }
}
