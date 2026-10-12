#include "metrics.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "agent.hpp"
#include <algorithm>
#include <ostream>

namespace sim {
namespace {
struct Supplies { int food=0,healing=0,escape=0; };
Supplies supplies() {
    Supplies s;
    for(auto i:rogue::game.inventory) {
        if(i.type==rogue::FOOD) s.food+=rogue::item_value(i);
        if(!rogue::item_type_identified(i.type)) continue;
        if(i.type==rogue::POTION_HEALING) s.healing+=rogue::item_value(i);
        if(i.type==rogue::SCROLL_TELEPORT || i.type==rogue::POTION_INVISIBILITY || i.type==rogue::SCROLL_FEAR)
            s.escape+=rogue::item_value(i);
    }
    return s;
}
int worn(uint8_t slot) { return slot<rogue::INVENTORY ? rogue::game.inventory[slot].type : 0; }
int enchant(uint8_t slot) { return slot<rogue::INVENTORY ? rogue::equipment_enchant(rogue::game.inventory[slot]) : 0; }
void key(std::ostream& o,const RunMetrics& r) { o<<r.seed<<','<<r.effective_seed<<','<<r.agent; }
}
void Collector::milestone(const std::string& name) {
    if(!enabled || std::any_of(data.milestones.begin(),data.milestones.end(),[&](const auto& m) { return m.name==name; })) return;
    auto& g=rogue::game; auto s=supplies();
    RunMetrics::Milestone m; m.name=name; m.actions=data.actions; m.turns=data.turns;
    m.floor=g.floor+1; m.hp=g.hp; m.max_hp=rogue::player_max_hp(); m.level=g.level;
    m.armor=rogue::player_armor_rating(); m.weapon=worn(g.weapon_slot); m.enchant=enchant(g.weapon_slot);
    m.offhand=worn(g.offhand_slot); m.food=s.food; m.healing=s.healing; m.escape=s.escape;
    data.milestones.push_back(m);
}
void Collector::close_encounter(const char* reason) {
    if(current_encounter<0) return;
    auto& e=data.encounters[size_t(current_encounter)];
    e.end=data.turns; e.exit_hp=rogue::game.hp; e.actions=data.actions-e.actions;
    e.survived=rogue::game.hp!=0; e.outcome=reason;
    current_encounter=-1; encounter_members.fill(false); encounter_species.fill(0);
}
void Collector::update_encounters(bool final) {
    if(!enabled) return;
    if(current_encounter>=0) {
        bool close=false;
        for(unsigned i=0;i<rogue::MONSTERS;++i) if(encounter_members[i] && rogue::game.monsters[i].type) {
            auto p=rogue::game.monsters[i].pos;
            if(std::abs(int(p.x)-rogue::game.player.x)+std::abs(int(p.y)-rogue::game.player.y)<=2) close=true;
        }
        if(close) encounter_evidence=data.turns;
        if(final || data.turns>=encounter_evidence+6)
            close_encounter(final ? (data.stuck ? "censored" : data.result=="death" ? "death" : "run_end") : "disengaged");
    }
    constexpr int horizons[]={25,50,100};
    for(auto& e:data.encounters) if(!e.outcome.empty()) {
        if(rogue::game.floor>e.floor && !rogue::game.has_amulet) e.next_floor=true;
        for(unsigned i=0;i<3;++i) if(e.followup[i]<0) {
            if(!e.survived || data.result=="death") e.followup[i]=data.turns>e.end+horizons[i] ? 1 : 0;
            else if(data.turns>=e.end+horizons[i]) e.followup[i]=1;
            else if(final && data.result=="escaped") e.followup[i]=1;
        }
    }
}
void Collector::encounter_event(EventKind k,uint8_t index,uint8_t type,uint16_t amount,uint8_t detail) {
    if(!enabled) return;
    bool contact=(k==EventKind::PlayerAttack || k==EventKind::MonsterAttack) && index<rogue::MONSTERS;
    if(contact) {
        if(current_encounter<0) {
            RunMetrics::Encounter e; auto& g=rogue::game; auto s=supplies();
            e.id=unsigned(data.encounters.size()+1); e.floor=g.floor; e.visit=unsigned(data.floors.size());
            e.start=e.end=data.turns; e.actions=data.actions ? data.actions-1 : 0; e.hp=g.hp; e.max_hp=rogue::player_max_hp();
            e.level=g.level; e.armor=rogue::player_armor_rating(); e.weapon=worn(g.weapon_slot);
            e.armor_enchant=rogue::player_armor_enchant();
            e.weapon_enchant=enchant(g.weapon_slot); e.offhand=worn(g.offhand_slot); e.light=rogue::player_light_radius();
            e.food=s.food; e.healing_reserve=s.healing; e.escape_reserve=s.escape; e.ascent=g.has_amulet;
            e.visible=rogue::can_see(g.monsters[index].pos) && rogue::player_can_see_monster(index);
            e.intentional=k==EventKind::PlayerAttack && intentional_action;
            e.known_trap=hazard_window>data.turns;
            e.summoning=summoning_window>data.turns;
            data.encounters.push_back(e); current_encounter=int(data.encounters.size()-1);
            encounter_members.fill(false); encounter_species.fill(0);
        }
        auto& e=data.encounters[size_t(current_encounter)];
        if(type<32) e.species_mask|=uint32_t(1)<<type;
        encounter_evidence=data.turns;
        if(!encounter_members[index]) { encounter_members[index]=true; ++e.participants; }
        for(auto t:encounter_species) if(t && t!=type) e.mixed=true;
        encounter_species[index]=type;
        if(type==rogue::PHANTOM && !e.phantom) {
            e.phantom=true; e.phantom_contact=data.turns; e.phantom_hp=rogue::game.hp; e.phantom_max_hp=rogue::player_max_hp();
            e.potion_sight=rogue::game.see_invisible!=0;
            e.first_phantom=!first_phantom_seen; first_phantom_seen=true;
            e.phantom_visible=rogue::can_see(rogue::game.monsters[index].pos) && rogue::player_can_see_monster(index);
            auto& g=rogue::game; auto s=supplies();
            e.phantom_weapon=worn(g.weapon_slot); e.phantom_offhand=worn(g.offhand_slot); e.phantom_light=rogue::player_light_radius();
            e.phantom_level=g.level; e.phantom_armor=rogue::player_armor_rating(); e.phantom_armor_enchant=rogue::player_armor_enchant();
            e.phantom_healing_reserve=s.healing; e.phantom_escape_reserve=s.escape; e.phantom_food=s.food;
            e.phantom_damage_before=e.damage; e.phantom_healing_before=e.healing; e.phantom_escape_before=e.escape;
        }
        if(type==rogue::PHANTOM) {
            ++e.phantom_attack_turns;
            e.phantom_sight_attack_turns+=rogue::game.see_invisible!=0;
        }
        if(type==rogue::LORD) milestone("lord_encountered");
    }
    if(k==EventKind::MonsterKilled && type==rogue::LORD) milestone("lord_defeated");
    if(k==EventKind::Pickup && type==rogue::YENDOR_AMULET) { milestone("yendor"); milestone("ascent_started"); }
    if(k==EventKind::Finished && type==rogue::ESCAPED) milestone("escaped");
    if((k==EventKind::FeatureActivated || k==EventKind::MonsterTrapActivated) && type==rogue::FEATURE_SUMMONING_TRAP)
        summoning_window=data.turns+8;
    if(k==EventKind::FeatureActivated && type==rogue::FEATURE_SUMMONING_TRAP) {
        int at=rogue::game.player.y*64+rogue::game.player.x;
        if(known_features[at]==type && !data.floors.empty()) {
            auto& nav=data.floors.back().exploration;
            ++nav.known_trap_crossings; nav.repeated_known_trap_crossings+=known_trap_entries[at]++!=0;
            nav.navigation.essential_crossings+=essential_action;
            hazard_window=data.turns+8;
            if(current_encounter>=0) data.encounters[size_t(current_encounter)].known_trap=true;
        }
    }
    if(k==EventKind::PlayerDamage && hazard_window>data.turns && !data.floors.empty())
        data.floors.back().exploration.hazard_damage+=amount;
    if(current_encounter<0) return;
    auto& e=data.encounters[size_t(current_encounter)];
    if(k==EventKind::Turn) {
        ++e.combat_turns;
        e.movement_turns+=movement_action &&
            (rogue::game.player.x!=action_origin.x || rogue::game.player.y!=action_origin.y);
    }
    if(k==EventKind::MonsterDamage && index<rogue::MONSTERS) player_damage_this_action[index]=detail!=0;
    if(k==EventKind::PlayerDamage) e.damage+=amount;
    if(k==EventKind::Consumed) {
        if(type==rogue::POTION_HEALING || type==rogue::POTION_EXPERIENCE) e.healing+=amount;
        if(type==rogue::SCROLL_TELEPORT || type==rogue::SCROLL_FEAR || type==rogue::POTION_INVISIBILITY) e.escape+=amount;
        if(type==rogue::FOOD) e.food_used+=amount;
    }
    if((k==EventKind::FeatureActivated || k==EventKind::MonsterTrapActivated) && type==rogue::FEATURE_SUMMONING_TRAP) e.summoning=true;
    if(k==EventKind::MonsterKilled && index<rogue::MONSTERS && encounter_members[index]) {
        ++e.kills; e.phantom_kills+=type==rogue::PHANTOM; encounter_members[index]=false;
        e.player_kills+=player_damage_this_action[index];
        e.phantom_player_kills+=type==rogue::PHANTOM && player_damage_this_action[index];
        if(std::none_of(encounter_members.begin(),encounter_members.end(),[](bool b) { return b; })) close_encounter("killed");
    }
    if(k==EventKind::MonsterChanged && index<rogue::MONSTERS && encounter_members[index]) e.mixed=true;
    if(k==EventKind::FeatureSpawn && index<rogue::MONSTERS) encounter_members[index]=false;
    if(k==EventKind::Finished) close_encounter(type==rogue::DEATH ? "death" : "run_end");
}
void Collector::record_route(const Action& a) {
    if(data.floors.empty() || a.kind!=ActionKind::Move || a.destination.x>=64 || a.destination.y>=32 ||
       a.goal=="melee visible monster" || a.goal=="repeat reported melee contact" || a.goal.find("probe")!=std::string::npos) return;
    auto& e=data.floors.back().exploration;
    SeenPosition target{a.destination.x,a.destination.y};
    if(reported_goal.x!=255 && target!=reported_goal) ++e.navigation.goal_changes;
    reported_goal=target;
    if(std::none_of(known_features.begin(),known_features.end(),[](uint8_t f) { return f==rogue::FEATURE_SUMMONING_TRAP; })) return;
    // Reporting-only geometry; neither distance nor feasibility enters policy.
    auto route=[&](bool safe) {
        std::array<int16_t,EXPLORER_CELLS> dist; dist.fill(-1);
        std::array<int16_t,EXPLORER_CELLS> queue; unsigned read=0,write=0;
        int root=rogue::game.player.y*64+rogue::game.player.x, goal=target.y*64+target.x;
        dist[root]=0; queue[write++]=int16_t(root);
        while(read<write) {
            int at=queue[read++]; if(at==goal) return int(dist[at]);
            for(int delta:{-64,1,64,-1}) {
                int n=at+delta;
                if(n<0 || n>=EXPLORER_CELLS || (delta==1 && at%64==63) || (delta==-1 && at%64==0) || dist[n]>=0 ||
                   known_terrain[n]==Terrain::Unknown || known_terrain[n]==Terrain::Wall ||
                   (safe && known_features[n]==rogue::FEATURE_SUMMONING_TRAP)) continue;
                dist[n]=int16_t(dist[at]+1); queue[write++]=int16_t(n);
            }
        }
        return EXPLORER_CELLS;
    };
    int direct=route(false),safe=route(true);
    int x=int(rogue::game.player.x)+a.dx,y=int(rogue::game.player.y)+a.dy;
    if(direct<safe && safe<EXPLORER_CELLS && x>=0 && x<64 && y>=0 && y<32 &&
       known_features[y*64+x]!=rogue::FEATURE_SUMMONING_TRAP) ++e.safe_detours;
}
void write_encounters_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,encounter,visit,floor,direction,start_turn,end_turn,actions,start_hp,max_hp,exit_hp,level,armor,weapon,weapon_enchant,offhand,light,food,healing_reserve,escape_reserve,visible_first,intentional,participants,mixed,phantom,first_phantom,phantom_contact_turn,phantom_start_hp,phantom_max_hp,known_trap,summoning,damage,healing_consumed,escape_consumed,food_consumed,combat_turns,movement_turns,kills,phantom_kills,outcome,survived,survived_25,survived_50,survived_100,next_floor,phantom_visible_first,armor_enchant,phantom_weapon,phantom_offhand,phantom_light,phantom_level,phantom_armor,phantom_armor_enchant,phantom_healing_reserve,phantom_escape_reserve,phantom_food,phantom_damage,phantom_healing_consumed,phantom_escape_consumed,species_mask,player_kills,phantom_player_kills,potion_sight_first,phantom_attack_events,phantom_sight_attack_events\n";
}
void write_encounters(std::ostream& o,const RunMetrics& r) {
    for(const auto& e:r.encounters) {
        key(o,r); o<<','<<e.id<<','<<e.visit<<','<<e.floor<<','<<(e.ascent ? "ascent" : "descent")
          <<','<<e.start<<','<<e.end<<','<<e.actions<<','<<e.hp<<','<<e.max_hp<<','<<e.exit_hp<<','<<e.level
          <<','<<e.armor<<','<<e.weapon<<','<<e.weapon_enchant<<','<<e.offhand<<','<<e.light<<','<<e.food
          <<','<<e.healing_reserve<<','<<e.escape_reserve<<','<<e.visible<<','<<e.intentional<<','<<e.participants
          <<','<<e.mixed<<','<<e.phantom<<','<<e.first_phantom<<','<<e.phantom_contact<<','<<e.phantom_hp<<','<<e.phantom_max_hp
          <<','<<e.known_trap<<','<<e.summoning<<','<<e.damage<<','<<e.healing<<','<<e.escape<<','<<e.food_used
          <<','<<e.combat_turns<<','<<e.movement_turns<<','<<e.kills<<','<<e.phantom_kills<<','<<e.outcome<<','<<e.survived;
        for(auto n:e.followup) o<<','<<n;
        o<<','<<e.next_floor<<','<<e.phantom_visible<<','<<e.armor_enchant<<','<<e.phantom_weapon<<','<<e.phantom_offhand
          <<','<<e.phantom_light<<','<<e.phantom_level<<','<<e.phantom_armor<<','<<e.phantom_armor_enchant
          <<','<<e.phantom_healing_reserve<<','<<e.phantom_escape_reserve<<','<<e.phantom_food
          <<','<<(e.damage-e.phantom_damage_before)<<','<<(e.healing-e.phantom_healing_before)<<','<<(e.escape-e.phantom_escape_before)
          <<','<<e.species_mask<<','<<e.player_kills<<','<<e.phantom_player_kills
          <<','<<e.potion_sight<<','<<e.phantom_attack_turns<<','<<e.phantom_sight_attack_turns<<'\n';
    }
}
void write_milestones_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,milestone,floor,actions,turns,hp,max_hp,level,armor,weapon,weapon_enchant,offhand,food,healing,escape\n";
}
void write_milestones(std::ostream& o,const RunMetrics& r) {
    for(const auto& m:r.milestones) {
        key(o,r); o<<','<<m.name<<','<<m.floor<<','<<m.actions<<','<<m.turns<<','<<m.hp<<','<<m.max_hp<<','<<m.level
            <<','<<m.armor<<','<<m.weapon<<','<<m.enchant<<','<<m.offhand<<','<<m.food<<','<<m.healing<<','<<m.escape<<'\n';
    }
}
}
