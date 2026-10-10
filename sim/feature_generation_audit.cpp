// Host-only generation census; no policy, gameplay RNG or saved state changes.
#include "metrics.hpp"
#include "world_gen.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <iostream>
#include <map>
#include <tuple>
#include <cstdlib>
int main(int argc, char** argv) {
    using namespace rogue; using namespace sim;
    unsigned seeds=argc>1 ? unsigned(std::strtoul(argv[1],nullptr,10)) : 10000;
    if(!seeds || seeds>65535) return 1;
    using Key=std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned,bool>;
    std::map<Key,std::pair<unsigned,unsigned>> counts;
    Collector collector; collector.enabled=false;
    auto experiment=std::make_shared<RuleExperiment>(); collector.experiment=experiment;
    CollectScope scope(collector);
    for(unsigned seed=1;seed<=seeds;++seed) for(uint8_t depth=0;depth<FLOORS;++depth) for(bool old : {false,true}) {
        game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=uint16_t(seed); game.floor=depth;
        experiment->rules={parse_rule("original-early-traps")};
        if(old) experiment->rules.push_back(parse_rule("original-holy-generation"));
        collector.data={}; // No visit history is needed in the generation audit.
        make_floor();
        unsigned unholy=0,eligible=0,preferred=0; bool holy=false;
        for(auto m:game.monsters) unholy+=undead_or_demonic(m.type);
        for(auto f:game.features) holy|=f.id==FEATURE_HOLY_SYMBOL;
        for(uint8_t y=0;y<MAP_H;++y) for(uint8_t x=0;x<MAP_W;++x) {
            Position p{x,y};
            if(wall_at(x,y) || p==game.player || p==game.up || p==game.down || door_at(p)!=NONE || monster_at(p)!=NONE || item_at(p)!=NONE) continue;
            ++eligible; preferred+=bool(generation::geometry(p)&(generation::OPEN|generation::ROOMY));
        }
        // Geometry bins retain sparse small-floor strata with explicit denominators.
        auto key=Key{depth,unsigned(generation::archetype(generation::floor_seed(generation::LAYOUT))),unholy,eligible/128,preferred/128,old};
        auto& count=counts[key]; ++count.first; count.second+=holy;
    }
    std::cout<<"floor,archetype,unholy_count,eligible_bin128,preferred_bin128,original,generated_floors,holy_floors\n";
    for(auto [key,value]:counts) {
        auto [floor,archetype,unholy,eligible,preferred,old]=key;
        std::cout<<floor<<','<<archetype<<','<<unholy<<','<<eligible<<','<<preferred<<','<<old<<','<<value.first<<','<<value.second<<'\n';
    }
}
