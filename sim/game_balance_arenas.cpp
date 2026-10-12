#include "simulator.hpp"
#include "explorer_observation.hpp"
#include "explorer_events.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <cstring>

using namespace rogue;
using namespace sim;
// Equal starting RNG, HP, equipment, geometry and agent preferences. Only the
// temporary sight state differs in free arms. The paid arm additionally carries
// an identified dose and spends a normal production drinking action.
int main(int argc,char** argv) {
    try {
        if(argc<2) throw std::runtime_error("usage: game_balance_arenas OUTPUT [SEEDS] [RULE...]");
        unsigned seeds=argc>2 ? unsigned(std::stoul(argv[2])) : 128;
        auto experiment=std::make_shared<RuleExperiment>();
        for(int n=3;n<argc;++n) experiment->rules.push_back(parse_rule(argv[n]));
        std::ofstream out(argv[1]);
        if(!out) throw std::runtime_error("cannot open arena output");
        out<<"seed,gear,phantoms,arm,resolved,survived,actions,turns,damage,exit_hp,kills,drunk,expired,attack_events,sight_attack_events\n";
        
        for(unsigned seed=1;seed<=seeds;++seed) for(unsigned gear=0;gear<3;++gear) for(unsigned count:{1u,3u}) {
            game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=uint16_t(seed);
            game.player={10,10}; game.up={50,19}; game.down={50,20}; game.floor=4;
            game.level=uint8_t(4+gear*4); game.hp=game.max_hp=uint8_t(24+gear*12);
            game.hunger=240; game.speed=4; game.strength=uint8_t(5+gear); game.dexterity=uint8_t(4+gear);
            game.weapon_slot=0; game.armor_slot=1;
            game.offhand_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
            game.inventory[0]=make_equipment(gear==0 ? DAGGER : gear==1 ? LONG_SWORD : TWO_HANDED_SWORD,0);
            game.inventory[1]=make_equipment(gear==0 ? LEATHER_ARMOR : gear==1 ? CHAIN_MAIL : PLATE_MAIL,0);
            std::memset(game.walls,255,sizeof game.walls);
            for(int y=8;y<=12;++y) for(int x=8;x<=14;++x) carve(uint8_t(x),uint8_t(y));
            Game initial=game; 
            for(int duration:{0,20,40,80,-1}) {
                game=initial; session={NONE,DEATH,false};
                auto agent=std::make_unique<ExplorerAgent>(); auto c=std::make_unique<Collector>();
                c->experiment=experiment; c->data.seed=c->data.effective_seed=uint16_t(seed); c->data.agent="explorer-v1";
                CollectScope scope(*c); 
                for(unsigned n=0;n<count;++n) game.monsters[n]={{uint8_t(11+n),10},PHANTOM,monster_health(PHANTOM),0,{0,0},MON_AGGRO};
                if(duration>=0) game.see_invisible=uint8_t(duration);
                else { game.inventory[2]={POTION_SEE_INVISIBLE,uint8_t(ITEM_IDENTIFIED|1)}; identify_type(POTION_SEE_INVISIBLE); }
                 c->enter_floor();  ExplorerObserver observer;
                ExplorerMessages messages; ExplorerMessageScope message_scope(&messages);
                bool live=true;
                for(unsigned action=0;action<512 && game.hp && live;++action) {
                    auto o=observer.observe(c->data.actions,c->data.turns); c->observe_exploration(o);
                     auto e=agent->choose_action(o);  Action a;
                    a.kind=e.kind==ExplorerActionKind::Move ? ActionKind::Move : e.kind==ExplorerActionKind::Use ? ActionKind::Use : ActionKind::Wait;
                    a.dx=e.dx; a.dy=e.dy; a.slot=e.slot; a.target=e.target; a.goal=e.goal; a.destination={e.destination.x,e.destination.y};
                    c->prepare_action(a); messages.prepare(e,{game.player.x,game.player.y},false); ++c->data.actions;
                     dispatch(a);  c->observe(); live=false;
                    for(const auto& m:game.monsters) live=live || bool(m.type);
                }
                c->data.result=game.hp ? "abandoned" : "death"; c->update_encounters(true);
                uint64_t events=0,sight=0;
                for(const auto& e:c->data.encounters) { events+=e.phantom_attack_turns; sight+=e.phantom_sight_attack_turns; }
                auto& item=c->data.items[POTION_SEE_INVISIBLE]; auto& m=c->data.monsters[PHANTOM];
                out<<seed<<','<<gear<<','<<count<<','<<duration<<','<<(!live || !game.hp)<<','<<bool(game.hp)
                   <<','<<c->data.actions<<','<<c->data.turns<<','<<m.player_damage<<','<<unsigned(game.hp)
                   <<','<<m.killed<<','<<item.drunk<<','<<item.expired<<','<<events<<','<<sight<<'\n';
            }
        }
        if(!out) throw std::runtime_error("arena output write failed");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
