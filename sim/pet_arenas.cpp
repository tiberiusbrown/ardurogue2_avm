#include "companions.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "metrics.hpp"
#include "status.hpp"
#include "world.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstring>

using namespace rogue;
using namespace sim;

// Conditional combat stress tests. Acquisition and MP are deliberately free;
// four Wisps/Golems are infeasible counterfactuals, not production acquisition.
int main(int argc,char** argv) {
    try {
        if(argc<2) throw std::runtime_error("usage: pet_arenas OUTPUT [SEEDS]");
        unsigned seeds=argc>2 ? unsigned(std::stoul(argv[2])) : 512;
        std::ofstream out(argv[1]);
        if(!out) throw std::runtime_error("cannot open pet arena output");
        out<<"seed,party,enemy,hostiles,resolved,player_alive,turns,enemies_killed,pets_alive,player_hp,pet_damage,pet_damage_absorbed,pet_deaths\n";
        const char* parties[]={"none","hound","mixed","trolls","wisps_infeasible","golems_infeasible","golem","wisp","spider"};
        const Position enemies[]={{22,15},{23,15},{22,14},{22,16},{23,14},{23,16},{24,15},{24,14},{24,16},{22,13},{22,17},{24,13}};
        const Position friends[]={{21,15},{20,14},{20,16},{19,15}};
        for(unsigned seed=1;seed<=seeds;++seed) for(uint8_t party=0;party<sizeof parties/sizeof *parties;++party)
        for(uint8_t species:{uint8_t(PHANTOM),uint8_t(DRAGON),uint8_t(LORD)}) for(uint8_t count:{uint8_t(1),uint8_t(4),uint8_t(12)}) {
            if(species==LORD && count!=1) continue;
            game={}; session={NONE,DEATH,false};
            game.player={20,15}; game.up={2,2}; game.down={40,25};
            game.floor=species==PHANTOM ? 4 : species==LORD ? 15 : 12;
            game.run_seed=game.random_state=uint16_t(seed); game.level=20;
            game.hp=game.max_hp=80; game.strength=game.dexterity=6; game.speed=4; game.hunger=255;
            game.weapon_slot=0; game.armor_slot=1; game.offhand_slot=2; game.amulet_slot=3;
            game.ring_slots[0]=game.ring_slots[1]=NONE;
            game.inventory[0]=make_equipment(LONG_SWORD,0); game.inventory[1]=make_equipment(PLATE_MAIL,0);
            game.inventory[2]={GLOWING_ORB,0}; game.inventory[3]={AMULET_BRILLIANCE,1};
            std::memset(game.walls,255,sizeof game.walls);
            for(uint8_t y=10;y<=20;++y) for(uint8_t x=15;x<=28;++x) carve(x,y);
            for(uint8_t h=0;h<count;++h) game.monsters[h]={enemies[h],species,monster_health(species),0,{0,0},MON_AGGRO};
            uint8_t members=party==0 ? 0 : party==1 || party>=6 ? 1 : 4;
            for(uint8_t p=0;p<members;++p) {
                uint8_t type=party<=1 ? HOUND : party==2 ? uint8_t(HOUND+p) : party==3 ? TROLL : party==4 || party==7 ? WISP : party==8 ? SPIDER : GOLEM;
                game.monsters[HOSTILE_MONSTERS+p]={friends[p],type,monster_health(type),0,{0,0},uint8_t(type>=HOUND ? MON_SUMMONED : 0)};
            }
            auto c=std::make_unique<Collector>(); c->data.agent="pet-arena"; c->data.seed=uint16_t(seed);
            CollectScope scope(*c); c->enter_floor();
            uint8_t live=count;
            unsigned turns=0;
            while(turns<128 && game.hp && live) {
                reset_status_position(); end_turn(); ++turns;
                live=0; for(uint8_t h=0;h<HOSTILE_MONSTERS;++h) live+=game.monsters[h].type!=0;
            }
            uint64_t damage=0,absorbed=0,deaths=0;
            for(const auto& p:c->data.floors.back().pets) { damage+=p.damage; absorbed+=p.damage_absorbed; deaths+=p.deaths; }
            out<<seed<<','<<parties[party]<<','<<sim::monster_name(species)<<','<<unsigned(count)<<','<<(!live || !game.hp)<<','<<bool(game.hp)
               <<','<<turns<<','<<unsigned(count-live)<<','<<unsigned(pet_count())<<','<<unsigned(game.hp)<<','<<damage<<','<<absorbed<<','<<deaths<<'\n';
        }
        if(!out) throw std::runtime_error("pet arena output write failed");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
