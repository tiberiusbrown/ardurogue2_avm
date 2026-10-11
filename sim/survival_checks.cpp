#include "metrics.hpp"
#include "simulator.hpp"
#include "explorer_observation.hpp"
#include "explorer_events.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace rogue;
using namespace sim;
namespace {
void check(bool b,const char* s) { if(!b) throw std::runtime_error(s); }
void arena() {
    game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=321;
    game.player={10,10}; game.up={10,10}; game.down={50,20}; game.hp=game.max_hp=80;
    game.level=5; game.speed=4; game.strength=7; game.dexterity=6; game.hunger=220;
    game.weapon_slot=game.armor_slot=game.offhand_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
    std::memset(game.walls,255,sizeof game.walls);
    for(int y=8;y<=12;++y) for(int x=8;x<=12;++x) carve(uint8_t(x),uint8_t(y));
    game.monsters[0]={{11,10},PHANTOM,16,0,{0,0},MON_AGGRO};
}
void segmentation() {
    arena(); auto c=std::make_unique<Collector>(); c->data.agent="explorer-v1"; c->enter_floor();
    auto before=game;
    c->data.actions=1;
    c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    check(c->data.encounters.size()==1 && c->data.encounters[0].first_phantom && !c->data.encounters[0].visible,
          "invisible contact duplicated or misclassified encounter");
    check(!std::memcmp(&game,&before,sizeof game),"diagnostics changed Game/RNG");
    game.monsters[0].pos={20,20}; c->data.turns=4; c->update_encounters();
    check(c->current_encounter==0,"temporary disappearance split encounter");
    game.monsters[0].pos={11,10}; c->data.turns=5;
    c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    game.monsters[1]={{10,11},ORC,12,0,{0,0},MON_AGGRO};
    c->encounter_event(EventKind::MonsterAttack,1,ORC,1);
    c->movement_action=true; c->action_origin={10,10};
    c->encounter_event(EventKind::Turn,255,0,1);
    check(c->data.encounters[0].movement_turns==0,"stationary melee counted as movement");
    game.player={9,10}; c->encounter_event(EventKind::Turn,255,0,1); game.player={10,10};
    check(c->data.encounters[0].movement_turns==1,"actual encounter movement was omitted");
    c->encounter_event(EventKind::PlayerDamage,255,0,7); game.hp=73;
    c->encounter_event(EventKind::Consumed,255,POTION_HEALING,1);
    c->encounter_event(EventKind::MonsterDamage,0,PHANTOM,1,1);
    c->encounter_event(EventKind::MonsterKilled,0,PHANTOM,1);
    check(c->current_encounter==0,"one death prematurely closed mixed encounter");
    c->data.turns=6; c->encounter_event(EventKind::MonsterDamage,1,ORC,1,0);
    c->encounter_event(EventKind::MonsterKilled,1,ORC,1);
    auto& e=c->data.encounters[0];
    check(e.actions==1,"encounter omitted its first submitted action");
    check(e.player_kills==1 && e.phantom_player_kills==1 &&
          e.species_mask==((uint32_t(1)<<PHANTOM)|(uint32_t(1)<<ORC)),"player/environmental kill or species attribution failed");
    check(e.mixed && e.participants==2 && e.phantom_kills==1 && e.damage==7 && e.healing==1 && e.survived,"mixed outcome/resource attribution failed");
    c->data.turns=31; c->update_encounters(); check(e.followup[0]==1 && e.followup[1]==-1,"follow-up horizon incorrectly measured");
    game.floor=1; c->update_encounters(); check(e.next_floor,"next descent floor missing");
    c->data.turns=40; c->data.result="death"; game.hp=0; c->update_encounters(true);
    check(e.followup[0]==1 && e.followup[1]==0 && e.followup[2]==0,"post-encounter death replaced earlier survival");
    arena(); c=std::make_unique<Collector>(); c->enter_floor();
    c->encounter_event(EventKind::FeatureActivated,0,FEATURE_SUMMONING_TRAP,1);
    c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    check(c->data.encounters[0].summoning && !c->data.encounters[0].known_trap,"first discovery confused with known hazard");
    c->known_features[game.player.y*64+game.player.x]=FEATURE_SUMMONING_TRAP;
    c->encounter_event(EventKind::FeatureActivated,0,FEATURE_SUMMONING_TRAP,1);
    check(c->data.encounters[0].known_trap,"known hazard entered during an encounter was omitted");
    game.monsters[0].pos={30,20}; c->data.turns=6; c->update_encounters();
    check(c->current_encounter<0 && c->data.encounters[0].outcome=="disengaged","teleport/disengagement did not close after quiet interval");
    c->data.turns=7; c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    check(c->data.encounters.size()==2 && !c->data.encounters[1].first_phantom,"reengagement manufactured a second first contact");
    c->data.stuck=true; c->data.result="SIM_STUCK"; c->update_encounters(true);
    check(c->data.encounters[1].outcome=="censored" && c->data.encounters[1].followup[0]==-1,"failure classified as death or follow-up survival");
    arena(); c=std::make_unique<Collector>(); c->enabled=false; c->encounter_event(EventKind::MonsterAttack,0,PHANTOM,1);
    check(c->data.encounters.empty() && !c->first_phantom_seen,"disabled telemetry still collects encounters");
}
void equal_start(std::ostream* out) {
    if(out) write_encounters_header(*out);
    for(unsigned seed=1;seed<=32;++seed) {
        RunMetrics results[2]; Game initial;
        arena(); game.run_seed=game.random_state=uint16_t(seed); game.hp=uint8_t(20+seed);
        game.inventory[0]=make_equipment(LONG_SWORD,0); game.weapon_slot=0;
        game.inventory[1]=make_equipment(CHAIN_MAIL,0); game.armor_slot=1; initial=game;
        for(unsigned policy=0;policy<2;++policy) {
            game=initial; session={NONE,DEATH,false};
            ExplorerParameters prefs; prefs.hazard_routing=int(policy);
            auto agent=std::make_unique<ExplorerAgent>(prefs); auto c=std::make_unique<Collector>();
            c->data.seed=c->data.effective_seed=uint16_t(seed); c->data.agent="explorer-v1";
            CollectScope scope(*c); c->enter_floor(); ExplorerObserver observer;
            ExplorerMessages messages; ExplorerMessageScope message_scope(&messages);
            for(unsigned turn=0;turn<256 && game.hp && game.monsters[0].type;++turn) {
                auto o=observer.observe(c->data.actions,c->data.turns); c->observe_exploration(o);
                auto e=agent->choose_action(o); Action a;
                a.kind=e.kind==ExplorerActionKind::Move ? ActionKind::Move : e.kind==ExplorerActionKind::Use ? ActionKind::Use : ActionKind::Wait;
                a.dx=e.dx; a.dy=e.dy; a.slot=e.slot; a.target=e.target; a.goal=e.goal; a.destination={e.destination.x,e.destination.y};
                c->prepare_action(a); messages.prepare(e,{game.player.x,game.player.y},false); ++c->data.actions;
                dispatch(a); c->observe();
            }
            c->data.result=game.hp ? "abandoned" : "death"; c->update_encounters(true);
            check(!game.monsters[0].type || !game.hp,"equal-start encounter did not resolve within bound");
            results[policy]=c->data;
            if(out) write_encounters(*out,c->data);
        }
        check(results[0].encounters.size()==results[1].encounters.size(),"hazard routing changed no-hazard tactical segmentation");
        std::ostringstream a,b; write_encounters(a,results[0]); write_encounters(b,results[1]);
        check(a.str()==b.str(),"equal-start no-hazard phantom outcomes differ");
    }
}
}
int main(int argc,char** argv) {
    try {
        segmentation(); std::ofstream file; if(argc>1) file.open(argv[1]);
        equal_start(argc>1 ? &file : nullptr);
        std::cout<<"survival segmentation, censoring and 32 paired equal-start phantom encounters passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
