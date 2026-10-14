#include "agent.hpp"
#include "companions.hpp"
#include "explorer_observation.hpp"
#include "game_internal.hpp"
#include "metrics.hpp"
#include "simulator.hpp"
#include "spells.hpp"
#include "world.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace rogue;
using namespace sim;
static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static void arena() {
    game={}; session={NONE,DEATH,false};
    game.player={20,15}; game.up={2,2}; game.down={25,15}; game.hp=game.max_hp=200;
    game.level=30; game.hunger=255; game.speed=1; game.mana=16; game.run_seed=game.random_state=71;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE; game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
    game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
}
static void monster(uint8_t slot,Position p,uint8_t type,uint8_t hp=100) { game.monsters[slot]={p,type,hp,255,{0,0},0}; }
static bool same_action(const ExplorerAction& a,const ExplorerAction& b) { return a.kind==b.kind && a.slot==b.slot && a.target==b.target && a.dx==b.dx && a.dy==b.dy && a.goal==b.goal; }
int main() {
    try {
        ExplorerAgent explorer; OmniscientAgent oracle;
        arena(); game.known_spells=spell_bit(SUMMON_HOUND);
        auto a=explorer.choose_action(observe_explorer(100,100));
        check(a.kind==ExplorerActionKind::Cast && a.slot==SUMMON_HOUND,"explorer does not summon safely");
        auto b=oracle.choose_action({game,100,100});
        check(b.kind==ActionKind::Cast && b.slot==SUMMON_HOUND,"oracle does not summon safely");
        check(cast_spell(a.slot) && pet_count()==1,"agent summon bypasses production");
        for(uint8_t type:{POTION_FRIENDSHIP,WAND_DOMINATION,SCROLL_TAMING}) {
            arena(); monster(0,{22,15},GOBLIN,1); game.inventory[0]={type,uint8_t(ITEM_IDENTIFIED|3)}; identify_type(type);
            explorer.reset(); oracle.reset();
            a=explorer.choose_action(observe_explorer(100,100)); b=oracle.choose_action({game,100,100});
            auto expected=type==SCROLL_TAMING ? ExplorerActionKind::Use : type==WAND_DOMINATION ? ExplorerActionKind::Wand : ExplorerActionKind::Throw;
            auto oracle_expected=type==SCROLL_TAMING ? ActionKind::Use : type==WAND_DOMINATION ? ActionKind::Wand : ActionKind::Throw;
            check(a.kind==expected && a.slot==0 && b.kind==oracle_expected && b.slot==0,"agents ignore taming resource");
            if(type==SCROLL_TAMING) check(a.target==0 && b.target==0,"scroll target handle lost");
            Game injured=game; game.monsters[0].hp=monster_health(GOBLIN); explorer.reset();
            auto healthy=explorer.choose_action(observe_explorer(100,100)); check(same_action(a,healthy),"explorer reads enemy HP for taming");
            game=injured; game.monsters[0].type=LORD; explorer.reset(); oracle.reset();
            a=explorer.choose_action(observe_explorer(100,100)); b=oracle.choose_action({game,100,100});
            check(!(a.kind==expected && a.slot==0) && !(b.kind==oracle_expected && b.slot==0),"agent tames immune species");
        }
        arena();
        const Position cells[]={{17,13},{18,13},{19,13},{20,13},{21,13},{22,13},{23,13},{17,14},
            {18,14},{19,14},{20,14},{21,14},{22,14},{23,14},{17,15},{18,15}};
        for(uint8_t i=0;i<MONSTERS;++i) monster(i,cells[i],i<12 ? GOBLIN : HOUND+i-12);
        auto observation=observe_explorer(0,0);
        check(observation.monster_count==16 && observation.companion_count==4,"observation capacity drops creatures");
        for(uint8_t i=0;i<16;++i) check(observation.monsters[i].friendly==(i>=12),"observed allegiance incorrect");
        game.monsters[12].pos={55,25}; game.monsters[0].hp=1;
        observation=observe_explorer(0,0);
        check(observation.monster_count==15 && observation.companion_count==4 && observation.companions[0].hp==100,"hidden pet health or location boundary");
        for(uint8_t i=0;i<observation.monster_count;++i) check(observation.monsters[i].handle!=12,"hidden pet position exposed");
        arena(); monster(12,{21,15},HOUND,14); monster(0,{22,15},PHANTOM,10); game.monsters[12].stun=0;
        check(observe_explorer(0,0).monster_count==1 && pet_action(12)==16,"hound scent leaks invisible enemy");
        check(!player_can_see_monster(0),"hound grants See Invisible");
        arena(); std::memset(game.walls,255,sizeof game.walls); for(uint8_t x=20;x<=25;++x) carve(x,15);
        monster(12,{21,15},GOBLIN,10); explorer.reset(); oracle.reset();
        Paths route; check(route.to(game.down)==5,"oracle path cannot cross pet");
        a=explorer.choose_action(observe_explorer(0,0));
        check(a.kind==ExplorerActionKind::Move && a.dx==1,"explorer cannot cross corridor pet");
        move_player(a.dx,a.dy); check(game.player==Position{21,15} && game.monsters[12].pos==Position{20,15},"agent movement attacks pet");
        for(uint8_t spell:{MAGIC_DART,PIERCING_BOLT,CHAIN_LIGHTNING,FIREBURST}) {
            arena(); monster(12,{22,15},HOUND,14); monster(0,{23,15},ORC,100); game.known_spells=spell_bit(spell);
            explorer.reset(); oracle.reset(); a=explorer.choose_action(observe_explorer(0,0)); b=oracle.choose_action({game,0,0});
            check(!(a.kind==ExplorerActionKind::Cast && a.dx==1) && !(b.kind==ActionKind::Cast && b.dx==1),"agent casts through companion");
        }
        arena(); monster(12,{21,14},BAT,1); monster(13,{19,14},SNAKE,3); monster(14,{19,15},HOUND,14); monster(15,{19,16},GOLEM,36);
        monster(0,{23,15},GOBLIN,10); game.inventory[0]={SCROLL_TAMING,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(SCROLL_TAMING);
        explorer.reset(); oracle.reset(); a=explorer.choose_action(observe_explorer(300,300)); b=oracle.choose_action({game,300,300});
        check(a.kind==ExplorerActionKind::Dismiss && b.kind==ActionKind::Dismiss && a.target==12 && b.target==12,"agents cannot upgrade weak full party");
        check(dismiss_pet(a.target) && pet_count()==3,"agent dismissal bypasses rules");
        Collector c; CollectScope sink(c); arena(); monster(0,{22,15},SNAKE,1); c.enter_floor();
        check(create_pet(HOUND,PetSource::Spell),"telemetry acquisition fixture");
        for(uint16_t seed=1;seed<100 && game.monsters[0].type;++seed) { game.random_state=seed; creature_attack(12,0); }
        auto& metric=c.data.floors.back().pets[HOUND];
        check(metric.acquired_spell==1 && metric.kills==1 && metric.xp==2 && metric.damage==1 && game.score==11,"pet kill telemetry attribution");
        damage_monster(12,255,true);
        check(metric.deaths==1 && metric.player_deaths==1 && metric.friendly_damage==14,"pet death telemetry");
        damage_monster(12,255,true);
        check(metric.deaths==1 && metric.friendly_damage==14 && !c.data.floors.back().pets[0].damage_absorbed,"dead slot emits duplicate pet damage");
        std::cout<<"pet agents, observations and telemetry passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
