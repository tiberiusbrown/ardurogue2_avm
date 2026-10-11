#include "explorer.hpp"
// Generated public, pure definitions end BEFORE Game/global declarations.
#include "explorer_public_rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <queue>
#include <utility>
#include <vector>

namespace sim {
using namespace rogue;
namespace {
constexpr int8_t dxs[]={0,1,0,-1}, dys[]={-1,0,1,0};
int cell(SeenPosition p) { return p.y*EXPLORER_W+p.x; }
SeenPosition position(int n) { return {uint8_t(n%EXPLORER_W),uint8_t(n/EXPLORER_W)}; }
bool inside(int x,int y) { return x>=0 && x<EXPLORER_W && y>=0 && y<EXPLORER_H; }
int distance(SeenPosition a,SeenPosition b) { return std::abs(int(a.x)-b.x)+std::abs(int(a.y)-b.y); }
bool passable(Terrain t) { return t!=Terrain::Unknown && t!=Terrain::Wall; }
ExplorerAction use(int s,const char* goal,int target=255) {
    ExplorerAction a; a.kind=ExplorerActionKind::Use; a.slot=uint8_t(s); a.target=uint8_t(target); a.goal=goal; return a;
}
ExplorerAction move(int d,const char* goal,SeenPosition target={}) {
    ExplorerAction a; a.kind=ExplorerActionKind::Move; a.dx=dxs[d]; a.dy=dys[d]; a.goal=goal; a.destination=target; return a;
}
bool equipped(const ExplorerObservation& o,int s) {
    return std::find(o.equipped.begin(),o.equipped.end(),s)!=o.equipped.end();
}
int find(const ExplorerObservation& o,int type) {
    for(int s=0;s<16;++s) if(o.inventory[s].type==type) return s;
    return -1;
}
int quantity(const ExplorerObservation& o,int type) {
    int total=0; for(auto& i:o.inventory) if(i.type==type) total+=i.quantity; return total;
}
int ambient(const ExplorerObservation& o) {
    int bonus=0;
    for(int k=3;k<6;++k) if(o.equipped[k]<16) {
        auto i=o.inventory[o.equipped[k]];
        if(i.type==RING_ILLUMINATION) bonus+=i.cursed ? -1 : 2;
        if(i.type==AMULET_BRILLIANCE) bonus+=i.cursed ? -1 : 3;
    }
    // Effective sight also recognizes the full-light experiment without a hidden flag.
    int off=o.equipped[2]<16 ? light_bonus(o.inventory[o.equipped[2]].type) : 0;
    return o.blindness ? std::clamp(1+bonus,0,6) : std::max(1+bonus,int(o.radius)-off);
}
int equipment_score(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& i) {
    if(i.cursed || i.category==ItemCategory::None) return 0;
    if(!i.type) return (i.category==ItemCategory::Ring || i.category==ItemCategory::Amulet) ? prefs.unknown_jewelry_score : 0;
    if(is_weapon(i.type)) {
        auto w=weapon_definition(i.type);
        int score=prefs.weapon_damage_weight*(w.minimum_damage+w.maximum_damage)+prefs.weapon_accuracy_weight*w.accuracy+prefs.enchant_weight*i.enchant;
        if(i.type==STORMBRINGER) score+=10;
        if(i.type==HAMMER_OF_RUIN) score+=18;
        if(i.type==GLASS_SWORD) score-=4;
        if(is_bow(i.type)) { auto r=ranged_weapon_definition(i.type); score=prefs.weapon_damage_weight*(r.minimum_damage+r.maximum_damage)+prefs.weapon_accuracy_weight*r.accuracy+2*r.range+prefs.enchant_weight*i.enchant; }
        return score;
    }
    if(is_armor(i.type)) return prefs.armor_rating_weight*armor_definition(i.type).rating+prefs.enchant_weight*i.enchant+(i.type==DRAGONHIDE ? (o.floor>=11 ? 45 : 20) : 0);
    bool contact=o.attacked;
    for(int m=0;m<o.monster_count;++m) if(distance(o.player,o.monsters[m].pos)<=prefs.shield_contact_distance) contact=true;
    if(is_light(i.type)) return is_fueled_light(i.type) && !i.quantity ? 0 :
        (ambient(o)<3 ? prefs.light_low_bonus : prefs.light_high_bonus)+prefs.light_radius_weight*light_bonus(i.type)+(i.type==GLOWING_ORB ? 25 : std::min(20,int(i.quantity)*fuel_denominator(i.type)/30))+
        (o.yendor && !contact && ambient(o)<3 ? prefs.ascent_light_bonus : 0);
    if(is_shield(i.type)) {
        int score=(contact ? prefs.shield_contact_bonus : ambient(o)>=3 ? prefs.shield_ambient_bonus : 0)+prefs.shield_rating_weight*shield_rating(i.type)+prefs.enchant_weight*i.enchant;
        return !contact && ambient(o)<3 ? score*(o.yendor ? prefs.ascent_shield_quiet_percent : prefs.shield_quiet_percent)/100 : score;
    }
    switch(i.type) {
    case RING_ILLUMINATION: {
        int without=ambient(o);
        for(int k=4;k<6;++k) if(o.equipped[k]<16 && o.inventory[o.equipped[k]].type==RING_ILLUMINATION)
            without-=o.inventory[o.equipped[k]].cursed ? -1 : 2;
        return prefs.illumination_ring_score+(without<3 ? prefs.illumination_dark_bonus : 0);
    }
    case AMULET_BRILLIANCE: {
        int without=ambient(o);
        if(o.equipped[3]<16 && o.inventory[o.equipped[3]].type==AMULET_BRILLIANCE)
            without-=o.inventory[o.equipped[3]].cursed ? -1 : 3;
        return prefs.brilliance_score+(without<3 ? prefs.brilliance_dark_bonus : 0);
    }
    case RING_PROTECTION:return prefs.protection_ring_score; case RING_INVISIBILITY:return prefs.invisibility_ring_score;
    case RING_STRENGTH:return 61; case RING_DEXTERITY:return 66; case RING_ATTACK:return 56;
    case RING_REPRISAL:return 125; case RING_HUNT:return quantity(o,ARROWS) ? 130 : 95;
    case RING_FIRE_IMMUNITY:return prefs.fire_immunity_ring_score+(o.floor>=11 ? prefs.fire_immunity_late_bonus : 0);
    case RING_SUSTENANCE:return prefs.sustenance_ring_score+(o.yendor ? prefs.sustenance_ascent_bonus : 0);
    case RING_SEE_INVISIBLE:return prefs.see_invisible_score;
    case AMULET_SPEED:return prefs.speed_amulet_score; case AMULET_IRONBLOOD:return 90;
    case AMULET_CLARITY:return prefs.clarity_amulet_score+(o.floor==15 && !o.yendor ? prefs.clarity_lord_bonus : 0);
    case AMULET_VAMPIRE:return 80; case AMULET_VITALITY:return 75;
    case AMULET_PHOENIX_HEART:return 185; case AMULET_HEART_OF_GIANT:return 190;
    case AMULET_WISDOM:return 60; case AMULET_REGENERATION:return 40; case AMULET_CONSERVATION:return 35;
    default:return 0;
    }
}
int score_slot(const ExplorerParameters& prefs,const ExplorerObservation& o,int s) { return s<16 ? equipment_score(prefs,o,o.inventory[s]) : 0; }
int best_weapon(const ExplorerParameters& prefs,const ExplorerObservation& o,bool bow) {
    int best=255;
    for(int s=0;s<16;++s) if(is_weapon(o.inventory[s].type) && is_bow(o.inventory[s].type)==bow &&
        score_slot(prefs,o,s)>score_slot(prefs,o,best)) best=s;
    return best;
}
int equipment_target(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& i) {
    switch(i.category) {
    case ItemCategory::Weapon:return o.equipped[0]; case ItemCategory::Armor:return o.equipped[1];
    case ItemCategory::Light:case ItemCategory::Shield:return o.equipped[2]; case ItemCategory::Amulet:return o.equipped[3];
    case ItemCategory::Ring:
        if(o.equipped[4]==255 || o.equipped[5]==255) return 255;
        if(o.inventory[o.equipped[4]].cursed) return o.equipped[5];
        if(o.inventory[o.equipped[5]].cursed) return o.equipped[4];
        return score_slot(prefs,o,o.equipped[4])<score_slot(prefs,o,o.equipped[5]) ? o.equipped[4] : o.equipped[5];
    default:return 255;
    }
}
bool upgrade(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& i) {
    int s=equipment_target(prefs,o,i);
    if(s<16 && o.inventory[s].cursed) return false;
    if(is_bow(i.type)) return score_slot(prefs,o,best_weapon(prefs,o,true))<equipment_score(prefs,o,i);
    if(is_weapon(i.type) && o.equipped[0]<16 && is_bow(o.inventory[o.equipped[0]].type)) s=best_weapon(prefs,o,false);
    return equipment_score(prefs,o,i)>score_slot(prefs,o,s);
}
int value(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& i,bool retention=false) {
    if(i.category==ItemCategory::None || i.cursed) return 0;
    if(i.type==YENDOR_AMULET) return 10000;
    if(!i.type) {
        switch(i.category) {
        case ItemCategory::Potion:return prefs.unknown_potion_value; case ItemCategory::Scroll:return prefs.unknown_scroll_value;
        case ItemCategory::Ring:case ItemCategory::Amulet:return 95;
        case ItemCategory::Wand:return prefs.unknown_wand_value; case ItemCategory::Food:return quantity(o,FOOD)>=prefs.food_stock_target ? 0 : o.hunger<60 ? 180 : 70;
        case ItemCategory::Light:return 150; case ItemCategory::Shield:return 65;
        case ItemCategory::Weapon:case ItemCategory::Armor:return prefs.unknown_gear_value;
        case ItemCategory::Ammo:return 50; default:return 0;
        }
    }
    if(is_light(i.type)) {
        int reserve=0,best=0;
        for(auto& own:o.inventory) if(is_light(own.type)) {
            best=std::max(best,equipment_score(prefs,o,own)); reserve+=own.type==GLOWING_ORB ? 10000 : own.quantity*fuel_denominator(own.type);
        }
        if(retention || equipment_score(prefs,o,i)>best) return 100+equipment_score(prefs,o,i);
        return reserve<600 ? 120+equipment_score(prefs,o,i)/2 : 0;
    }
    if(is_weapon(i.type)||is_armor(i.type)||is_shield(i.type)||is_ring(i.type)||is_amulet(i.type))
        return retention || upgrade(prefs,o,i) ? 100+equipment_score(prefs,o,i) : 0;
    if(is_ammo(i.type)) return retention ? 170+std::min(24,int(i.quantity))*4 : quantity(o,ARROWS)>=24 ? 0 : best_weapon(prefs,o,true)<16 ? 170 : 45;
    if(is_wand(i.type)) {
        if(i.individual && (i.modifier==WAND_CURSED || i.modifier==WAND_UNRELIABLE || !i.quantity)) return 0;
        return i.type==WAND_DIGGING ? 60 : 65+5*i.quantity;
    }
    switch(i.type) {
    case FOOD:return !retention && quantity(o,FOOD)>=prefs.food_stock_target ? 0 : std::max(retention ? 70+20*i.quantity : 70,o.hunger<60 ? 180 : 70);
    case POTION_HEALING:return 180; case POTION_EXPERIENCE:return 220;
    case POTION_STRENGTH:return o.strength<12||o.weakened ? 160 : 40;
    case POTION_DEXTERITY:return o.dexterity<12 ? 150 : 40;
    case POTION_PARALYSIS:return 100; case POTION_CONFUSION:case POTION_BLINDNESS:return 65;
    case POTION_SLOWING:case POTION_POISON:return 60; case POTION_HARMING:return 55; case POTION_INVISIBILITY:return 70;
    case SCROLL_IDENTIFY:return 170; case SCROLL_ENCHANT:return 190;
    case SCROLL_REMOVE_CURSE:return 100; case SCROLL_MAPPING:return 170;
    case SCROLL_TORMENT:return 95; case SCROLL_MASS_CONFUSE:return 100; case SCROLL_MASS_POISON:return 75;
    case SCROLL_FEAR:return 80; case SCROLL_TELEPORT:return 85; default:return 0;
    }
}
bool same_item(const SeenItem& a,const SeenItem& b) {
    // These categories each have one public subtype. A distant generic food
    // or arrow sprite can match a carried stack without a hidden identity.
    if(a.category==b.category && (a.category==ItemCategory::Food || a.category==ItemCategory::Ammo)) return true;
    return a.category==b.category && (a.type && b.type ? a.type==b.type : a.appearance!=255 && a.appearance==b.appearance);
}
int identity_group(ItemCategory c) {
    switch(c) { case ItemCategory::Potion:return 0; case ItemCategory::Scroll:return 1;
    case ItemCategory::Ring:return 2; case ItemCategory::Amulet:return 3; case ItemCategory::Wand:return 4;
    default:return 5; }
}
bool fits(const ExplorerObservation& o,const SeenItem& incoming) {
    for(auto& i:o.inventory) {
        if(i.category==ItemCategory::None) return true;
        bool stack=incoming.category==ItemCategory::Food||incoming.category==ItemCategory::Ammo||incoming.category==ItemCategory::Potion||incoming.category==ItemCategory::Scroll;
        if(stack && same_item(i,incoming) && int(i.quantity)+incoming.quantity<=(incoming.category==ItemCategory::Ammo ? 255 : 63)) return true;
    }
    return false;
}
int replacement(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& incoming) {
    int worst=value(prefs,o,incoming)-10,slot=-1;
    int bow=best_weapon(prefs,o,true), melee=best_weapon(prefs,o,false);
    for(int s=0;s<16;++s) {
        auto i=o.inventory[s];
        if(equipped(o,s)||i.cursed || s==melee || s==bow) continue;
        if(incoming.category==ItemCategory::Ammo && i.type==RING_HUNT) continue;
        if((is_bow(incoming.type)||incoming.type==RING_HUNT) && i.type==ARROWS) continue;
        int v=value(prefs,o,i,true)+(is_stackable(i.type) ? i.quantity*5 : 0);
        if(v<worst) { worst=v; slot=s; }
    }
    return slot;
}
int trap_penalty(const ExplorerParameters& prefs,int f) {
    if(f==FEATURE_TELEPORT_TRAP) return prefs.teleport_cost;
    if(f==FEATURE_SUMMONING_TRAP) return prefs.summoning_cost;
    return f>=FEATURE_SPIKE_TRAP && f<=FEATURE_RUST_TRAP ? prefs.trap_cost : 0;
}
bool sees_monster(const ExplorerObservation& o,SeenPosition p) {
    for(int i=0;i<o.monster_count;++i) if(o.monsters[i].pos==p) return true;
    return false;
}
bool fire_safe(const ExplorerObservation& o,const SeenItem& wand,SeenPosition target) {
    if(wand.modifier==WAND_SPREADING || wand.modifier==WAND_OVERPOWERED) return false;
    int r=wand.modifier==WAND_POWERFUL ? 2 : 1;
    for(int k=1;k<6;++k) if(o.equipped[k]<16) {
        auto i=o.inventory[o.equipped[k]];
        if((i.type==RING_FIRE_IMMUNITY || i.type==DRAGONHIDE) && !i.cursed) return true;
    }
    return std::abs(int(target.x)-o.player.x)>r || std::abs(int(target.y)-o.player.y)>r;
}
}
void ExplorerAgent::reset() {
    for(auto& f:floors) { f.tiles.fill({}); f.enemies.fill({}); f.up=f.down={}; f.visited=false; f.ascent_layout=false; }
    identities={}; stats={}; active_floor=255; last_actions=last_turns=UINT64_MAX; last_experiment=last_mass_scroll=0;
    last_position=frontier=last_retreat=shadow={}; shadow_turn=0; probe_direction=0; previous={}; retreat_budget=quiet_turns=stalled=bow_cooldown=0;
    pending_ring=255;
    blocked_contact={}; blocked_contact_until=0;
}
void ExplorerAgent::remember(const ExplorerObservation& o) {
    const auto& prefs=parameters;
    auto& f=floors[o.floor];
    for(int g=0;g<6;++g) for(int a=0;a<16;++a) if(o.identities[g][a]) identities[g][a]=o.identities[g][a];
    if(active_floor!=o.floor) {
        active_floor=o.floor; frontier=shadow={}; last_position={}; last_retreat={}; retreat_budget=quiet_turns=stalled=0;
        blocked_contact={}; blocked_contact_until=0;
        // Production regenerates a DIFFERENT layout after Yendor; preserve history,
        // but never route through it as current known terrain.
        if(f.visited && f.ascent_layout!=o.yendor) {
            for(auto& t:f.tiles) { t.historical=t.terrain; t.terrain=Terrain::Unknown; t.feature=0; t.has_item=t.surveyed=false; t.visits=t.failures=0; }
            f.up=f.down={};
        }
        f.enemies.fill({}); f.visited=true; f.ascent_layout=o.yendor;
    }
    bool changed=last_actions!=o.actions || last_turns!=o.turns || last_position!=o.player;
    if(o.contact_reported && o.contact.x<64 && o.contact.y<32) { shadow=o.contact; shadow_turn=o.turns; }
    else if(o.turns>shadow_turn+3 || (previous.goal=="repeat reported melee contact" && !o.contact_reported)) shadow={};
    if(changed && last_position.x!=255 && previous.kind==ExplorerActionKind::Move && last_position==o.player && o.last_rejected) {
        int x=int(last_position.x)+previous.dx,y=int(last_position.y)+previous.dy;
        if(inside(x,y)) {
            auto& t=f.tiles[y*EXPLORER_W+x]; t.failures=uint8_t(std::min(255,int(t.failures)+1));
            ++stats.unreachable;
        }
    }
    if(changed && last_position!=o.player) {
        auto& t=f.tiles[cell(o.player)]; if(t.visits) ++stats.repeats;
        t.visits=uint8_t(std::min(255,int(t.visits)+1)); stalled=0;
    } else if(changed) ++stalled;
    for(int n=0;n<EXPLORER_CELLS;++n) {
        auto& t=f.tiles[n];
        if(o.terrain[n]!=Terrain::Unknown) {
            if(t.terrain==Terrain::Unknown) ++stats.revealed;
            t.terrain=o.terrain[n]; t.feature=o.features[n];
            if(passable(t.terrain)) t.failures=0;
            if(t.terrain==Terrain::Up) f.up=position(n);
            if(t.terrain==Terrain::Down) f.down=position(n);
        }
        if(o.visible[n]) {
            t.surveyed=true;
            bool found=false;
            for(int i=0;i<o.ground_count;++i) if(cell(o.ground[i].pos)==n) { found=true; break; }
            bool covered=sees_monster(o,position(n));
            if(t.has_item && !found && !covered) ++stats.missing_items;
            if(!found && !covered) { t.has_item=false; t.item={}; }
        }
    }
    for(int i=0;i<o.ground_count;++i) {
        auto& g=o.ground[i]; auto& t=f.tiles[cell(g.pos)];
        // A generic sprite does not erase a detailed description learned earlier.
        if(!t.has_item || t.item.category!=g.item.category || g.item.appearance!=255 || g.item.type) t.item=g.item;
        t.has_item=true;
    }
    for(auto& t:f.tiles) if(t.has_item && !t.item.type && t.item.appearance<16)
        t.item.type=identities[identity_group(t.item.category)][t.item.appearance];
    if(changed && stalled>=unsigned(prefs.stationary_contact_turns) && o.contact_reported && distance(o.player,o.contact)==1) {
        // An accepted attack may still block travel indefinitely. Our own contact
        // square and lack of movement justify a temporary detour, without knowing
        // which creature is there now or where it originated.
        blocked_contact=o.contact; blocked_contact_until=o.turns+std::min(64,prefs.stationary_contact_turns);
        shadow={}; frontier={}; stalled=0;
    }
    for(auto& m:f.enemies) if(m.type && (o.turns>m.seen+prefs.monster_memory_turns || (o.visible[cell(m.pos)] && !sees_monster(o,m.pos)))) m={};
    for(int i=0;i<o.monster_count;++i) {
        auto& s=o.monsters[i]; int slot=-1;
        for(int j=0;j<12;++j) if(f.enemies[j].pos==s.pos || !f.enemies[j].type) { slot=j; break; }
        if(slot<0) slot=i;
        f.enemies[slot]={s.pos,s.type,o.turns};
    }
    if(changed) {
        quiet_turns=o.monster_count || o.attacked ? 0 : quiet_turns+1;
        if(quiet_turns>prefs.retreat_reset_quiet_turns) retreat_budget=0;
        if(bow_cooldown) --bow_cooldown;
    }
    last_position=o.player; last_actions=o.actions; last_turns=o.turns;
}
void ExplorerAgent::paths(const ExplorerObservation& o) {
    const auto& prefs=parameters;
    costs.fill(30000); steps.fill(-1); first.fill(-1);
    auto& f=floors[o.floor]; int root=cell(o.player);
    using Node=std::pair<int,int>;
    // Retain the backing allocation between decisions; no per-turn allocation.
    thread_local std::vector<Node> storage;
    storage.clear(); storage.reserve(EXPLORER_CELLS*4);
    auto push=[&](Node n) { storage.push_back(n); std::push_heap(storage.begin(),storage.end(),std::greater<Node>()); };
    costs[root]=0; steps[root]=0; push({0,root});
    while(!storage.empty()) {
        std::pop_heap(storage.begin(),storage.end(),std::greater<Node>());
        auto [cost,at]=storage.back(); storage.pop_back();
        if(cost!=costs[at]) continue;
        for(int d=0;d<4;++d) {
            int x=at%EXPLORER_W+dxs[d],y=at/EXPLORER_W+dys[d];
            if(!inside(x,y)) continue;
            int n=y*EXPLORER_W+x; auto& t=f.tiles[n];
            if(!passable(t.terrain) || t.failures>=2) continue;
            if(o.turns<blocked_contact_until && position(n)==blocked_contact) continue;
            int penalty=trap_penalty(prefs,t.feature)+(t.terrain==Terrain::ClosedDoor ? 1 : 0);
            for(auto& m:f.enemies) if(m.type>=ORC && o.turns<=m.seen+prefs.monster_memory_turns && distance(position(n),m.pos)<=1) penalty+=prefs.remembered_threat_cost;
            int next=cost+1+penalty;
            if(next<costs[n]) { costs[n]=int16_t(next); steps[n]=int16_t(steps[at]+1); first[n]=at==root ? int8_t(d) : first[at]; push({next,n}); }
        }
    }
}
ExplorerAction ExplorerAgent::navigate(SeenPosition p,const char* goal) {
    if(p.x!=255 && first[cell(p)]>=0) return move(first[cell(p)],goal,p);
    ExplorerAction a; a.goal="no reachable known route"; a.no_path=true; ++stats.unreachable; return a;
}
ExplorerAction ExplorerAgent::pickup(const ExplorerObservation& o) {
    const auto& prefs=parameters;
    int best=-1,worth=0;
    for(int i=0;i<o.ground_count;++i) {
        auto& g=o.ground[i]; int v=value(prefs,o,g.item);
        bool survival=g.item.type==FOOD || g.item.type==POTION_HEALING || is_light(g.item.type);
        if(g.pos!=o.player || g.handle>=16 || (o.yendor && g.item.type!=YENDOR_AMULET && !survival)) continue;
        if(v>worth && (g.item.type==YENDOR_AMULET || fits(o,g.item)||replacement(prefs,o,g.item)>=0)) { best=i; worth=v; }
    }
    if(best<0) return {};
    auto& g=o.ground[best]; ExplorerAction a;
    a.kind=ExplorerActionKind::Take; a.target=g.handle; a.goal="collect observed item"; a.destination=g.pos;
    if(g.item.type!=YENDOR_AMULET && !fits(o,g.item)) a.slot=uint8_t(replacement(prefs,o,g.item));
    return a;
}
ExplorerAction ExplorerAgent::choose_action(const ExplorerObservation& incoming) {
    const auto& prefs=parameters;
    remember(incoming);
    auto o=incoming;
    // Keep a shield briefly when an observed nearby enemy drops out of sight
    // after changing offhand gear. This is uncertainty from our own sightings,
    // not a fresh enemy position, and never creates a combat target.
    for(auto& m:floors[o.floor].enemies) if(m.type && o.turns<=m.seen+prefs.shield_threat_memory_turns &&
        distance(o.player,m.pos)<=prefs.shield_contact_distance) o.attacked=true;
    auto decide=[&]() -> ExplorerAction {
        auto& f=floors[o.floor];
        if(o.pending_pickups) return pickup(o);
        if(o.paralyzed) { ExplorerAction a; a.goal="recover from paralysis"; return a; }
        int adjacent=-1, threats=0,nearest=100; bool dragon_threat=false,mass_target=false;
        for(int m=0;m<o.monster_count;++m) {
            int d=distance(o.player,o.monsters[m].pos); nearest=std::min(nearest,d);
            if(d==1 && (adjacent<0 || o.monsters[m].type>o.monsters[adjacent].type)) adjacent=m;
            if(d<=2) ++threats;
            if(o.monsters[m].type>=INCUBUS && d<=prefs.mass_scroll_distance) mass_target=true;
            if(o.monsters[m].type==DRAGON && d<=6) dragon_threat=true;
        }
        // Incoming status text establishes danger without establishing a bearing.
        // An invisible attacker still warrants combat healing and escape supplies.
        if(o.attacked && o.reported_attacker) threats=std::max(1,threats);
        // Finish an observed opponent instead of alternating between adjacent
        // species. This uses our own action history, never the target's live HP.
        if(previous.kind==ExplorerActionKind::Move && previous.goal=="melee visible monster")
            for(int m=0;m<o.monster_count;++m) if(o.monsters[m].pos==previous.destination && distance(o.player,o.monsters[m].pos)==1) adjacent=m;
        bool emergency=threats && (o.hp<prefs.emergency_hp || o.hp*100<o.max_hp*prefs.emergency_percent || threats>=prefs.crowd_threshold);
        if(dragon_threat && o.hp*100<o.max_hp*prefs.dragon_escape_percent) emergency=true;
        int healing=find(o,POTION_HEALING);
        if(healing>=0 && (o.hp*100<o.max_hp*prefs.healing_percent || (threats && o.hp*100<o.max_hp*prefs.combat_healing_percent))) return use(healing,"known healing");
        int strength=find(o,POTION_STRENGTH);
        if(o.weakened && strength>=0) return use(strength,"restore strength");
        int xp=find(o,POTION_EXPERIENCE);
        if(xp>=0) return use(xp,"gain levels and recover HP");
        if((mass_target || threats>=prefs.crowd_threshold) && o.turns>=last_mass_scroll+prefs.mass_scroll_cooldown) {
            for(int t:{SCROLL_MASS_CONFUSE,SCROLL_TORMENT,SCROLL_MASS_POISON}) {
                int s=find(o,t); if(s>=0) { last_mass_scroll=o.turns; return use(s,"control observed dangerous group"); }
            }
        }
        if(emergency) {
            for(int t:{SCROLL_FEAR,SCROLL_TELEPORT,POTION_INVISIBILITY}) {
                if(t==SCROLL_FEAR && !o.monster_count) continue;
                int s=find(o,t); if(s>=0 && (t!=POTION_INVISIBILITY || !o.invisible)) return use(s,"emergency control or escape");
            }
        }
        int food=find(o,FOOD);
        if(food>=0 && o.hunger<prefs.food_threshold) return use(food,"maintain hunger");
        if(o.turns<blocked_contact_until && distance(o.player,blocked_contact)==1) {
            int best=-1,visits=256;
            for(int d=0;d<4;++d) {
                int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d]; if(!inside(x,y)) continue;
                auto p=SeenPosition{uint8_t(x),uint8_t(y)}; auto& t=f.tiles[cell(p)];
                if(p!=blocked_contact && passable(t.terrain) && !t.feature && !sees_monster(o,p) && int(t.visits)<visits) {
                    best=d; visits=t.visits;
                }
            }
            if(best>=0) return move(best,"detour around repeated melee contact",{uint8_t(int(o.player.x)+dxs[best]),uint8_t(int(o.player.y)+dys[best])});
            ExplorerAction a; a.goal="wait for observed contact detour"; return a;
        }
        if(pending_ring<16) {
            auto i=o.inventory[pending_ring];
            if(i.category!=ItemCategory::Ring || i.cursed || equipped(o,pending_ring)) pending_ring=255;
            else if(i.type || (!threats && quiet_turns>=prefs.jewelry_quiet_turns)) {
                if(o.equipped[4]==255 || o.equipped[5]==255) {
                    int slot=pending_ring; pending_ring=255; return use(slot,"finish planned ring replacement");
                }
                pending_ring=255;
            }
        }
        // Safe equipment upgrades use displayed identity/enchantment only.
        int upgrade_slot=-1,upgrade_gain=-1;
        for(int s=0;s<16;++s) {
            auto i=o.inventory[s];
            int old=equipment_target(prefs,o,i);
            bool experiment=!i.type && (i.category==ItemCategory::Ring || i.category==ItemCategory::Amulet) && !threats &&
                quiet_turns>=prefs.jewelry_quiet_turns && o.hp*100>=o.max_hp*prefs.jewelry_experiment_health_percent &&
                (old==255 || !o.inventory[old].cursed) && score_slot(prefs,o,old)<=prefs.jewelry_experiment_max_score;
            if(equipped(o,s) || is_bow(i.type) || (!experiment && !upgrade(prefs,o,i))) continue;
            bool equipment=i.category==ItemCategory::Weapon||i.category==ItemCategory::Armor||i.category==ItemCategory::Light||i.category==ItemCategory::Shield||i.category==ItemCategory::Ring||i.category==ItemCategory::Amulet;
            if(!equipment) continue;
            if(!i.type && (threats || quiet_turns<prefs.jewelry_quiet_turns)) continue;
            int gain=equipment_score(prefs,o,i)-score_slot(prefs,o,old);
            if(experiment) gain=std::max(1,gain);
            if(gain>upgrade_gain) { upgrade_gain=gain; upgrade_slot=s; }
        }
        if(upgrade_slot>=0) {
            int s=upgrade_slot; auto i=o.inventory[s]; int old=equipment_target(prefs,o,i);
            if(i.category==ItemCategory::Ring && o.equipped[4]<16 && o.equipped[5]<16) {
                if(!o.inventory[old].cursed) { pending_ring=uint8_t(s); return use(old,"remove weaker known ring"); }
            }
            return use(s,i.type ? "equip observed upgrade" : "try unknown jewelry");
        }
        int melee=best_weapon(prefs,o,false);
        if((adjacent>=0 || (shadow.x!=255 && distance(shadow,o.player)==1)) &&
            o.equipped[0]<16 && is_bow(o.inventory[o.equipped[0]].type) && melee<16 && !o.inventory[o.equipped[0]].cursed) {
            bow_cooldown=4; return use(melee,"restore melee weapon for close contact");
        }
        // Ranged decisions use only visible species and known ray cells. No scan_ray.
        for(int d=0;d<4;++d) {
            int target=-1;
            for(int step=1;step<=6;++step) {
                int x=int(o.player.x)+dxs[d]*step,y=int(o.player.y)+dys[d]*step;
                if(!inside(x,y)) break;
                int n=y*EXPLORER_W+x;
                if(!o.visible[n] || f.tiles[n].terrain==Terrain::Wall || f.tiles[n].terrain==Terrain::ClosedDoor) break;
                for(int m=0;m<o.monster_count;++m) if(cell(o.monsters[m].pos)==n) { target=m; break; }
                if(target>=0) break;
            }
            if(target<0) continue;
            auto& monster=o.monsters[target]; int range=distance(monster.pos,o.player);
            bool danger=monster.type>=ORC || emergency;
            if(o.turns>=last_experiment+prefs.wand_experiment_interval) {
                for(int s=0;s<16;++s) if(o.inventory[s].category==ItemCategory::Wand && !o.inventory[s].individual) {
                    auto i=o.inventory[s];
                    bool known=i.type!=0;
                    if(range<(known ? prefs.known_wand_experiment_range : prefs.unknown_wand_experiment_range)) continue;
                    if((!known || i.type==WAND_FIRE) && (adjacent>=0 || range<3)) continue;
                    if(i.type==WAND_DIGGING || (i.type==WAND_POLYMORPH && (!emergency || monster.type==LORD)) ||
                        ((i.type==WAND_TELEPORT || i.type==WAND_FORCE) && !emergency)) continue;
                    last_experiment=o.turns; ExplorerAction a; a.kind=ExplorerActionKind::Wand; a.slot=uint8_t(s);
                    a.dx=dxs[d]; a.dy=dys[d]; a.goal="test unexamined wand against observed distant enemy"; a.destination=monster.pos; return a;
                }
            }
            for(int t:{POTION_PARALYSIS,POTION_CONFUSION,POTION_BLINDNESS,POTION_SLOWING,POTION_POISON,WAND_ICE,WAND_STRIKING,WAND_FIRE,POTION_HARMING,WAND_TELEPORT,WAND_FORCE,WAND_POLYMORPH}) {
                int s=find(o,t); if(s<0 || !danger) continue;
                auto i=o.inventory[s];
                if(is_wand(t) && (!i.individual || !i.quantity || i.modifier==WAND_CURSED || i.modifier==WAND_UNRELIABLE)) continue;
                if(t==WAND_FIRE && (monster.type==DRAGON || !fire_safe(o,i,monster.pos))) continue;
                if((t==WAND_TELEPORT || t==WAND_FORCE || t==WAND_POLYMORPH) && !emergency) continue;
                if(t==WAND_POLYMORPH && (monster.type==LORD || monster.type<INCUBUS)) continue;
                if(is_potion(t) && t!=POTION_HARMING && !emergency && monster.type<INCUBUS) continue;
                ExplorerAction a; a.kind=is_wand(t) ? ExplorerActionKind::Wand : ExplorerActionKind::Throw;
                a.slot=uint8_t(s); a.dx=dxs[d]; a.dy=dys[d]; a.goal="target visible monster"; a.destination=monster.pos;
                if(is_wand(t) && (i.modifier==WAND_SPREADING || i.modifier==WAND_OVERPOWERED)) a.dx=a.dy=0;
                return a;
            }
            int bow=best_weapon(prefs,o,true);
            if(bow<16 && quantity(o,ARROWS) && !bow_cooldown && (range>=3 || o.equipped[0]==bow) && range<=ranged_weapon_definition(o.inventory[bow].type).range) {
                if(o.equipped[0]!=bow) {
                    if(o.equipped[0]<16 && o.inventory[o.equipped[0]].cursed) continue;
                    return use(bow,"prepare visible ranged shot");
                }
                ExplorerAction a; a.kind=ExplorerActionKind::Throw; a.slot=uint8_t(find(o,ARROWS));
                a.dx=dxs[d]; a.dy=dys[d]; a.goal="shoot visible monster"; a.destination=monster.pos; return a;
            }
        }
        if(o.equipped[0]<16 && is_bow(o.inventory[o.equipped[0]].type) && melee<16 && !o.inventory[o.equipped[0]].cursed) {
            bow_cooldown=4; return use(melee,"restore melee weapon");
        }
        if(adjacent>=0) {
            if(emergency && retreat_budget<prefs.retreat_steps) {
                int best=-1,best_score=-10000;
                for(int d=0;d<4;++d) {
                    int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d];
                    if(!inside(x,y)) continue;
                    auto p=SeenPosition{uint8_t(x),uint8_t(y)}; auto& t=f.tiles[cell(p)];
                    if(!o.visible[cell(p)] || !passable(t.terrain) || t.terrain==Terrain::ClosedDoor || t.feature || sees_monster(o,p)) continue;
                    int separation=100,connections=0;
                    for(int m=0;m<o.monster_count;++m) separation=std::min(separation,distance(p,o.monsters[m].pos));
                    for(int k=0;k<4;++k) if(inside(x+dxs[k],y+dys[k]) && passable(f.tiles[(y+dys[k])*EXPLORER_W+x+dxs[k]].terrain)) ++connections;
                    int score=separation*prefs.retreat_separation_weight+connections*prefs.retreat_connectivity_weight-(p==last_retreat ? prefs.retreat_reversal_cost : 0);
                    if(separation>nearest && connections>=prefs.retreat_connections && score>best_score) { best=d; best_score=score; }
                }
                if(best>=0) { ++retreat_budget; last_retreat=o.player; return move(best,"bounded observed retreat"); }
            }
            auto p=o.monsters[adjacent].pos;
            for(int d=0;d<4;++d) if(int(p.x)==int(o.player.x)+dxs[d] && int(p.y)==int(o.player.y)+dys[d]) return move(d,"melee visible monster",p);
        }
        if(shadow.x!=255 && distance(shadow,o.player)==1 && o.turns<=shadow_turn+3) {
            for(int d=0;d<4;++d) if(int(shadow.x)==int(o.player.x)+dxs[d] && int(shadow.y)==int(o.player.y)+dys[d]) return move(d,"repeat reported melee contact",shadow);
        }
        if(o.attacked && !o.monster_count && o.reported_attacker && !o.confused) {
            // The message reports species, never bearing. Probe cardinal known
            // cells deterministically; a hit/miss message can establish contact.
            for(int k=0;k<4;++k) {
                int d=int(probe_direction++%4),x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d];
                if(inside(x,y) && passable(f.tiles[y*64+x].terrain) && !f.tiles[y*64+x].feature)
                    return move(d,"probe for reported unseen attacker",{uint8_t(x),uint8_t(y)});
            }
        }
        auto picked=pickup(o); if(picked.kind==ExplorerActionKind::Take) return picked;
        if(!threats) {
            for(int t:{POTION_STRENGTH,POTION_DEXTERITY}) { int s=find(o,t); if(s>=0 && (t==POTION_STRENGTH ? o.strength : o.dexterity)<12) return use(s,"permanent stats"); }
            int unknown=-1,unknown_priority=-1;
            for(int s=0;s<16;++s) if(o.inventory[s].category!=ItemCategory::None && (!o.inventory[s].type || (!o.inventory[s].individual && o.inventory[s].category==ItemCategory::Wand))) {
                auto category=o.inventory[s].category;
                int priority=category==ItemCategory::Amulet ? 3 : category==ItemCategory::Ring ? 2 : category==ItemCategory::Wand ? 1 : 0;
                if(priority>unknown_priority) { unknown=s; unknown_priority=priority; }
            }
            int identify=find(o,SCROLL_IDENTIFY);
            if(unknown>=0 && identify>=0) return use(identify,"identify before experimenting",unknown);
            int uncurse=find(o,SCROLL_REMOVE_CURSE);
            for(int s=0;s<16;++s) if(o.inventory[s].cursed && uncurse>=0) return use(uncurse,"remove known curse",s);
            int enchant=find(o,SCROLL_ENCHANT);
            if(enchant>=0) {
                for(int k:{1,0,3,4,5}) if(o.equipped[k]<16 && !o.inventory[o.equipped[k]].cursed) return use(enchant,"enchant equipped item",o.equipped[k]);
            }
            int mapping=find(o,SCROLL_MAPPING);
            if(mapping>=0 && (o.yendor ? f.up.x==255 : f.down.x==255)) return use(mapping,"legitimate magic mapping");
            if(!o.yendor && quiet_turns>=prefs.scroll_quiet_turns && o.hp>=prefs.scroll_min_hp && !o.blindness && !o.confused && o.turns>=last_experiment+prefs.scroll_interval) {
                for(int s=0;s<16;++s) if(!o.inventory[s].type && o.inventory[s].category==ItemCategory::Scroll) {
                    last_experiment=o.turns;
                    int target=unknown>=0 && unknown!=s ? unknown : o.equipped[1]<16 ? o.equipped[1] : o.equipped[0];
                    return use(s,"safe unknown scroll experiment",target);
                }
            }
            if(!o.yendor && quiet_turns>=prefs.experiment_quiet_turns && o.hp>=prefs.potion_min_hp && o.hp*100>=o.max_hp*prefs.potion_health_percent && !o.blindness && !o.confused && !o.slowed && o.turns>=last_experiment+prefs.experiment_interval) {
                for(int s=0;s<16;++s) if(!o.inventory[s].type && o.inventory[s].category==ItemCategory::Potion) {
                    last_experiment=o.turns;
                    int target=unknown>=0 && unknown!=s ? unknown : o.equipped[1]<16 ? o.equipped[1] : o.equipped[0];
                    return use(s,"bounded safe consumable experiment",target);
                }
            }
        }
        if(o.blindness) { ExplorerAction a; a.goal="wait out blindness using retained memory"; return a; }
        paths(o);
        SeenPosition stairs=o.yendor ? f.up : f.down;
        if(o.yendor && o.hunger<prefs.ascent_food_seek_hunger && quantity(o,FOOD)<prefs.ascent_food_reserve_units) {
            int food=-1,best_cost=30000;
            for(int n=0;n<EXPLORER_CELLS;++n) if(f.tiles[n].has_item && f.tiles[n].item.category==ItemCategory::Food &&
                steps[n]>0 && steps[n]<=prefs.ascent_food_detour_distance && costs[n]<best_cost &&
                (fits(o,f.tiles[n].item) || replacement(prefs,o,f.tiles[n].item)>=0)) { food=n; best_cost=costs[n]; }
            if(food>=0) return navigate(position(food),"collect remembered food needed for ascent");
        }
        if(stairs==o.player && o.yendor) { ExplorerAction a; a.kind=ExplorerActionKind::Stairs; a.goal="ascend and escape"; return a; }
        if(o.yendor && stairs.x!=255 && steps[cell(stairs)]>=0) return navigate(stairs,"return to discovered up stairs");
        // Observable early experience opportunities, without chasing hidden enemies.
        if(!o.yendor && o.floor<prefs.early_xp_floors && o.level<prefs.early_xp_level && o.hp*100>=o.max_hp*prefs.early_xp_health_percent) {
            for(int m=0;m<o.monster_count;++m) {
                auto& s=o.monsters[m];
                if(s.type>=SNAKE && s.type<=GOBLIN && steps[cell(s.pos)]>0 && steps[cell(s.pos)]<=prefs.early_xp_distance)
                    return navigate(s.pos,"approach visible early experience");
            }
        }
        int loot=-1,loot_score=0;
        if(!o.yendor) for(int n=0;n<EXPLORER_CELLS;++n) {
            auto& t=f.tiles[n]; if(!t.has_item || steps[n]<=0 || steps[n]>prefs.loot_max_distance || t.visits>=prefs.loot_max_visits) continue;
            if(!fits(o,t.item) && replacement(prefs,o,t.item)<0) continue;
            int score=value(prefs,o,t.item)*100/(costs[n]+prefs.loot_distance_offset)-int(t.visits)*prefs.loot_visit_cost;
            if(score>loot_score) { loot=n; loot_score=score; }
        }
        if(loot>=0 && (stairs.x==255 || o.hunger>=60 || steps[loot]<=4)) return navigate(position(loot),"seek remembered item sprite");
        // Compare discovered stairs with observable frontier gain. Runtime weights
        // can favor gathering resources before descent, without unseen loot targets.
        int best=-1,best_score=-100000;
        int radius=std::max(1,int(o.radius));
        for(int n=0;n<EXPLORER_CELLS;++n) {
            auto& t=f.tiles[n]; if(steps[n]<=0 || !passable(t.terrain) || t.failures>=2) continue;
            int x=n%EXPLORER_W,y=n/EXPLORER_W,adjacent_unknown=0,gain=0;
            auto needs_survey=[&](int at) { auto& tile=f.tiles[at]; return tile.terrain==Terrain::Unknown || (passable(tile.terrain) && !tile.surveyed); };
            for(int d=0;d<4;++d) if(inside(x+dxs[d],y+dys[d]) && needs_survey((y+dys[d])*EXPLORER_W+x+dxs[d])) ++adjacent_unknown;
            if(!t.surveyed) ++adjacent_unknown;
            if(!adjacent_unknown) continue;
            for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx)
                if(dx*dx+dy*dy<=radius*radius && inside(x+dx,y+dy) && needs_survey((y+dy)*EXPLORER_W+x+dx)) ++gain;
            int score=prefs.frontier_adjacent_weight*adjacent_unknown+prefs.frontier_gain_weight*gain-prefs.frontier_distance_cost*costs[n]-prefs.frontier_visit_cost*t.visits;
            if(position(n)==frontier) score+=prefs.frontier_hold_bonus;
            if(o.hunger<40) score-=steps[n]*prefs.hunger_distance_cost;
            if(score>best_score) { best=n; best_score=score; }
        }
        if(stairs.x!=255 && steps[cell(stairs)]>=0) {
            int stair_score=prefs.stair_base_score-prefs.stair_distance_cost*costs[cell(stairs)];
            if(o.hp*2<o.max_hp || o.hunger<40) stair_score+=prefs.stair_low_health_bonus;
            if(best<0 || stair_score>=best_score) {
                if(stairs==o.player) { ExplorerAction a; a.kind=ExplorerActionKind::Stairs; a.goal="descend discovered stairs"; return a; }
                return navigate(stairs,"reach discovered stairs");
            }
        }
        if(best>=0) {
            if(frontier!=position(best)) { frontier=position(best); ++stats.frontier_changes; }
            ++stats.exploring_actions; return navigate(frontier,"explore known frontier");
        }
        // Radius zero has no passive neighboring reveals. Explicit tactile probes
        // have a stated basis and never fabricate a path through unknown terrain.
        if(!o.radius) {
            int best_probe=-1,visits=1000;
            for(int d=0;d<4;++d) {
                int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d]; if(!inside(x,y)) continue;
                auto& t=f.tiles[y*EXPLORER_W+x];
                if(t.terrain!=Terrain::Wall && t.failures<2 && t.visits<visits) { best_probe=d; visits=t.visits; }
            }
            if(best_probe>=0) { ++stats.exploring_actions; return move(best_probe,"explicit tactile probe at radius zero"); }
        }
        // Last-seen enemies are never live targets. Only a visible final-floor
        // opponent supplies a destination; otherwise exploration continues.
        if(o.floor==15 && !o.yendor) for(int m=0;m<o.monster_count;++m)
            if(o.monsters[m].type==LORD && steps[cell(o.monsters[m].pos)]>0) return navigate(o.monsters[m].pos,"approach observed Lord");
        if(o.floor==15 && !o.yendor) {
            // Mapping or teleportation can leave an unseen Lord/Yendor on
            // already surveyed terrain. Patrol known cells by our own visits;
            // no unseen monster or item position supplies the destination.
            int target=-1,score=INT32_MAX;
            for(int n=0;n<EXPLORER_CELLS;++n) if(steps[n]>0 && passable(f.tiles[n].terrain) && !trap_penalty(prefs,f.tiles[n].feature)) {
                int candidate=int(f.tiles[n].visits)*10000+costs[n];
                if(candidate<score) { score=candidate; target=n; }
            }
            if(target>=0) { ++stats.exploring_actions; return navigate(position(target),"patrol surveyed final floor"); }
        }
        ExplorerAction a;
        if(o.turns<blocked_contact_until) { a.goal="wait for observed contact detour"; return a; }
        a.no_path=true; a.goal="exploration exhausted: no known stairs or reachable frontier"; return a;
    };
    auto a=decide(); previous=a; return a;
}
SeenFeatureChoice ExplorerAgent::choose_feature(const ExplorerObservation& o) const {
    const auto& prefs=parameters;
    SeenFeatureChoice choice;
    int type=o.features[cell(o.player)];
    if(type<FEATURE_FOUNTAIN || type>FEATURE_ALCHEMICAL_POOL || o.paralyzed) return choice;
    if(floors[o.floor].tiles[cell(o.player)].visits>=1 && type!=FEATURE_HEALING_SPRING) return choice;
    bool pressure=o.attacked;
    for(int m=0;m<o.monster_count;++m) if(distance(o.monsters[m].pos,o.player)<=2) pressure=true;
    if(type==FEATURE_HEALING_SPRING) choice.accept=o.hp<o.max_hp || o.weakened || o.confused || o.slowed;
    else if(type==FEATURE_FOUNTAIN) choice.accept=o.hp>=10 && (o.hp<o.max_hp || o.hunger<180 || o.weakened);
    else choice.accept=!pressure && o.hp>=10;
    if(type==FEATURE_ENCHANTED_FORGE || type==FEATURE_ALCHEMICAL_POOL) {
        int best=-1;
        for(int s=0;s<16;++s) {
            auto i=o.inventory[s];
            bool eligible=type==FEATURE_ENCHANTED_FORGE ? is_weapon(i.type)||is_armor(i.type)||is_shield(i.type) : i.category==ItemCategory::Wand;
            if(!eligible || i.cursed || (type==FEATURE_ENCHANTED_FORGE && i.enchant>=MAX_EQUIPMENT_ENCHANT)) continue;
            int score=equipment_score(prefs,o,i)+(equipped(o,s) ? 100 : 0);
            if(score>best) { best=score; choice.slot=uint8_t(s); }
        }
        choice.accept=choice.accept && choice.slot!=255;
    } else if(type==FEATURE_SACRIFICIAL_ALTAR) {
        choice.accept=false; // Preserve supplies under uncertainty.
    }
    return choice;
}
}
