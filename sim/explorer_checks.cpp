#include "explorer_observation.hpp"
#include "simulator.hpp"
#include "game_internal.hpp"
#include "spells.hpp"
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
    s<<int(i.category)<<':'<<int(i.type)<<':'<<int(i.appearance)<<':'<<int(i.quantity)<<':'<<int(i.magnitude)<<':'<<int(i.modifier)<<':'<<int(i.enchant)<<':'<<int(i.spell)<<':'<<i.individual<<':'<<i.cursed<<';';
}
std::string observation_text(const ExplorerObservation& o) {
    std::ostringstream s;
    s<<int(o.player.x)<<':'<<int(o.player.y)<<':'<<int(o.floor)<<':'<<int(o.hp)<<':'<<int(o.max_hp)<<':'<<int(o.level)<<':'<<int(o.xp)<<':'<<int(o.strength)<<':'<<int(o.dexterity)<<':'<<int(o.speed)<<':'<<int(o.hunger)<<':'<<int(o.radius)<<':'<<o.yendor<<':'<<o.pending_pickups<<':'<<o.actions<<':'<<o.turns<<':'<<o.attacked<<':'<<o.incoming_attack<<':'<<int(o.hp_loss);
    s<<':'<<int(o.reported_attacker)<<':'<<int(o.reported_species.strength)<<':'<<o.reported_species.traits
        <<':'<<o.contact_reported<<':'<<int(o.contact_type)<<':'<<int(o.contact.x)<<':'<<int(o.contact.y)
        <<':'<<int(o.contact_origin.x)<<':'<<int(o.contact_origin.y)<<':'<<int(o.melee_feedback)
        <<':'<<int(o.incoming_attacks)<<':'<<int(o.anonymous_attacks)<<':'<<o.encounter_changed<<':'<<o.summoning_reported;
    s<<':'<<o.known_spells<<':'<<int(o.mana)<<':'<<int(o.max_mana)<<':'<<o.studying<<':'<<int(o.study_spell)<<':'<<int(o.study_progress);
    for(auto b:o.equipped) s<<int(b)<<',';
    for(auto b:{o.weakened,o.confused,o.paralyzed,o.slowed,o.invisible,o.blindness}) s<<int(b)<<',';
    for(auto b:{o.see_invisible,o.haste,o.levitation,o.fire_resistance,o.vitality_drained}) s<<int(b)<<',';
    for(auto& i:o.inventory) item_text(s,i);
    for(int n=0;n<2048;++n) s<<int(o.terrain[n])<<':'<<int(o.features[n])<<':'<<o.visible[n]<<':'<<o.detected_objects[n]<<',';
    for(int g=0;g<o.ground_count;++g) { auto& a=o.ground[g]; s<<int(a.pos.x)<<':'<<int(a.pos.y)<<':'<<int(a.handle); item_text(s,a.item); }
    for(int m=0;m<o.monster_count;++m) { auto& a=o.monsters[m]; s<<int(a.pos.x)<<':'<<int(a.pos.y)<<':'<<int(a.type)<<':'<<int(a.strength)<<':'<<int(a.speed)<<':'<<int(a.armor)<<':'<<a.traits<<':'<<int(a.handle); }
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
    a.confused=a.paralyzed=a.slowed=a.invisible=a.blindness=2;
    a.see_invisible=a.haste=a.levitation=a.fire_resistance=2; b=a;
    b.confused=b.paralyzed=b.slowed=b.invisible=b.blindness=7;
    b.see_invisible=b.haste=b.levitation=b.fire_resistance=7;
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
    ExplorerParameters prefs; prefs.stationary_contact_turns=32; prefs.invisible_response=0; prefs.contact_aware=0;
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
void invisible_contact_checks() {
    arena(); auto o=observe_explorer(0,0); auto p=std::make_unique<ExplorerAgent>();
    o.contact_reported=true; o.contact={11,10}; o.contact_origin=o.player;
    o.melee_feedback=MeleeFeedback::Miss; o.incoming_attack=true; o.anonymous_attacks=o.incoming_attacks=1;
    Game before=game;
    for(int turn=0;turn<48;++turn) {
        o.actions=o.turns=turn; auto a=p->choose_action(o);
        check(a.kind==ExplorerActionKind::Move && a.dx==1 && a.dy==0,"favorable roll misses abandoned fresh east contact");
        check(p->contact_memory().state==ContactState::Confirmed,"melee miss failed to confirm contact");
    }
    check(std::memcmp(&before,&game,sizeof game)==0,"contact policy mutated production state/RNG");
    p->reset(); o.melee_feedback=MeleeFeedback::Hit;
    for(int turn=0;turn<160;++turn) {
        o.actions=o.turns=turn; auto a=p->choose_action(o);
        check(a.kind==ExplorerActionKind::Move && a.dx==1,"productive healthy contact timed out");
    }
    // Noncombat feedback reduces confidence briefly; it does not erase the square.
    o.actions=o.turns=160; o.contact_reported=false; o.incoming_attack=false; o.anonymous_attacks=0;
    o.pending_pickups=true; p->choose_action(o);
    // The preceding action WAS a strike, so missing its result invalidates it.
    check(p->contact_memory().square.x==255,"failed repeated strike retained certainty");
    o.pending_pickups=false; o.actions=o.turns=161; o.incoming_attack=true; o.anonymous_attacks=1;
    auto a=p->choose_action(o); check(!(a.kind==ExplorerActionKind::Move && a.dx==1),"failed strike immediately retested east");
    p->reset(); o.actions=o.turns=0; o.contact_reported=true; o.melee_feedback=MeleeFeedback::Hit; p->choose_action(o);
    o.actions=o.turns=1; o.contact_reported=false; o.player={10,9}; p->choose_action(o);
    check(p->contact_memory().square.x==255,"player movement translated stale contact bearing");
    o.player={10,10}; p->reset(); o.actions=o.turns=0; o.contact_reported=true; o.encounter_changed=true;
    p->choose_action(o); check(p->contact_memory().state==ContactState::Uncertain,"knockback retained certainty");
    o.encounter_changed=false; o.melee_feedback=MeleeFeedback::Defeat; o.incoming_attack=false; o.anonymous_attacks=0;
    o.actions=o.turns=1; p->choose_action(o); check(p->contact_memory().square.x==255,"defeat retained occupied square");
    // A different action can preserve uncertain contact for a short retry.
    p->reset(); o.actions=o.turns=0; o.contact_reported=true; o.melee_feedback=MeleeFeedback::Hit;
    o.inventory[0]={ItemCategory::Potion,POTION_HEALING,255,1}; o.hp=30;
    a=p->choose_action(o); check(a.kind==ExplorerActionKind::Use,"contact prevented healing");
    o.inventory[0]={}; o.hp=80; o.actions=o.turns=1; o.contact_reported=false;
    p->choose_action(o); check(p->contact_memory().state==ContactState::Uncertain && p->contact_memory().square==SeenPosition{11,10},"healing instantly forgot contact");
    o.actions=o.turns=5; p->choose_action(o); check(p->contact_memory().square.x==255,"unconfirmed square never expired");
    // No bearing is supplied by two anonymous messages; probing stays bounded.
    arena(); o=observe_explorer(0,0); o.incoming_attack=true; o.incoming_attacks=o.anonymous_attacks=2;
    p->reset(); unsigned probes=0;
    for(int turn=0;turn<8;++turn) {
        o.actions=o.turns=turn; a=p->choose_action(o); probes+=a.goal=="probe unseen contact candidates";
        check(p->contact_memory().square.x==255,"incoming attacks invented a bearing");
    }
    check(probes<=4 && p->risk.unseen && p->risk.attackers>=2,"multiple attacker probing/risk unbounded or absent");
    o.contact_reported=true; o.contact={11,10}; o.contact_origin=o.player; o.melee_feedback=MeleeFeedback::Hit;
    o.hp=10; o.hp_loss=12; o.inventory[0]={ItemCategory::Scroll,SCROLL_TELEPORT,255,1}; o.actions=o.turns=9;
    a=p->choose_action(o); check(a.kind==ExplorerActionKind::Use && a.slot==0,"dangerous confirmed contact prevented escape");
    arena(); o=observe_explorer(0,0); o.hp=10; o.hp_loss=8; o.incoming_attack=true; o.anonymous_attacks=1;
    p->reset(); a=p->choose_action(o);
    check(p->contact_memory().state==ContactState::Disengaging && a.kind==ExplorerActionKind::Move,"unknown lethal pressure did not start retreat");
    o.actions=o.turns=1; o.hp_loss=0; o.contact_reported=true; o.contact_origin=o.player;
    o.contact={uint8_t(int(o.player.x)+a.dx),uint8_t(int(o.player.y)+a.dy)}; o.melee_feedback=MeleeFeedback::Miss;
    auto retreat=p->choose_action(o);
    check(p->contact_memory().square==o.contact && retreat.kind==ExplorerActionKind::Move && retreat.destination!=o.contact,"new contact during disengagement selected the occupied escape square");
    p->reset(); o=observe_explorer(0,0); o.hp=10; o.hp_loss=8; o.attacked=o.incoming_attack=true; o.anonymous_attacks=2;
    unsigned retreat_attempts=0; a=p->choose_action(o);
    for(int turn=1;turn<20;++turn) {
        retreat_attempts+=a.goal=="disengage from unseen encounter";
        o.actions=o.turns=turn; o.hp_loss=0; o.contact_reported=a.kind==ExplorerActionKind::Move;
        o.contact_origin=o.player; o.contact={uint8_t(int(o.player.x)+a.dx),uint8_t(int(o.player.y)+a.dy)};
        o.melee_feedback=MeleeFeedback::Miss; a=p->choose_action(o);
    }
    check(retreat_attempts<=unsigned(p->parameters.retreat_steps),"failed unseen retreats ignored the existing movement budget");
    // Mixed threats preserve anonymous pressure and prioritize visible combat.
    arena(); o=observe_explorer(0,0); auto info=monster_info(LORD);
    o.monsters[0]={{10,9},LORD,info.strength,info.speed,info.armor,info.flags}; o.monster_count=1;
    o.incoming_attack=true; o.incoming_attacks=o.anonymous_attacks=1; o.contact_reported=true; o.contact={11,10};
    o.melee_feedback=MeleeFeedback::Hit; p->reset(); a=p->choose_action(o);
    check(p->risk.unseen && p->risk.attackers>=2,"visible enemy erased anonymous danger");
    check(!(a.kind==ExplorerActionKind::Move && a.dx==1),"contact outranked visible lethal threat");
    o.monster_count=0; o.hp=100; o.confused=1; p->reset(); p->choose_action(o);
    check(p->contact_memory().square.x==255,"confused intention established contact");
    o.confused=0; o.summoning_reported=true; o.hp_loss=0; p->reset(); bool disengaged=false;
    for(int turn=0;turn<40;++turn) {
        o.actions=o.turns=turn; a=p->choose_action(o);
        if(p->contact_memory().state==ContactState::Disengaging) { disengaged=true; break; }
    }
    check(disengaged,"reported summoning replenishment caused indefinite combat");
    o.summoning_reported=false; p->reset(); o.melee_feedback=MeleeFeedback::Miss; disengaged=false;
    for(int turn=0;turn<80;++turn) { o.actions=o.turns=turn; p->choose_action(o); if(p->contact_memory().state==ContactState::Disengaging) { disengaged=true; break; } }
    check(disengaged,"no-progress misses lacked a separate loop safeguard");
    // Separate deaths can reset short episodes while replacements hold the
    // player stationary. A final position-based guard must survive those resets.
    p->reset(); o.incoming_attack=false; o.anonymous_attacks=0; disengaged=false;
    for(int turn=0;turn<1030;++turn) {
        o.actions=o.turns=turn; o.contact_reported=turn%2==0;
        o.melee_feedback=o.contact_reported ? MeleeFeedback::Hit : MeleeFeedback::Defeat;
        p->choose_action(o);
        if(p->contact_memory().state==ContactState::Disengaging) { disengaged=true; break; }
    }
    check(disengaged,"stationary replacement deaths reset the absolute loop guard");
    p->reset(); o.melee_feedback=MeleeFeedback::Hit; o.actions=o.turns=0; p->choose_action(o);
    o.floor=1; o.contact_reported=false; o.incoming_attack=false; o.actions=o.turns=1; p->choose_action(o);
    check(p->contact_memory().state==ContactState::None,"contact survived floor transition");
    // Actual production dispatch supplies miss, hit and defeat, sanitized first.
    ExplorerMessages messages; ExplorerMessageScope scope(&messages); ExplorerAction attempted;
    attempted.kind=ExplorerActionKind::Move; attempted.dx=1; bool hit=false,miss=false,defeat=false;
    for(int seed=1;seed<64 && !(hit&&miss&&defeat);++seed) {
        arena(); game.random_state=uint16_t(seed); game.monsters[0]={{11,10},PHANTOM,uint8_t(seed%2 ? 100 : 1),0,{0,0},MON_AGGRO};
        messages.prepare(attempted,{10,10},false); Action act; act.kind=ActionKind::Move; act.dx=1; dispatch(act);
        auto actual=observe_explorer(1,1);
        if(actual.melee_feedback==MeleeFeedback::Defeat) check(!actual.contact_reported,"defeat invented a target association");
        else check(actual.contact_reported && actual.contact_type==0 && actual.contact_origin==SeenPosition{10,10},"production melee feedback missing or leaked species");
        hit|=actual.melee_feedback==MeleeFeedback::Hit; miss|=actual.melee_feedback==MeleeFeedback::Miss; defeat|=actual.melee_feedback==MeleeFeedback::Defeat;
    }
    check(hit&&miss&&defeat,"production status cases not exercised");
    arena(); messages.prepare(attempted,{10,10},false); Action act; act.kind=ActionKind::Move; act.dx=1; dispatch(act);
    check(!observe_explorer(1,1).contact_reported,"empty movement confirmed melee");
    arena(); game.walls[cell(11,10)>>3]|=uint8_t(1u<<(cell(11,10)&7)); messages.prepare(attempted,{10,10},false); dispatch(act);
    check(!observe_explorer(1,1).contact_reported,"wall bump confirmed melee");
    // An unrelated death during an empty move cannot reveal a false bearing.
    messages.prepare(attempted,{10,10},false); messages.text("You defeat"); messages.monster(0);
    check(!observe_explorer(2,2).contact_reported,"unattributed death disclosed attempted square");
    for(const char* change:{"is blasted back!","disappears!","changes form!","triggers a teleport trap!"}) {
        messages.prepare(attempted,{10,10},false); messages.text("You hit"); messages.monster(0);
        messages.text(change); check(observe_explorer(2,2).encounter_changed,"observable displacement/form change lost");
    }
    messages.prepare(attempted,{10,10},false); messages.monster(0); messages.text("hits you!"); messages.monster(0); messages.text("misses you.");
    auto anonymous=observe_explorer(2,2);
    check(anonymous.anonymous_attacks==2 && anonymous.incoming_attacks==2 && anonymous.contact.x==255,"multiple printed attacks disclosed bearing or disappeared");
    // Identical contact histories remain identical under hidden HP/position/RNG changes.
    arena(); auto first=observe_explorer(0,0); first.contact_reported=true; first.contact={11,10}; first.contact_origin=first.player; first.melee_feedback=MeleeFeedback::Miss;
    auto other=std::make_unique<ExplorerAgent>(); p->reset(); other->reset();
    for(int turn=0;turn<20;++turn) {
        first.actions=first.turns=turn; auto expected=p->choose_action(first);
        game.random_state=uint16_t(200+turn); game.monsters[0]={{uint8_t(20+turn),20},PHANTOM,uint8_t(1+turn),0,{0,0},MON_AGGRO};
        Game unchanged=game; auto actual=other->choose_action(first);
        check(action_text(expected)==action_text(actual) && !std::memcmp(&unchanged,&game,sizeof game),"contact history read/mutated hidden state or RNG");
    }
    // Production telemetry accounts an initially unseen kill without informing policy.
    arena(); game.monsters[0]={{11,10},PHANTOM,1,0,{0,0},MON_AGGRO}; Collector collector; collector.data.agent="explorer-v1";
    CollectScope collection(collector); collector.enter_floor(); ExplorerObserver observer;
    for(unsigned turn=0;turn<64 && game.monsters[0].type;++turn) {
        collector.observe_exploration(observer.observe(turn,turn)); collector.prepare_action(act);
        messages.prepare(attempted,{10,10},false); dispatch(act); collector.observe_contact(observer.observe(turn+1,turn+1));
    }
    collector.close_floor(false); auto metrics=collector.data.floors.back().exploration.invisible;
    check(metrics.encounters==1 && metrics.kills==1 && metrics.ordinary_kills==1,"invisible encounter/kill telemetry failed production dispatch");
    check(metrics.confirmations==metrics.hits+metrics.misses && metrics.damage==metrics.ordinary_damage+metrics.hazard_damage,"invisible telemetry does not reconcile");
    std::ostringstream header,row; write_exploration_header(header); write_exploration(row,collector.data);
    check(header.str().find("unseen_probes_to_first_contact")!=std::string::npos && header.str().find("phantom_deaths")!=std::string::npos,"invisible schema counters missing");
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
    std::ostringstream roundtrip_trace; Options roundtrip_options; roundtrip_options.trace=&roundtrip_trace;
    auto p=std::make_unique<ExplorerAgent>(); auto r=run_started(24,*p,roundtrip_options);
    if(r.result!="escaped") { auto text=roundtrip_trace.str(); std::cerr<<text.substr(text.size()>10000 ? text.size()-10000 : 0); }
    check(r.result=="escaped" && r.has_yendor && r.floors_exited==16,"controlled Lord/Yendor/full ascent did not escape");
    check(r.monsters[LORD].killed==1 && r.items[YENDOR_AMULET].picked_up==1,"controlled roundtrip bypassed production Lord/Yendor");
}
void hazard_navigation() {
    auto corridor=[] {
        ExplorerObservation o; o.player={10,10}; o.hp=o.max_hp=100; o.level=5;
        o.strength=5; o.dexterity=o.speed=4; o.hunger=220; o.radius=6;
        o.terrain.fill(Terrain::Wall); o.visible.set();
        for(int x=10;x<=14;++x) o.terrain[cell(x,10)]=Terrain::Floor;
        o.terrain[cell(14,10)]=Terrain::Down;
        o.features[cell(11,10)]=FEATURE_SUMMONING_TRAP;
        return o;
    };
    auto p=std::make_unique<ExplorerAgent>(); auto o=corridor();
    for(int x=10;x<=14;++x) o.terrain[cell(x,11)]=Terrain::Floor;
    auto a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && a.dy==1 && !a.essential_hazard,"safe summoning detour lost to short hazardous route");
    // Every step of a safe route avoids re-entry, including previously visited cells.
    for(unsigned i=0;i<12 && o.player!=SeenPosition{14,10};++i) {
        a=p->choose_action(o);
        if(a.kind!=ExplorerActionKind::Move) break;
        o.player={uint8_t(o.player.x+a.dx),uint8_t(o.player.y+a.dy)}; ++o.actions; ++o.turns;
        check(o.features[cell(o.player.x,o.player.y)]!=FEATURE_SUMMONING_TRAP,"safe route entered summoning trap");
    }
    check(o.player==SeenPosition{14,10},"safe detour failed to reach stairs");
    p->reset(); o=corridor(); a=p->choose_action(o);
    check(a.dx==1 && a.essential_hazard,"mandatory trapped passage permanently blocked");
    o.player={11,10}; ++o.actions; ++o.turns; o.summoning_reported=true; o.hp_loss=12; o.hp=88;
    a=p->choose_action(o);
    check(p->memory(0).tiles[cell(11,10)].hazard_entries==1 && p->memory(0).tiles[cell(11,10)].hazard_loss==12,"activation history not retained from public observations");
    check(a.dx==1,"essential crossing did not continue away from trap");
    p->reset(); o=corridor(); o.terrain[cell(14,10)]=Terrain::Floor;
    o.ground_count=1; o.ground[0].pos={14,10}; o.ground[0].item.category=ItemCategory::Ammo;
    a=p->choose_action(o); check(a.no_path && !a.essential_hazard,"optional loot licensed dangerous crossing");
    check(p->stats.hazard_abandoned==1,"hazard abandonment was not diagnosed");
    // A trap can be the last observed frontier, rather than an intermediate
    // square on a route to known stairs. It still permits essential progression.
    p->reset(); o=corridor(); o.features.fill(0); o.ground_count=0;
    o.terrain[cell(13,10)]=o.terrain[cell(14,10)]=Terrain::Unknown;
    o.visible.reset(cell(13,10)); o.visible.reset(cell(14,10));
    o.features[cell(12,10)]=FEATURE_SUMMONING_TRAP;
    a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && a.dx==1 && a.essential_hazard &&
          a.destination==SeenPosition{12,10},"summoning trap frontier incorrectly exhausted exploration");
    o.player={11,10}; ++o.actions; ++o.turns; a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && a.dx==1 && a.essential_hazard,
          "essential trap frontier lost its destination before entry");
    // Authoritative hidden traps cannot affect an equal sanitized input.
    arena(); game.features[0]={{11,10},uint8_t(FEATURE_SUMMONING_TRAP|MAP_FEATURE_HIDDEN)};
    p->reset(); o=corridor(); o.features.fill(0); a=p->choose_action(o);
    check(a.dx==1 && !a.essential_hazard,"policy inspected authoritative hidden trap");
    auto other=std::make_unique<ExplorerAgent>(); game.features[0]={};
    check(action_text(a)==action_text(other->choose_action(o)),"hidden trap changed equal-input navigation");
    p->reset(); o=corridor(); o.incoming_attack=o.attacked=true; o.anonymous_attacks=1;
    o.hp=15; o.hp_loss=8; o.terrain[cell(10,11)]=Terrain::Floor;
    a=p->choose_action(o);
    check(a.kind==ExplorerActionKind::Move && !(a.dx==1 && a.dy==0),"unseen disengagement preferred known trap over safe retreat");
    p->reset(); o=corridor(); o.features.fill(0); o.terrain[cell(11,10)]=Terrain::ClosedDoor;
    a=p->choose_action(o); check(a.dx==1,"closed door ceased to be a legal opening route");
    ++o.actions; ++o.turns; o.terrain[cell(11,10)]=Terrain::OpenDoor;
    a=p->choose_action(o); check(a.dx==1 && p->memory(0).tiles[cell(11,10)].terrain==Terrain::OpenDoor,"observed opening did not refresh route");
    ++o.actions; ++o.turns; o.terrain[cell(11,10)]=Terrain::ClosedDoor;
    p->choose_action(o); check(p->memory(0).tiles[cell(11,10)].terrain==Terrain::ClosedDoor,"observed closure not retained");
    p->reset(); o=corridor(); p->choose_action(o);
    o.floor=1; ++o.actions; p->choose_action(o);
    o.floor=0; o.yendor=true; ++o.actions; o.features.fill(0); o.terrain[cell(14,10)]=Terrain::Up;
    a=p->choose_action(o);
    check(a.dx==1 && p->memory(0).tiles[cell(11,10)].feature==0,"ascent reused descent hazard location");
    p->reset(); o=corridor(); o.terrain[cell(11,10)]=Terrain::Wall;
    for(unsigned i=0;i<20;++i) { ++o.actions; ++o.turns; a=p->choose_action(o); check(a.no_path,"no-path handling fabricated terrain or a route"); }
    // Equal-value optional targets are consumed once and cannot create a movement cycle.
    p->reset(); o=corridor(); o.features.fill(0);
    for(int y=8;y<=12;++y) o.terrain[cell(10,y)]=Terrain::Floor;
    o.ground_count=2;
    for(int i=0;i<2;++i) { o.ground[i].pos={10,uint8_t(i ? 12 : 8)}; o.ground[i].handle=uint8_t(i); o.ground[i].item.category=ItemCategory::Food; o.ground[i].item.type=FOOD; o.ground[i].item.quantity=1; }
    unsigned movements=0;
    for(unsigned i=0;i<60;++i) {
        a=p->choose_action(o); ++o.actions; ++o.turns;
        if(a.kind==ExplorerActionKind::Move) { o.player={uint8_t(o.player.x+a.dx),uint8_t(o.player.y+a.dy)}; ++movements; }
        if(a.kind==ExplorerActionKind::Take) {
            for(unsigned g=0;g<o.ground_count;++g) if(o.ground[g].pos==o.player) { o.ground[g]=o.ground[--o.ground_count]; break; }
        }
        if(a.kind==ExplorerActionKind::Stairs) break;
    }
    check(o.player==SeenPosition{14,10} && movements<24,"competing goals caused unlimited movement cycle");
}
void replay() {
    auto p=std::make_unique<ExplorerAgent>(); Options o; o.agent=p->name();
    std::ostringstream t1,t2; o.trace=&t1; auto a=run(42,*p,o); auto final=game;
    o.trace=&t2; auto b=run(42,*p,o);
    check(a.action_hash==b.action_hash && a.result==b.result && t1.str()==t2.str() && !std::memcmp(&final,&game,sizeof game),"explorer replay changed");
    o.trace=nullptr; o.telemetry=false; auto c=run(42,*p,o);
    check(a.action_hash==c.action_hash && !std::memcmp(&final,&game,sizeof game),"telemetry changed explorer gameplay");
    for(uint16_t seed=1;seed<=32;++seed) { auto r=run(seed,*p); check(!r.stuck,"constructed competence seeds stuck"); }
    Options regression; regression.max_actions=100000;
    for(uint16_t seed:{uint16_t(2478),uint16_t(41822)}) {
        auto r=run(seed,*p,regression);
        check(!r.stuck && (r.result=="death" || r.result=="escaped"),
              "recorded summoning-frontier/empty-wand failure regressed");
    }
    regression.max_actions=60000;
    auto disabled=std::make_shared<RuleExperiment>();
    disabled->rules={parse_rule("balance:magic=0")}; regression.experiment=disabled;
    auto r=run(43779,*p,regression);
    check(!r.stuck && (r.result=="death" || r.result=="escaped"),
          "recorded perpetual visible-combat failure regressed");
}
void prolonged_visible_combat() {
    arena(); game.monsters[0]={{11,10},SNAKE,3,0,{0,0},MON_AGGRO};
    auto o=observe_explorer(0,0);
    // A single known exit is sufficient after a prolonged fight. The policy
    // receives no live monster HP, spawning source or hidden regeneration data.
    o.terrain[cell(10,9)]=o.terrain[cell(10,11)]=Terrain::Wall;
    auto p=std::make_unique<ExplorerAgent>(); p->reset();
    for(unsigned n=0;n<64;++n) {
        o.actions=o.turns=n;
        o.melee_feedback=n%5 ? MeleeFeedback::Hit : MeleeFeedback::Defeat;
        auto a=p->choose_action(o);
        check(a.goal=="melee visible monster","ordinary visible fight disengaged before guard");
    }
    o.actions=o.turns=64;
    auto a=p->choose_action(o);
    check(a.goal=="bounded observed retreat" && a.dx==-1 && !a.dy,
          "replacement visible opponents reset bounded combat guard");
    p->reset();
    check(p->choose_action(o).goal=="melee visible monster","reset retained prolonged-combat history");
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
    arena(); game.hp=1; game.inventory[0]={WAND_FORCE,0};
    identify_type(WAND_FORCE); game.monsters[0]={{11,10},ANGEL,24,0,{0,0},MON_AGGRO};
    game.monsters[1]={{10,11},ANGEL,24,0,{0,0},MON_AGGRO}; p->reset();
    o=observe_explorer(100,100); a=p->choose_action(o);
    check(a.goal!="disable visible survival threat","unexamined depleted wand entered emergency control loop");
    o.last_rejected=true; ++o.actions; a=p->choose_action(o);
    check(a.kind!=ExplorerActionKind::Wand,"rejected unexamined wand retried without advancing turns");
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
void potion_capacity_boundary() {
    arena(); game.monsters[0]={{11,10},PHANTOM,12,0,{0,0},MON_AGGRO};
    game.see_invisible=3; auto a=observe_explorer(1,1);
    game.see_invisible=80; auto b=observe_explorer(1,1);
    check(a.see_invisible && b.see_invisible && observation_text(a)==observation_text(b),"remaining sight duration disclosed");
    game.blindness=1; check(observe_explorer(1,1).monster_count==0,"dose leaked blind monster positions");
    arena(); game.inventory[19]={POTION_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(POTION_SEE_INVISIBLE);
    auto o=observe_explorer(0,0); o.incoming_attack=true; o.anonymous_attacks=1; o.hp_loss=1;
    auto p=std::make_unique<ExplorerAgent>(); p->reset(); auto action=p->choose_action(o);
    check(action.kind==ExplorerActionKind::Use && action.slot==19,"identified high-slot sight not used for invisible contact");
    std::memset(game.identified_items,0,sizeof game.identified_items); o=observe_explorer(0,0); o.incoming_attack=true; o.anonymous_attacks=1;
    p->reset(); action=p->choose_action(o);
    check(o.inventory[19].type==0 && action.goal!="temporary sight against invisible contact","hidden potion identity informed sight choice");
    arena(); for(unsigned n=0;n<GROUND_ITEMS;++n) game.ground[n]={{10,10},{FOOD,1}};
    for(unsigned n=0;n<MONSTERS;++n) { game.monsters[n]={{10,10},MIMIC,20,0,{0,0},0}; set_mimic_appearance(game.monsters[n],MIMIC_SCROLL); }
    o=observe_explorer(0,0); check(o.ground_count==GROUND_ITEMS+MONSTERS,"expanded ground/mimic observation truncated");
    for(unsigned n=0;n<GROUND_ITEMS;++n) check(o.ground[n].handle==n,"expanded ground handle changed");
}

}
void consumable_observations() {
    arena(); game.ground[0]={{60,30},{POTION_FIRE,1}};
    game.monsters[0]={{59,30},MIMIC,10,0,{0,0},0};
    auto before=observe_explorer(0,0);
    check(!before.detected_objects.any(),"detection before scroll leaked objects");
    apply_scroll_effect(SCROLL_DETECT_OBJECTS);
    auto detected=observe_explorer(1,1);
    check(detected.detected_objects.count()==1 && detected.detected_objects[cell(60,30)] &&
        detected.terrain[cell(60,30)]==Terrain::Unknown && !detected.features[cell(60,30)] && !detected.monster_count && !detected.ground_count,"detection leaked terrain/identity/mimic");
    auto p=std::make_unique<ExplorerAgent>(); p->reset(); p->choose_action(detected);
    check(p->memory(0).tiles[cell(60,30)].has_item && p->memory(0).tiles[cell(60,30)].terrain==Terrain::Unknown,"object positions not remembered independently of terrain");
    arena(); game.haste=game.levitation=game.fire_resistance=1;
    auto o=observe_explorer(0,0); game.haste=game.levitation=game.fire_resistance=200;
    auto longer=observe_explorer(0,0);
    check(o.haste && o.levitation && o.fire_resistance && longer.haste && longer.levitation && longer.fire_resistance,"public beneficial status flags absent");
    arena(); game.hp=10; game.inventory[0]={POTION_EXTRA_HEALING,1}; identify_type(POTION_EXTRA_HEALING);
    p->reset(); auto a=p->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer extra healing not used");
    arena(); game.vamp_drain=30; game.inventory[0]={POTION_RESTORE_VITALITY,1}; identify_type(POTION_RESTORE_VITALITY);
    p->reset(); a=p->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer vitality drain not restored");
    arena(); game.inventory[0]={POTION_FIRE,1}; identify_type(POTION_FIRE);
    p->reset(); a=p->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Use || a.slot!=0,"explorer deliberately drank fire");
    arena(); game.inventory[0]={SCROLL_SUMMONING,1}; identify_type(SCROLL_SUMMONING);
    p->reset(); a=p->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Use || a.slot!=0,"explorer deliberately read summoning");
    arena(); for(uint8_t t=POTION_FIRST;t<=POTION_LAST;++t) identify_type(t);
    o=observe_explorer(0,0); for(uint8_t t=POTION_FIRST;t<=POTION_LAST;++t) check(o.identities[0][item_appearance(t)]==t,"explorer appearance mapping truncated");
    auto control=std::make_shared<RuleExperiment>();
    control->rules={parse_rule("balance:new-consumables-percent=0")};
    Options options; options.agent=ExplorerAgent::name(); options.experiment=control;
    for(uint16_t seed : {uint16_t(3146),uint16_t(6725),uint16_t(378),uint16_t(7461)})
        check(!run(seed,*p,options).stuck,"explorer frontier/blocked-probe regression reached action guard");
    options.experiment.reset();
    for(uint16_t seed : {uint16_t(6725),uint16_t(7523),uint16_t(6025)})
        check(!run(seed,*p,options).stuck,"new-content explorer blind-probe/loot regression reached action guard");
}
void magic_observations() {
    auto policy=std::make_unique<ExplorerAgent>();
    auto setup=[&](uint8_t spell) {
        arena(); game.known_spells=spell_bit(spell); game.mana=16;
        game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
        game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
        for(uint8_t y=7;y<=13;++y) for(uint8_t x=7;x<=13;++x) carve(x,y);
        policy->reset();
    };
    setup(MAGIC_DART); game.ground[0]={{11,10},{SPELLBOOK,BLINK}};
    auto o=observe_explorer(0,0);
    check(o.mana==16 && o.max_mana==6 && o.known_spells==1 && o.ground_count==1 && o.ground[0].item.category==ItemCategory::Spellbook && o.ground[0].item.type==0 && o.ground[0].item.spell==255,"public magic/book identity absent");
    Game remote_book=game; remote_book.ground[0].item.info=BLOOD_BOLT;
    identical(game,remote_book,"remote spellbook icon disclosed spell identity");
    game.ground[0].item.info=BLINK;
    game.player=game.ground[0].pos; auto inspected=observe_explorer(0,0);
    check(inspected.ground[0].item.spell==BLINK && inspected.ground[0].item.type==SPELLBOOK,"standing pickup inspection lost public book name");
    game.player={10,10};
    Game public_world=game,hidden=game; hidden.ground[1]={{60,30},{SPELLBOOK,EXCHANGE}};
    hidden.monsters[3]={{60,29},LORD,100,0,{0,0},0}; hidden.features[2]={{11,11},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    identical(public_world,hidden,"magic observation leaked hidden content");
    setup(MAGIC_DART); game.monsters[0]={{12,10},GOBLIN,10,15,{0,0},0};
    auto a=policy->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Cast && a.slot==MAGIC_DART && a.dx==1 && !a.dy,"explorer ignored ranged dart");
    check(cast_spell(a.slot,a.dx,a.dy) && game.mana==14 && game.turns==1,"explorer dart execution");
    setup(MAGIC_DART); game.mana=2; game.monsters[0]={{12,10},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Cast,"explorer wasted last mana in mild encounter");
    setup(MAGIC_DART); game.known_spells|=spell_bit(BLINK); game.mana=5;
    game.monsters[0]={{12,10},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Cast,"explorer spent emergency Blink reserve on mild dart");
    setup(SHOCKWAVE); game.hp=1;
    game.monsters[0]={{11,10},GOBLIN,100,15,{0,0},0}; game.monsters[1]={{10,11},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==SHOCKWAVE,"explorer missed defensive Shockwave");
    check(cast_spell(a.slot) && game.monsters[0].pos.x>=13 && game.monsters[1].pos.y>=13,"agent Shockwave failed multi-tile execution");
    setup(SHOCKWAVE); game.hp=1;
    o=observe_explorer(0,0); o.incoming_attack=true; o.anonymous_attacks=2; o.hp_loss=10;
    a=policy->choose_action(o); check(a.kind==ExplorerActionKind::Cast && a.slot==SHOCKWAVE,"explorer ignored public unseen crowd evidence");
    setup(BANISH); game.hp=1; game.monsters[0]={{11,10},PHANTOM,100,15,{0,0},0};
    o=observe_explorer(0,0); o.incoming_attack=true; o.anonymous_attacks=1;
    o.contact_reported=true; o.contact={11,10}; o.contact_origin=o.player; o.melee_feedback=MeleeFeedback::Miss;
    a=policy->choose_action(o); check(!o.monster_count && a.kind==ExplorerActionKind::Cast && a.slot==BANISH && a.dx==1,"explorer ignored confirmed unseen spell bearing");
    o.contact_reported=false; policy->reset(); a=policy->choose_action(o);
    check(a.kind!=ExplorerActionKind::Cast,"explorer invented an unseen spell bearing");
    setup(BLINK); game.hp=1; game.monsters[0]={{11,10},LORD,100,15,{0,0},0};
    game.monsters[1]={{10,11},LORD,100,15,{0,0},0};
    o=observe_explorer(0,0); o.incoming_attack=true; o.hp_loss=20;
    a=policy->choose_action(o); check(a.kind==ExplorerActionKind::Cast && a.slot==BLINK,"explorer missed emergency Blink");
    setup(REPULSION); game.monsters[0]={{11,10},GOBLIN,100,15,{0,0},0}; game.features[0]={{12,10},FEATURE_SPIKE_TRAP};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==REPULSION && a.dx==1,"explorer ignored observed trap knockback");
    for(uint8_t spell : {uint8_t(CONFOUND),uint8_t(FROSTBIND),uint8_t(BANISH)}) {
        setup(spell); game.hp=1; game.monsters[0]={{11,10},LORD,100,15,{0,0},0};
        a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==spell,"explorer missed dangerous ray control");
    }
    setup(PIERCING_BOLT); game.monsters[0]={{11,10},GOBLIN,100,15,{0,0},0}; game.monsters[1]={{13,10},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==PIERCING_BOLT,"explorer missed two-target bolt");
    setup(CLEANSE); game.blindness=10;
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==CLEANSE,"explorer missed harmful blindness cleanse");
    setup(EXCHANGE); game.hp=1;
    game.monsters[0]={{11,10},LORD,100,15,{0,0},0}; game.monsters[4]={{10,13},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Cast && a.slot==EXCHANGE && a.target==4,"explorer failed visible Exchange escape selection");
    game.features[0]={{10,13},FEATURE_SPIKE_TRAP}; policy->reset();
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Cast,"explorer exchanged onto known trap");
    setup(SENSE_TRAPS); game.floor=2; game.mana=player_max_mana();
    o=observe_explorer(0,0);
    a=policy->choose_action(o); check(a.kind==ExplorerActionKind::Cast && a.slot==SENSE_TRAPS,"explorer missed unsurveyed trap detection opportunity");
    o.turns=1; a=policy->choose_action(o); check(a.kind!=ExplorerActionKind::Cast,"explorer repeated trap detection without regeneration/navigation");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,EXCHANGE}; game.inventory[1]={FOOD,1}; game.hunger=90;
    a=policy->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Use && a.slot==1 && use_inventory(a.slot),"explorer failed food preparation for study");
    a=policy->choose_action(observe_explorer(1,1)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer failed study after eating");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,BLINK}; game.inventory[19]={}; game.amulet_slot=NONE;
    game.inventory[1]=make_equipment(ROUND_SHIELD,0); game.offhand_slot=1;
    a=policy->choose_action(observe_explorer(0,0));
    check(a.kind==ExplorerActionKind::Use && a.slot==18 && use_inventory(a.slot),"explorer failed lighting preparation for study");
    a=policy->choose_action(observe_explorer(1,1)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer failed study after lighting");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,FROSTBIND};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer ignored safe study");
    check(use_inventory(a.slot) && studying(),"explorer study initiation rejected");
    a=policy->choose_action(observe_explorer(1,0)); check(a.kind==ExplorerActionKind::Wait,"explorer did another action while studying");
    game.see_invisible=1; check(dispatch(Action{}),"study tick rejected");
    check(!studying() && game.inventory[0].type==SPELLBOOK,"sim study interruption consumed book");
    a=policy->choose_action(observe_explorer(2,1)); check(a.kind!=ExplorerActionKind::Use || a.slot!=0,"explorer retried interrupted study immediately");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,FROSTBIND}; game.monsters[0]={{12,10},GOBLIN,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Use || a.slot!=0,"explorer studied by visible hostile");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,MAGIC_DART};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Use || a.slot!=0,"explorer studied duplicate");
    // The oracle uses the same authoritative spell rules, including safe study.
    setup(SHOCKWAVE); game.hp=1; game.monsters[0]={{11,10},GOBLIN,100,15,{0,0},0}; game.monsters[1]={{10,11},GOBLIN,100,15,{0,0},0};
    OmniscientAgent oracle; auto oa=oracle.choose_action({game,0,0});
    check(oa.kind==ActionKind::Cast && oa.slot==SHOCKWAVE && dispatch(oa),"oracle Shockwave execution");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,EXCHANGE}; game.inventory[1]={FOOD,1}; game.hunger=90; oracle.reset();
    oa=oracle.choose_action({game,0,0}); check(oa.kind==ActionKind::Use && oa.slot==1 && dispatch(oa),"oracle failed food preparation for study");
    setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,EXCHANGE}; oracle.reset(); oa=oracle.choose_action({game,0,0});
    check(oa.kind==ActionKind::Use && oa.slot==0 && dispatch(oa) && studying(),"oracle safe study missing");
    for(unsigned turn=0;turn<80;++turn) { oa=oracle.choose_action({game,turn+1,turn}); check(oa.kind==ActionKind::Wait && dispatch(oa),"oracle study skipped/rejected turn"); }
    check((game.known_spells & spell_bit(EXCHANGE)) && !studying() && game.turns==80,"oracle failed complete learning");
}
void expansion_agents() {
    auto policy=std::make_unique<ExplorerAgent>(); OmniscientAgent oracle;
    auto setup=[&](uint8_t spell) {
        arena(); game.known_spells=spell_bit(spell); game.mana=16;
        game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
        game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
        for(uint8_t y=6;y<=16;++y) for(uint8_t x=6;x<=16;++x) carve(x,y);
        policy->reset(); oracle.reset();
    };
    for(uint8_t spell : {uint8_t(FIREBURST),uint8_t(CHAIN_LIGHTNING)}) {
        setup(spell); game.monsters[0]={{12,10},GOBLIN,100,15,{0,0},0}; game.monsters[1]={{13,11},ORC,100,15,{0,0},0};
        auto a=policy->choose_action(observe_explorer(0,0));
        check(a.kind==ExplorerActionKind::Cast && a.slot==spell,"explorer failed clustered new attack");
        auto b=oracle.choose_action({game,0,0}); check(b.kind==ActionKind::Cast && b.slot==spell,"oracle failed clustered new attack");
    }
    setup(FIREBURST); game.monsters[0]={{11,10},ORC,100,15,{0,0},0}; game.monsters[1]={{12,11},ORC,100,15,{0,0},0};
    auto a=policy->choose_action(observe_explorer(0,0)); check(a.kind!=ExplorerActionKind::Cast,"explorer unsafe self-burning Fireburst");
    setup(PHASE_STEP); game.hp=1; game.monsters[0]={{11,10},LORD,100,15,{0,0},0}; game.monsters[1]={{10,11},ORC,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==PHASE_STEP,"explorer failed safe controlled escape");
    Game known=game,hidden=game; hidden.features[0]={{10,7},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    identical(known,hidden,"Phase Step policy inspected hidden landing trap");
    game=known; auto b=oracle.choose_action({game,0,0}); check(b.kind==ActionKind::Cast && b.slot==PHASE_STEP,"oracle failed controlled escape");
    setup(SLUMBER); game.hp=1; game.monsters[0]={{11,10},LORD,100,0,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==SLUMBER,"explorer failed emergency Slumber");
    b=oracle.choose_action({game,0,0}); check(b.kind==ActionKind::Cast && b.slot==SLUMBER,"oracle failed emergency Slumber");
    setup(BLOOD_BOLT); game.hp=game.max_hp=200; game.inventory[0]={POTION_HEALING,1}; identify_type(POTION_HEALING);
    game.monsters[0]={{13,10},DRAGON,6,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Cast && a.slot==BLOOD_BOLT,"explorer ignored conservative Blood Bolt opportunity");
    b=oracle.choose_action({game,0,0}); check(b.kind==ActionKind::Cast && b.slot==BLOOD_BOLT,"oracle ignored efficient Blood Bolt kill");
    game.hp=3; policy->reset(); oracle.reset();
    a=policy->choose_action(observe_explorer(0,0)); b=oracle.choose_action({game,0,0});
    check(a.kind!=ExplorerActionKind::Cast && b.kind!=ActionKind::Cast,"agent attempted suicidal Blood Bolt");
    setup(MAGIC_DART); game.known_spells=0; game.inventory[0]={WAND_LIGHTNING,uint8_t(ITEM_IDENTIFIED|3)}; identify_type(WAND_LIGHTNING);
    game.monsters[0]={{12,10},GOBLIN,100,15,{0,0},0}; game.monsters[1]={{13,11},ORC,100,15,{0,0},0};
    a=policy->choose_action(observe_explorer(0,0)); b=oracle.choose_action({game,0,0});
    check(a.kind==ExplorerActionKind::Wand && b.kind==ActionKind::Wand,"agents missed efficient lightning wand chain");
    check(b.goal=="efficient lightning chain","oracle dropped planned chain action");
    for(auto modifier : {WAND_CURSED,WAND_UNRELIABLE}) {
        set_wand_modifier(game.inventory[0],modifier); policy->reset(); oracle.reset();
        a=policy->choose_action(observe_explorer(0,0)); b=oracle.choose_action({game,0,0});
        check(a.kind!=ExplorerActionKind::Wand && b.kind!=ActionKind::Wand,"agent used known afflicted lightning wand");
    }
    for(uint8_t spell=FIREBURST;spell<SPELL_COUNT;++spell) {
        setup(MAGIC_DART); game.inventory[0]={SPELLBOOK,spell};
        a=policy->choose_action(observe_explorer(0,0)); check(a.kind==ExplorerActionKind::Use && a.slot==0,"explorer failed new book safe study");
        b=oracle.choose_action({game,0,0}); check(b.kind==ActionKind::Use && b.slot==0,"oracle failed new book safe study");
        game.ground[0]={game.player,{SPELLBOOK,spell}};
        b=oracle.choose_pickup({game,0,0}); check(b.kind!=ActionKind::Take,"oracle collected a carried duplicate spellbook");
    }
}
int main() {
    try { expansion_agents(); magic_observations(); prolonged_visible_combat(); consumable_observations(); potion_capacity_boundary(); noninterference(); perception_and_memory(); identification_and_mapping(); navigation_and_tactics(); stationary_contact_detour(); invisible_contact_checks(); hazard_navigation(); controlled_roundtrip(); replay(); parameters_and_messages(); competence();
        std::cout<<checks<<" explorer checks passed; policy bytes="<<sizeof(ExplorerAgent)<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<"explorer check failed: "<<e.what()<<'\n'; return 1; }
}
