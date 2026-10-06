#include "simulator.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace rogue;
using namespace sim;
void check_v2_policy();
namespace {
void check(bool c,const char* message) { if(!c) throw std::runtime_error(message); }
std::string all_metrics(const RunMetrics& r) {
    std::ostringstream o; for(const auto& stream:csv_streams) stream.rows(o,r); return o.str();
}
void arena() {
    game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=123;
    game.hp=game.max_hp=100; game.level=1; game.strength=5; game.dexterity=4; game.speed=4;
    game.hunger=220; game.player={4,4}; game.up={1,1}; game.down={8,8};
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
}
void determinism() {
    OmniscientAgent a;
    std::ostringstream first,second,without;
    constexpr uint16_t escape_seed = 1;
    Options o; o.trace=&first; auto r1=run(escape_seed,a,o); Game final=game;
    o.trace=&second; auto r2=run(escape_seed,a,o);
    check(all_metrics(r1)==all_metrics(r2) && first.str()==second.str(),"same seed changed metrics/trace");
    check(std::memcmp(&final,&game,sizeof game)==0,"same seed changed final state");
    o.trace=nullptr; auto quiet=run(escape_seed,a,o);
    check(all_metrics(r1)==all_metrics(quiet),"tracing perturbed the result or telemetry");
    o.telemetry=false; o.trace=&without; auto disabled=run(escape_seed,a,o);
    check(first.str()==without.str(),"telemetry collection changed action/event trace");
    check(r1.actions==disabled.actions && r1.turns==disabled.turns && r1.score==disabled.score &&
        r1.result==disabled.result && r1.action_hash==disabled.action_hash &&
        std::memcmp(&final,&game,sizeof game)==0,"telemetry collection changed gameplay/RNG");
    check(r1.result=="escaped" && r1.has_yendor && r1.final_floor==0,"escape regression seed 1 failed");
    check(r1.monsters[LORD].killed==1 && r1.items[YENDOR_AMULET].picked_up==1,"escape skipped Lord/Yendor");
    check(r1.floors.size()==31 && r1.floors_exited==31,"round trip lacks complete floor visits");
    uint64_t actions=0,turns=0;
    for(const auto& f:r1.floors) { actions+=f.actions; turns+=f.turns; }
    check(actions==r1.actions && turns==r1.turns,"floor accounting lost stair actions/turns");
    for(int i=0;i<16;++i) check(r1.floors[i].floor==i && !r1.floors[i].ascent,"descent visits merged");
    for(int i=0;i<15;++i) check(r1.floors[16+i].floor==14-i && r1.floors[16+i].ascent,"ascent visits merged");
}
void policy_regressions() {
    arena(); OmniscientAgent a;
    game.inventory[0]={RING_STRENGTH,1}; game.inventory[1]={RING_DEXTERITY,1};
    game.inventory[2]={RING_PROTECTION,1}; game.ring_slots[0]=0; game.ring_slots[1]=1;
    Game before=game;
    auto action=a.choose_action({game,0,0});
    check(std::memcmp(&before,&game,sizeof game)==0,"agent advanced RNG/mutated state");
    check(action.kind==ActionKind::Use && action.slot==0,"agent did not remove weakest ring");
    check(dispatch(action),"ring removal rejected");
    action=a.choose_action({game,1,1});
    check(action.kind==ActionKind::Use && action.slot==2,"agent re-equipped weaker ring");
    check(dispatch(action),"ring replacement rejected");
    arena(); game.inventory[0]={FOOD,8};
    for(int i=1;i<INVENTORY;++i) game.inventory[i]=make_equipment(PLATE_MAIL,0);
    game.armor_slot=1; game.ground[0]={game.player,{SCROLL_TELEPORT,1}};
    action=a.choose_action({game,0,0});
    check(!(action.kind==ActionKind::Take && action.slot==0),"stocked food discarded using acquisition cap");
    arena(); for(int i=0;i<INVENTORY;++i) game.inventory[i]={SCROLL_IDENTIFY,1};
    game.ground[0]={game.player,{HEALING,1}};
    action=a.choose_action({game,0,0});
    check(action.kind==ActionKind::Take && action.slot==0,"full inventory lacks deterministic swap");
    check(dispatch(action) && game.inventory[0].type==HEALING && game.ground[0].item.type==SCROLL_IDENTIFY,
        "full inventory did not use production ground swap");
    arena(); game.inventory[0]=make_equipment(PLATE_MAIL,0); game.inventory[0].info|=ITEM_CURSED; game.armor_slot=0;
    game.ground[0]={game.player,{HEALING,1}};
    Action swap; swap.kind=ActionKind::Swap; swap.target=0; swap.slot=0;
    check(!dispatch(swap),"dispatcher removed cursed equipment");
}
void path_and_dispatch() {
    arena(); std::memset(game.walls,0xff,sizeof game.walls);
    for(int x=4;x<=8;++x) carve(static_cast<uint8_t>(x),4);
    game.doors[0]={{5,4}}; game.door_count=1;
    game.monsters[0]={{6,4},SNAKE,3,0,{0,0},0};
    Paths safe, combat(true);
    check(safe.to({6,4})==2 && safe.to({8,4})==-1 && combat.to({8,4})==4,"BFS monster occupancy wrong");
    auto move=combat.move_to({8,4},"test");
    check(dispatch(move) && door_open(0) && game.player==Position{4,4},"closed door bypassed production movement");
    game.ground[0]={{8,4},{HEALING,1}};
    Action take; take.kind=ActionKind::Take; take.target=0;
    check(!dispatch(take) && game.ground[0].item.type==HEALING,"remote item pickup allowed");
    game.paralyzed=2; Action wait; check(dispatch(wait) && game.paralyzed==1,"wait did not advance production statuses");
    Action invalid; invalid.kind=ActionKind::Move; invalid.dx=invalid.dy=1;
    check(!dispatch(invalid),"non-cardinal move accepted");
}
void hooks() {
    arena(); game.monsters[0]={{5,4},SNAKE,3,0,{0,0},0};
    Collector c; CollectScope sink(c); c.enter_floor(); c.observe();
    damage_monster(0,255,true);
    check(c.data.monsters[SNAKE].damage_taken==3 && c.data.monsters[SNAKE].killed==1,
        "damage telemetry includes overkill or misses a kill");
    check(c.data.floors[0].damage_dealt==3,"floor damage mismatch");
    game.monsters[1]={{5,4},LORD,1,0,{0,0},0}; damage_monster(1,1,true);
    check(c.data.items[YENDOR_AMULET].generated==1 && game.ground[15].item.type==YENDOR_AMULET,
        "Lord drop generation hook missing");
    game.player=game.ground[15].pos; c.observe();
    Action take; take.kind=ActionKind::Take; take.target=15; check(dispatch(take),"Yendor pickup failed");
    check(game.has_amulet && c.data.items[YENDOR_AMULET].picked_up==1 &&
        c.data.items[YENDOR_AMULET].reached==1,"Yendor pickup/reach telemetry missing");
    game.inventory[0]={FOOD,2}; Action eat; eat.kind=ActionKind::Use; eat.slot=0; check(dispatch(eat),"food failed");
    check(c.data.items[FOOD].used==1 && c.data.items[FOOD].consumed==1,"food use/consumption wrong");
    game.hp=2;
    { DamageScope damage(Cause::Monster,DRAGON); hurt_player(250); }
    check(c.data.death_cause=="DRAGON" && c.data.monsters[DRAGON].deaths==1 &&
        c.data.monsters[DRAGON].player_damage==2,"death attribution/actual damage wrong");
}
void wand_identity() {
    arena();
    game.ground[0]={game.player,{WAND_TELEPORT,5}};
    game.ground[1]={game.player,{WAND_FORCE,5}};
    Collector c; CollectScope sink(c); c.enter_floor();
    auto perform = [&](Action a) { c.prepare_action(a); check(dispatch(a),"wand identity fixture rejected"); };
    Action take; take.kind=ActionKind::Take; take.target=0; perform(take);
    Action activate; activate.kind=ActionKind::Wand; activate.slot=0; activate.dx=1; perform(activate);
    Action drop; drop.kind=ActionKind::Drop; drop.slot=0; perform(drop);
    perform(take); perform(activate);
    Action swap; swap.kind=ActionKind::Swap; swap.slot=0; swap.target=1;
    perform(swap); perform(activate); perform(swap); perform(activate);
    const auto& t=c.data.items[WAND_TELEPORT]; const auto& f=c.data.items[WAND_FORCE];
    check(t.generated==1 && t.picked_up==3 && t.wands_picked_up==1 && t.wands_activated==1 && t.charges_used==3,
        "teleport drop/repick/swap counted multiple physical wands");
    check(f.generated==1 && f.wands_picked_up==1 && f.wands_activated==1 && f.charges_used==1,
        "force swap lost physical wand identity");
    // Empty inventory slots and fresh floor ground slots must get fresh IDs.
    game.inventory[0]={}; game.ground[0]={game.player,{WAND_TELEPORT,1}};
    c.enter_floor(); perform(take); perform(activate);
    check(t.wands_picked_up==2 && t.wands_activated==2,"slot reuse conflated a fresh wand");
}
class InvalidAgent final:public Agent {
public:
    const char* name() const override { return "test-invalid"; }
    Action choose_action(const DecisionContext&) override { Action a; a.kind=ActionKind::Use; return a; }
};
class LostAgent final:public Agent {
public:
    const char* name() const override { return "test-lost"; }
    Action choose_action(const DecisionContext&) override { Action a; a.goal="unable to find path"; a.diagnostic=Diagnostic::NoPath; return a; }
};
class MutatingAgent final:public Agent {
public:
    const char* name() const override { return "test-mutating"; }
    Action choose_action(const DecisionContext&) override { ++game.random_state; return {}; }
};
class SwappingAgent final:public Agent {
public:
    const char* name() const override { return "test-swapping"; }
    Action choose_action(const DecisionContext&) override { Action a; a.kind=ActionKind::Swap; a.target=a.slot=0; return a; }
};
class ThrowingAgent final:public Agent {
public:
    const char* name() const override { return "test-throwing"; }
    Action choose_action(const DecisionContext&) override { throw std::runtime_error("policy failure"); }
};
void safety() {
    OmniscientAgent a; Options o; o.max_actions=1;
    auto r=run(3,a,o);
    check(r.result=="SIM_STUCK" && r.stuck && r.reason=="excessive action count" && !session.ended,
        "action budget counted as game loss");
    InvalidAgent invalid; o={}; o.max_rejected=3;
    r=run(3,invalid,o); check(r.result=="SIM_STUCK" && r.reason=="action rejected repeatedly", "rejection detection failed");
    LostAgent lost; o={}; o.max_path_failures=3;
    r=run(3,lost,o); check(r.result=="SIM_STUCK" && r.reason=="unable to find path","path failure not diagnosed");
    MutatingAgent bad; r=run(3,bad);
    check(r.result=="SIM_ERROR" && r.stuck && r.actions==0,"agent RNG mutation not detected");
    ThrowingAgent throwing; r=run(3,throwing);
    check(r.result=="SIM_ERROR" && r.reason=="agent error: policy failure","agent error did not remain a per-run failure");
    arena(); game.inventory[0]={FOOD,8}; game.ground[0]={game.player,{SCROLL_TELEPORT,1}};
    SwappingAgent swapping; r=run_started(123,swapping);
    check(r.result=="SIM_STUCK" && r.reason.find("inventory-policy failure")==0,"swap cycle not diagnosed");
    arena(); game.hp=1; game.hunger=0; game.turns=3;
    r=run_started(123,lost);
    check(r.result=="death" && r.death_cause=="starvation" && !r.stuck,"starvation classified as simulator failure");
}
void competence() {
    OmniscientAgent a; int leaves=0,deep=0,wins=0; uint64_t kills=0,food=0,healing=0,equipment=0;
    for(uint16_t seed=1;seed<=32;++seed) {
        auto r=run(seed,a); check(!r.stuck,"fixed competence set stuck");
        leaves+=r.deepest>0; deep+=r.deepest>=12; wins+=r.result=="escaped";
        food+=r.items[FOOD].used; healing+=r.items[HEALING].drunk;
        for(const auto& m:r.monsters) kills+=m.killed;
        for(const auto& i:r.items) equipment+=i.equipped;
    }
    check(leaves>0 && deep>0 && wins>0 && kills && food && healing && equipment,"agent competence regression");
    std::cout << "fixed seeds 1..32: left floor 0=" << leaves << " deep=" << deep << " escapes=" << wins << '\n';
}
class RngExperiment final:public Experiment {
public:
    void apply(Game& g,ExperimentContext,std::vector<Intervention>&) const override { ++g.random_state; }
};
void experiments() {
    OmniscientAgent agent; Options o;
    auto ordinary=run(4,agent,o); auto final=game;
    o.experiment=std::make_shared<RuleExperiment>();
    auto noop=run(4,agent,o);
    check(all_metrics(ordinary)==all_metrics(noop) && std::memcmp(&final,&game,sizeof game)==0,"no-op experiment changed normal run");
    o.experiment=std::make_shared<RngExperiment>(); bool rejected=false;
    try { run(4,agent,o); } catch(const std::runtime_error&) { rejected=true; }
    check(rejected,"experiment gameplay RNG mutation not rejected");
    arena(); game.floor=2;
    game.ground[0]={{3,6},{HEALING,4}}; game.ground[1]={{8,7},{HEALING,3}};
    game.ground[2]={{9,7},make_equipment(DAGGER,-1)};
    game.monsters[0]={{4,6},SNAKE,1,6,{255,255},MON_AGGRO};
    auto exp=std::make_shared<RuleExperiment>();
    exp->rules={parse_rule("replace-item:HEALING:FOOD:floor=2:direction=descent:max=1"),
        parse_rule("replace-item:DAGGER:SPEAR:info=preserve"),parse_rule("replace-monster:SNAKE:MIMIC")};
    Collector c; c.experiment=exp; CollectScope scope(c); auto rng=game.random_state;
    after_floor_generation(); c.enter_floor(); c.close_floor(false);
    check(game.random_state==rng,"replacement consumed gameplay RNG");
    check(game.ground[0].pos==Position{3,6} && game.ground[0].item.type==FOOD && game.ground[0].item.info==1,
        "replacement failed position/default info invariant");
    check(game.ground[1].item.type==HEALING && game.ground[2].item.info==make_equipment(DAGGER,-1).info,
        "replacement cap or compatible encoding failed");
    check(game.monsters[0].pos==Position{4,6} && game.monsters[0].hp==monster_health(MIMIC) &&
        game.monsters[0].stun==0 && game.monsters[0].effects[0]==0 && game.monsters[0].effects[1]==0 &&
        mimic_appearance(game.monsters[0])==MIMIC_SCROLL,"monster initialization invariant failed");
    check(c.data.items[FOOD].generated==1 && c.data.items[HEALING].generated==3 &&
        c.data.monsters[MIMIC].generated==1 && c.data.monsters[SNAKE].generated==0,"intervention occurred after generation scan");
    check(c.data.interventions.size()==3 && c.data.interventions[0].count==1 && c.data.interventions[0].visit==1,
        "intervention ledger does not reconcile");
    exp->rules={parse_rule("remove-item:HEALING"),parse_rule("remove-monster:MIMIC")};
    exp->apply(game,{123,2},c.data.interventions);
    check(!game.ground[1].item.type && !game.monsters[0].type,"removal failed");
    bool incompatible=false;
    try { parse_rule("replace-item:DAGGER:FOOD:info=preserve"); } catch(const std::runtime_error&) { incompatible=true; }
    check(incompatible,"incompatible instance info preservation accepted");
}
}
int main() {
    try {
        static_assert(sizeof(Game)==774 && SAVE_VERSION==23,"native saved layout changed");
        check_v2_policy(); determinism(); policy_regressions(); path_and_dispatch(); hooks(); wand_identity(); safety(); competence(); experiments();
        std::cout << "simulator checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
