#include "explorer.hpp"
// Generated public, pure definitions end BEFORE Game/global declarations.
#include "explorer_public_rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <queue>
#include <utility>
#include <tuple>
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
    for(int s=0;s<o.inventory_capacity;++s) if(o.inventory[s].type==type) return s;
    return -1;
}
int quantity(const ExplorerObservation& o,int type) {
    int total=0; for(auto& i:o.inventory) if(i.type==type) total+=i.quantity; return total;
}
int ambient(const ExplorerObservation& o) {
    int bonus=0;
    for(int k=3;k<6;++k) if(o.equipped[k]<o.inventory_capacity) {
        auto i=o.inventory[o.equipped[k]];
        if(i.type==RING_ILLUMINATION) bonus+=i.cursed ? -1 : 2;
        if(i.type==AMULET_BRILLIANCE) bonus+=i.cursed ? -1 : 3;
    }
    // Effective sight also recognizes the full-light experiment without a hidden flag.
    int off=o.equipped[2]<o.inventory_capacity ? light_bonus(o.inventory[o.equipped[2]].type) : 0;
    if(!o.blindness && o.radius>std::clamp(1+bonus+off,0,6)) return o.radius;
    return std::clamp(1+bonus,0,6);
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
    if((is_light(i.type) || is_shield(i.type)) && prefs.offhand_mode) {
        if(is_fueled_light(i.type) && !i.quantity) return 0;
        int sight=o.blindness ? 0 : std::clamp(ambient(o)+light_bonus(i.type),0,6);
        if(prefs.offhand_mode==3) return is_light(i.type) ? 1000+40*sight+(i.type==GLOWING_ORB ? 10 : 0) : 0;
        if(prefs.offhand_mode==4) return is_shield(i.type) ? 1000+40*shield_rating(i.type)+prefs.enchant_weight*i.enchant : 40*sight;
        if(prefs.offhand_mode==5 && o.equipped[2]<o.inventory_capacity && o.inventory[o.equipped[2]].category!=ItemCategory::None &&
            i.category!=o.inventory[o.equipped[2]].category) return 0;
        // Evaluate resulting sight for the whole worn configuration. Diminishing
        // returns let permanent jewelry make a shield useful during exploration.
        int per_radius=5*prefs.light_radius_weight;
        int visibility=sight<=3 ? sight*per_radius : 3*per_radius+(sight-3)*per_radius/2;
        if(prefs.offhand_mode==1) visibility+=prefs.light_radius_weight*light_bonus(i.type)*3;
        bool physical=false, special=false;
        for(int m=0;m<o.monster_count;++m) {
            const auto& monster=o.monsters[m];
            if(distance(o.player,monster.pos)<=2) physical=true;
            if(monster.traits&(MON_FIRE_BREATH|MON_NATURAL_INVIS)) special=true;
        }
        int defense=is_shield(i.type) ? shield_rating(i.type)*(physical || o.attacked ? 2*prefs.shield_rating_weight : prefs.shield_rating_weight/2)+prefs.enchant_weight*i.enchant : 0;
        if(contact && !special) visibility=visibility*2/3;
        if(o.blindness) visibility=0;
        int fuel=is_light(i.type) ? (i.type==GLOWING_ORB ? 35 : std::min(25,int(i.quantity)*fuel_denominator(i.type)/30)) : 0;
        return visibility+defense+fuel;
    }
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
        for(int k=4;k<6;++k) if(o.equipped[k]<o.inventory_capacity && o.inventory[o.equipped[k]].type==RING_ILLUMINATION)
            without-=o.inventory[o.equipped[k]].cursed ? -1 : 2;
        return prefs.illumination_ring_score+(without<3 ? prefs.illumination_dark_bonus : 0);
    }
    case AMULET_BRILLIANCE: {
        int without=ambient(o);
        if(o.equipped[3]<o.inventory_capacity && o.inventory[o.equipped[3]].type==AMULET_BRILLIANCE)
            without-=o.inventory[o.equipped[3]].cursed ? -1 : 3;
        return prefs.brilliance_score+(without<3 ? prefs.brilliance_dark_bonus : 0);
    }
    case RING_PROTECTION:return prefs.protection_ring_score; case RING_INVISIBILITY:return prefs.invisibility_ring_score;
    case RING_STRENGTH:return 61; case RING_DEXTERITY:return 66; case RING_ATTACK:return 56;
    case RING_REPRISAL:return 125; case RING_HUNT:return quantity(o,ARROWS) ? 130 : 95;
    case RING_FIRE_IMMUNITY:return prefs.fire_immunity_ring_score+(o.floor>=11 ? prefs.fire_immunity_late_bonus : 0);
    case RING_SUSTENANCE:return prefs.sustenance_ring_score+(o.yendor ? prefs.sustenance_ascent_bonus : 0);
    case RING_SEE_INVISIBLE:return prefs.see_invisible_score+((o.reported_attacker==PHANTOM || (o.floor>=2 && o.floor<=6)) ? 180 : 0);
    case AMULET_SPEED:return prefs.speed_amulet_score; case AMULET_IRONBLOOD:return 90;
    case AMULET_CLARITY:return prefs.clarity_amulet_score+(o.floor==15 && !o.yendor ? prefs.clarity_lord_bonus : 0);
    case AMULET_VAMPIRE:return 80; case AMULET_VITALITY:return 75;
    case AMULET_PHOENIX_HEART:return 185; case AMULET_HEART_OF_GIANT:return 190;
    case AMULET_WISDOM:return 60; case AMULET_REGENERATION:return 40; case AMULET_CONSERVATION:return 35;
    default:return 0;
    }
}
int score_slot(const ExplorerParameters& prefs,const ExplorerObservation& o,int s) { return s<o.inventory_capacity ? equipment_score(prefs,o,o.inventory[s]) : 0; }
int best_weapon(const ExplorerParameters& prefs,const ExplorerObservation& o,bool bow) {
    int best=255;
    for(int s=0;s<o.inventory_capacity;++s) if(is_weapon(o.inventory[s].type) && is_bow(o.inventory[s].type)==bow &&
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
    if(s<o.inventory_capacity && o.inventory[s].cursed) return false;
    if(is_bow(i.type)) return score_slot(prefs,o,best_weapon(prefs,o,true))<equipment_score(prefs,o,i);
    if(is_weapon(i.type) && o.equipped[0]<o.inventory_capacity && is_bow(o.inventory[o.equipped[0]].type)) s=best_weapon(prefs,o,false);
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
        case ItemCategory::Light: {
            int reserve=0;
            for(auto own:o.inventory) if(is_light(own.type)) reserve+=own.type==GLOWING_ORB ? 10000 : own.quantity*fuel_denominator(own.type);
            return prefs.resource_planning && reserve>=1000 ? 25 : 150;
        }
        case ItemCategory::Shield:
            return prefs.resource_planning && o.equipped[2]<o.inventory_capacity && shield_rating(o.inventory[o.equipped[2]].type)==3 ? 15 : 65;
        case ItemCategory::Weapon:
            return prefs.resource_planning && score_slot(prefs,o,best_weapon(prefs,o,false))>=40 ? 45 : prefs.unknown_gear_value;
        case ItemCategory::Armor:
            return prefs.resource_planning && o.equipped[1]<o.inventory_capacity && armor_definition(o.inventory[o.equipped[1]].type).rating>=5 ? 45 : prefs.unknown_gear_value;
        case ItemCategory::Ammo:return 50; default:return 0;
        }
    }
    if(is_light(i.type)) {
        int reserve=0,best=0;
        for(auto& own:o.inventory) if(is_light(own.type)) {
            best=std::max(best,equipment_score(prefs,o,own)); reserve+=own.type==GLOWING_ORB ? 10000 : own.quantity*fuel_denominator(own.type);
        }
        if(retention || equipment_score(prefs,o,i)>best) return 100+equipment_score(prefs,o,i);
        return reserve<600 ? std::min(100+equipment_score(prefs,o,i),120+equipment_score(prefs,o,i)/2) : 0;
    }
    if(is_weapon(i.type)||is_armor(i.type)||is_shield(i.type)||is_ring(i.type)||is_amulet(i.type))
        return retention || upgrade(prefs,o,i) ? 100+equipment_score(prefs,o,i) : 0;
    if(is_ammo(i.type)) return retention ? 170+std::min(24,int(i.quantity))*4 : quantity(o,ARROWS)>=24 ? 0 : best_weapon(prefs,o,true)<o.inventory_capacity ? 170 : 45;
    if(is_wand(i.type)) {
        if(i.individual && (i.modifier==WAND_CURSED || i.modifier==WAND_UNRELIABLE || !i.quantity)) return 0;
        return i.type==WAND_DIGGING ? 60 : 65+5*i.quantity;
    }
    switch(i.type) {
    case FOOD:return !retention && quantity(o,FOOD)>=prefs.food_stock_target ? 0 : std::max(retention ? 70+20*i.quantity : 70,o.hunger<60 ? 180 : 70);
    case POTION_EXTRA_HEALING:return 260;
    case POTION_RESTORE_VITALITY:return o.weakened ? 160 : 80;
    case POTION_HASTE:case POTION_LEVITATION:case POTION_FIRE_RESISTANCE:return 100;
    case POTION_FIRE:return 85;
    case SCROLL_HOLD_MONSTER:return 160; case SCROLL_MASS_TELEPORT:return 180;
    case SCROLL_DETECT_TRAPS:case SCROLL_DETECT_OBJECTS:return 120;
    case SCROLL_FIRE:return 45; case SCROLL_SUMMONING:return 0;
    case POTION_HEALING:return 240; case POTION_EXPERIENCE:return 220;
    case POTION_STRENGTH:return o.strength<12||o.weakened ? 160 : 40;
    case POTION_DEXTERITY:return o.dexterity<12 ? 150 : 40;
    case POTION_SEE_INVISIBLE:return (o.floor>=2 && o.floor<=4) ? 150 : 90;
    case POTION_PARALYSIS:return 100; case POTION_CONFUSION:case POTION_BLINDNESS:return 65;
    case POTION_SLOWING:case POTION_POISON:return 60; case POTION_HARMING:return 55; case POTION_INVISIBILITY:return 70;
    case SCROLL_IDENTIFY:return 170; case SCROLL_ENCHANT:return 190;
    case SCROLL_REMOVE_CURSE:return 100; case SCROLL_MAPPING:return 170;
    case SCROLL_TORMENT:return 95; case SCROLL_MASS_CONFUSE:return 100; case SCROLL_MASS_POISON:return 75;
    case SCROLL_FEAR:return 160; case SCROLL_TELEPORT:return 200; default:return 0;
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
    for(int s=0;s<o.inventory_capacity;++s) {
        const auto& i=o.inventory[s];
        if(i.category==ItemCategory::None) return true;
        bool stack=incoming.category==ItemCategory::Food||incoming.category==ItemCategory::Ammo||incoming.category==ItemCategory::Potion||incoming.category==ItemCategory::Scroll;
        if(stack && same_item(i,incoming) && int(i.quantity)+incoming.quantity<=(incoming.category==ItemCategory::Ammo ? 255 : 63)) return true;
    }
    return false;
}
int replacement(const ExplorerParameters& prefs,const ExplorerObservation& o,const SeenItem& incoming) {
    int worst=value(prefs,o,incoming)-10,slot=-1;
    int bow=best_weapon(prefs,o,true), melee=best_weapon(prefs,o,false);
    for(int s=0;s<o.inventory_capacity;++s) {
        auto i=o.inventory[s];
        if(equipped(o,s)||i.cursed || s==melee || s==bow) continue;
        if(prefs.resource_planning && (i.type==FOOD || i.type==POTION_HEALING || i.type==SCROLL_TELEPORT || i.type==SCROLL_FEAR) &&
            incoming.category!=ItemCategory::Food && incoming.type!=POTION_HEALING && incoming.type!=SCROLL_TELEPORT && incoming.type!=YENDOR_AMULET) continue;
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
    if(o.fire_resistance) return true;
    if(wand.modifier==WAND_SPREADING || wand.modifier==WAND_OVERPOWERED) return false;
    int r=wand.modifier==WAND_POWERFUL ? 2 : 1;
    for(int k=1;k<6;++k) if(o.equipped[k]<o.inventory_capacity) {
        auto i=o.inventory[o.equipped[k]];
        if((i.type==RING_FIRE_IMMUNITY || i.type==DRAGONHIDE) && !i.cursed) return true;
    }
    return std::abs(int(target.x)-o.player.x)>r || std::abs(int(target.y)-o.player.y)>r;
}
}
int expected_information(const ExplorerFloor& f,SeenPosition origin,int radius,bool obstruction) {
    radius=std::clamp(radius,0,6);
    int gain=0;
    for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
        int x=int(origin.x)+dx,y=int(origin.y)+dy;
        if(dx*dx+dy*dy>radius*radius || !inside(x,y)) continue;
        auto& target=f.tiles[y*EXPLORER_W+x];
        if(target.failures>=2 || (target.terrain!=Terrain::Unknown && (!passable(target.terrain)||target.surveyed))) continue;
        bool blocked=false;
        if(obstruction) {
            // Integer ray through memory only. Unknown cells are optimistic;
            // a known wall/closed door stops the ray before its destination.
            int atx=origin.x,aty=origin.y,ax=std::abs(dx),ay=std::abs(dy),error=ax-ay;
            int sx=dx<0 ? -1 : 1,sy=dy<0 ? -1 : 1;
            while(atx!=x || aty!=y) {
                int twice=2*error;
                if(twice>-ay) { error-=ay; atx+=sx; }
                if(twice<ax) { error+=ax; aty+=sy; }
                if(atx==x && aty==y) break;
                auto& tile=f.tiles[aty*EXPLORER_W+atx];
                if(tile.failures>=2 || tile.terrain==Terrain::Wall || tile.terrain==Terrain::ClosedDoor) { blocked=true; break; }
            }
        }
        if(!blocked) gain+=obstruction && target.terrain==Terrain::Unknown ? 2 : 1;
    }
    return gain;
}
SurvivalRisk estimate_risk(const ExplorerObservation& o,const ExplorerFloor& f,const ExplorerParameters& prefs) {
    SurvivalRisk r;
    int armor=0,enchant=0,speed=std::max(1,int(o.speed));
    bool fire=o.fire_resistance,clarity=false,iron=false,invisible=o.invisible!=0;
    for(int k=1;k<6;++k) if(o.equipped[k]<o.inventory_capacity) {
        auto i=o.inventory[o.equipped[k]];
        if(k==1) { armor+=armor_definition(i.type).rating; enchant+=i.enchant; }
        if(k==2) { armor+=shield_rating(i.type); enchant+=i.enchant; }
        if(i.type==RING_PROTECTION) armor+=i.cursed ? -1 : 1; // magnitude undisclosed
        if(!i.cursed) {
            fire=fire || i.type==RING_FIRE_IMMUNITY || i.type==DRAGONHIDE;
            clarity=clarity || i.type==AMULET_CLARITY;
            iron=iron || i.type==AMULET_IRONBLOOD;
            invisible=invisible || i.type==RING_INVISIBILITY;
            if(i.type==AMULET_SPEED) --speed; // lower bound on known benefit
            if(i.type==AMULET_PHOENIX_HEART || i.type==AMULET_HEART_OF_GIANT) speed-=2;
        }
    }
    speed=std::max(1,speed*(o.slowed ? 2 : 1)-(o.haste ? 1 : 0));
    int absorption=std::max(0,armor*3/4+std::clamp(enchant,-5,5)/2);
    auto add=[&](const SeenMonster& m,int d,bool uncertain) {
        if(!m.type || d>6) return;
        if(invisible && !(m.traits&MON_SEE_INVIS)) return;
        int ms=std::max(1,int(m.speed));
        int actions=std::clamp((speed*100+ms-1)/ms,25,400);
        int reach=(m.traits&MON_NOMOVE) ? 1 : 1+(actions+99)/100;
        int physical=std::max(1,int(m.strength)+2-absorption);
        int hit=(physical*actions*3+399)/400; // bounded 75% hit allowance
        int first=d<=reach ? hit : 0;
        int second=d<=reach+(actions+99)/100 ? hit : 0;
        if(d>1) first=first/2; // approach consumes some of the action budget
        if((m.traits&MON_FIRE_BREATH) && !fire) {
            bool aligned=m.pos.x==o.player.x || m.pos.y==o.player.y;
            if(aligned && d<=5) first=std::max(first,16);
            if(aligned || std::abs(int(m.pos.x)-o.player.x)<=1 || std::abs(int(m.pos.y)-o.player.y)<=1) second=std::max(second,16);
            r.fire=first>=16 || second>=16;
        }
        int special=0;
        if((m.traits&MON_PARALYZE_HIT) && !iron) special+=5;
        if((m.traits&MON_CONFUSE_HIT) && !clarity) special+=3;
        if(m.traits&(MON_POISON|MON_VAMPIRE|MON_REGENS)) ++special;
        if(uncertain) { first=(first+1)/2; second=(second+1)/2; }
        if(first || second) { ++r.attackers; r.one+=first; r.two+=first+second+special; }
    };
    for(int m=0;m<o.monster_count;++m) add(o.monsters[m],distance(o.player,o.monsters[m].pos),false);
    if(o.attacked && o.reported_attacker) {
        bool visible=false;
        for(int m=0;m<o.monster_count;++m) if(o.monsters[m].type==o.reported_attacker) visible=true;
        if(!visible) { add(o.reported_species,1,false); r.unseen=true; }
    }
    if(o.incoming_attack && !o.reported_attacker &&
        (prefs.contact_aware ? o.anonymous_attacks || !o.monster_count : !o.monster_count)) {
        // "Something hits/misses you" supplies neither species nor bearing.
        // Displayed HP loss is a lower bound after healing, with bounded padding.
        int damage=std::min(40,std::max(3,int(o.hp_loss)+2));
        int attacks=prefs.contact_aware ? std::clamp(int(o.anonymous_attacks),1,3) : 1;
        r.attackers+=attacks; r.one+=damage; r.two+=2*damage+3*attacks; r.unseen=true;
    } else if(prefs.contact_aware && o.anonymous_attacks) {
        int damage=std::min(40,std::max(3,int(o.hp_loss)+2));
        r.attackers+=std::min(3,int(o.anonymous_attacks)); r.one+=damage; r.two+=2*damage+3; r.unseen=true;
    }
    for(auto& m:f.enemies) if(m.type && m.seen<o.turns && o.turns<=m.seen+prefs.monster_memory_turns &&
        !sees_monster(o,m.pos) && distance(o.player,m.pos)<=3) add(m.species,distance(o.player,m.pos),true);
    r.status=o.confused || o.paralyzed || o.weakened || o.slowed;
    if(o.confused || o.paralyzed) { r.one=r.one*3/2; r.two=r.two*3/2; }
    for(int d=0;d<4;++d) {
        int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d];
        if(!inside(x,y)) continue;
        auto p=SeenPosition{uint8_t(x),uint8_t(y)}; auto& t=f.tiles[cell(p)];
        if(!passable(t.terrain)||t.terrain==Terrain::ClosedDoor||trap_penalty(prefs,t.feature)||sees_monster(o,p)) continue;
        bool safe=true;
        for(int m=0;m<o.monster_count;++m) if(distance(p,o.monsters[m].pos)<=1) safe=false;
        for(int m=0;m<o.monster_count;++m) if(distance(p,o.monsters[m].pos)<=(speed+std::max(1,int(o.monsters[m].speed))-1)/std::max(1,int(o.monsters[m].speed))) safe=false;
        if(safe && !r.unseen && r.attackers) r.retreat=true;
    }
    if(r.attackers && !r.retreat) r.two+=std::min(5,r.one/4);
    if(r.attackers) {
        r.danger=Danger::Caution;
        if(r.two*prefs.risk_margin_percent>=int(o.hp)*100 || o.hp<prefs.emergency_hp) r.danger=Danger::Serious;
        if(r.one>=o.hp) r.danger=Danger::Lethal;
    }
    return r;
}
void ExplorerAgent::reset() {
    for(auto& f:floors) { f.tiles.fill({}); f.enemies.fill({}); f.up=f.down={}; f.visited=false; f.ascent_layout=false; }
    identities={}; stats={}; active_floor=255; last_actions=last_turns=UINT64_MAX; last_experiment=last_mass_scroll=0;
    last_position=frontier=last_retreat=shadow={}; shadow_turn=0; probe_direction=0; previous={}; retreat_budget=quiet_turns=stalled=bow_cooldown=0;
    pending_ring=255;
    blocked_contact={}; blocked_contact_until=0;
    blocked_origin={}; invisible_contact={}; previous_healing=false;
    previous_hp=0; unseen_turns=contact_damage=0; last_offhand_switch=last_control=0; controlled_position={}; risk={};
    objective=deferred_objective=prior_position=recent_hazard={}; recent_positions.fill({});
    deferred_until=hazard_until=0; navigation_moves=objective_failures=frontier_stale=0;
}
void ExplorerAgent::remember_contact(const ExplorerObservation& o) {
    auto& c=invisible_contact;
    bool named_visible=false;
    for(int m=0;m<o.monster_count;++m) if(o.monsters[m].type==o.reported_attacker) named_visible=true;
    bool fresh=o.incoming_attack && (o.anonymous_attacks || !o.monster_count || (o.reported_attacker && !named_visible));
    bool contact=o.contact_reported && o.contact.x<64 && o.contact.y<32;
    bool unseen_contact=contact && !o.contact_type && !sees_monster(o,o.contact);
    if(c.state==ContactState::None && (fresh || unseen_contact)) {
        c={}; c.state=ContactState::Suspected; c.started=c.progress=c.threat=o.turns;
        c.observed_hp=o.hp;
    }
    if(c.state==ContactState::None) return;
    if(fresh || unseen_contact) c.threat=o.turns;
    c.recent_attacks=std::max<uint8_t>(o.anonymous_attacks,fresh ? 1 : 0);
    uint8_t loss=std::max(o.hp_loss,uint8_t(c.observed_hp>o.hp ? c.observed_hp-o.hp : 0));
    c.observed_hp=o.hp; c.damage+=loss;
    c.losses[o.actions%c.losses.size()]=loss;
    c.recent_damage=0; for(auto loss:c.losses) c.recent_damage+=loss;
    if(previous_healing && !o.last_rejected) ++c.healing;
    if(o.summoning_reported) ++c.summons;
    if(c.probe_origin!=o.player) {
        c.tested=0; c.probe_origin=o.player;
        // A just-vacated square is a tested empty hypothesis. Prevent immediate
        // reverse probes when a successful probe merely walked into empty space.
        if(previous.goal=="probe unseen contact candidates" && last_position.x!=255)
            for(int d=0;d<4;++d) if(int(last_position.x)==int(o.player.x)+dxs[d] &&
                int(last_position.y)==int(o.player.y)+dys[d]) c.tested|=uint8_t(1u<<d);
    }
    bool moved=last_position.x!=255 && last_position!=o.player;
    bool lost_strike=previous.kind==ExplorerActionKind::Move && previous.destination==c.square && !contact;
    if(lost_strike && !moved) for(int d=0;d<4;++d)
        if(previous.dx==dxs[d] && previous.dy==dys[d]) c.tested|=uint8_t(1u<<d);
    if(moved || lost_strike || o.confused || (c.square.x!=255 && distance(o.player,c.square)!=1)) {
        c.square={};
        if(c.state!=ContactState::Disengaging) c.state=ContactState::Suspected;
    } else if(c.state==ContactState::Confirmed && !contact) c.state=ContactState::Uncertain;
    if(o.melee_feedback==MeleeFeedback::Defeat) {
        ++c.defeats; c.progress=o.turns; c.misses=0; c.square={}; c.tested=0;
        if(c.state!=ContactState::Disengaging) c.state=fresh || o.summoning_reported || c.summons ? ContactState::Suspected : ContactState::None;
    } else if(unseen_contact && !o.confused &&
        (o.contact_origin.x==255 ? !moved : o.contact_origin==o.player) && distance(o.player,o.contact)==1) {
        c.square=o.contact; c.origin=o.player; c.confirmed=o.turns;
        c.feedback=o.melee_feedback; ++c.confirmations; c.tested=0;
        if(c.state==ContactState::Disengaging) {
            // Discovering a bearing during retreat changes which squares to
            // avoid, without changing the strategic decision to disengage.
            blocked_contact=c.square; blocked_contact_until=c.disengage_until;
        }
        if(o.melee_feedback==MeleeFeedback::Hit) { c.progress=o.turns; c.misses=0; }
        else ++c.misses; // A roll miss confirms presence, without proving damage.
        if(c.state!=ContactState::Disengaging) c.state=ContactState::Confirmed;
    }
    if(o.encounter_changed && c.square.x!=255 && c.state!=ContactState::Disengaging)
        c.state=ContactState::Uncertain;
    if(c.square.x!=255 && sees_monster(o,c.square)) {
        c.square={}; if(c.state!=ContactState::Disengaging) c.state=ContactState::Suspected;
    }
    if(c.square.x!=255 && o.turns>c.confirmed+3) c.square={};
    if(c.state==ContactState::Disengaging && o.turns>=c.disengage_until) {
        c.state=fresh ? ContactState::Suspected : ContactState::None;
        c.square={}; c.tested=0; c.started=c.progress=o.turns; c.damage=c.healing=c.defeats=c.summons=0;
    }
    // Completing a retreat ends the tactical episode. Avoidance remains a
    // separate route cooldown; it must not force sixty-four turns of wandering.
    if(o.turns>c.threat+6) c={};
    if(previous.goal=="probe unseen contact candidates" && !contact && !moved) {
        for(int d=0;d<4;++d) if(previous.dx==dxs[d] && previous.dy==dys[d]) c.tested|=uint8_t(1u<<d);
    }
}
void ExplorerAgent::remember(const ExplorerObservation& o) {
    const auto& prefs=parameters;
    auto& f=floors[o.floor];
    uint64_t revealed_before=stats.revealed;
    for(int g=0;g<6;++g) for(int a=0;a<EXPLORER_APPEARANCES;++a) if(o.identities[g][a]) identities[g][a]=o.identities[g][a];
    if(active_floor!=o.floor) {
        active_floor=o.floor; frontier=shadow={}; last_position={}; last_retreat={}; retreat_budget=quiet_turns=stalled=0;
        blocked_contact={}; blocked_contact_until=0;
        blocked_origin={}; invisible_contact={}; previous_healing=false;
        unseen_turns=contact_damage=0; previous_hp=o.hp; controlled_position={}; last_offhand_switch=0;
        objective=deferred_objective=prior_position=recent_hazard={}; recent_positions.fill({});
        deferred_until=hazard_until=0; navigation_moves=objective_failures=frontier_stale=0;
        // Production regenerates a DIFFERENT layout after Yendor; preserve history,
        // but never route through it as current known terrain.
        if(f.visited && f.ascent_layout!=o.yendor) {
            for(auto& t:f.tiles) { t.historical=t.terrain; t.terrain=Terrain::Unknown; t.feature=0; t.has_item=t.surveyed=false; t.visits=t.failures=t.hazard_entries=t.hazard_loss=t.survey_attempts=0; }
            f.up=f.down={};
        }
        f.enemies.fill({}); f.visited=true; f.ascent_layout=o.yendor;
    }
    bool changed=last_actions!=o.actions || last_turns!=o.turns || last_position!=o.player;
    if(changed) {
        bool unseen=o.incoming_attack && !o.monster_count;
        unseen_turns=unseen ? unseen_turns+1 : quiet_turns>3 ? 0 : unseen_turns;
        if(unseen && previous_hp>o.hp) contact_damage+=previous_hp-o.hp;
        if(last_position!=o.player) contact_damage=0;
        previous_hp=o.hp;
    }
    if(prefs.contact_aware) {
        if(changed) remember_contact(o);
        if(invisible_contact.state!=ContactState::None) {
            shadow=invisible_contact.square; shadow_turn=invisible_contact.confirmed;
        } else if(o.contact_reported && o.contact_type) {
            // Keep ordinary visible-combat action history behavior unchanged.
            shadow=o.contact; shadow_turn=o.turns;
        } else if(o.melee_feedback==MeleeFeedback::Defeat && invisible_contact.confirmations) shadow={};
        else if(o.turns>shadow_turn+3 || (previous.goal=="repeat reported melee contact" && !o.contact_reported)) shadow={};
    } else {
        if(o.contact_reported && o.contact.x<64 && o.contact.y<32) { shadow=o.contact; shadow_turn=o.turns; }
        else if(o.turns>shadow_turn+3 || (previous.goal=="repeat reported melee contact" && !o.contact_reported)) shadow={};
    }
    if(changed && last_position.x!=255 && previous.kind==ExplorerActionKind::Move && last_position==o.player &&
       (o.last_rejected || (!o.contact_reported && !o.incoming_attack && !o.confused && !o.paralyzed))) {
        int x=int(last_position.x)+previous.dx,y=int(last_position.y)+previous.dy;
        if(inside(x,y)) {
            auto& t=f.tiles[y*EXPLORER_W+x];
            if(o.last_rejected || t.terrain==Terrain::Unknown) {
                t.failures=uint8_t(std::min(255,int(t.failures)+1));
                ++stats.unreachable;
            }
        }
    }
    if(changed && last_position!=o.player) {
        if(prefs.hazard_routing && last_position.x!=255) {
            if(o.player==prior_position) ++stats.reversals;
            prior_position=last_position;
            auto& entered=f.tiles[cell(o.player)];
            if(entered.feature==FEATURE_SUMMONING_TRAP) {
                ++stats.known_crossings;
                if(entered.hazard_entries) ++stats.repeated_crossings;
                entered.hazard_entries=uint8_t(std::min(255,int(entered.hazard_entries)+1));
                recent_hazard=o.player; hazard_until=o.turns+24;
                if(previous.essential_hazard) ++stats.essential_crossings;
            }
            if(previous.navigation) {
                recent_positions[navigation_moves++%recent_positions.size()]=o.player;
                if(navigation_moves>=4) {
                    auto at=[&](unsigned ago) { return recent_positions[(navigation_moves-ago)%recent_positions.size()]; };
                    if(at(1)==at(3) && at(2)==at(4)) {
                        if(previous.goal=="explore known frontier" && objective.x!=255)
                            f.tiles[cell(objective)].survey_attempts=3;
                        deferred_objective=objective; deferred_until=o.turns+16; objective={};
                        ++stats.cycles; ++stats.goal_failures;
                    }
                }
            }
        }
        auto& t=f.tiles[cell(o.player)]; if(t.visits) ++stats.repeats;
        t.visits=uint16_t(std::min(65535,int(t.visits)+1)); stalled=0;
    } else if(changed) ++stalled;
    if(prefs.hazard_routing && changed && recent_hazard.x!=255 && o.turns<hazard_until && o.hp_loss) {
        auto& t=f.tiles[cell(recent_hazard)];
        t.hazard_loss=uint8_t(std::min(255,int(t.hazard_loss)+o.hp_loss));
    }
    if(prefs.hazard_routing && changed && o.last_rejected && previous.destination==objective && ++objective_failures>=3) {
        deferred_objective=objective; deferred_until=o.turns+16; objective={}; ++stats.goal_failures;
    }
    for(int n=0;n<EXPLORER_CELLS;++n) {
        auto& t=f.tiles[n];
        if(o.terrain[n]!=Terrain::Unknown) {
            if(t.terrain!=Terrain::Unknown && t.terrain!=o.terrain[n]) {
                t.survey_attempts=0;
                for(int d=0;d<4;++d) {
                    int x=n%EXPLORER_W+dxs[d], y=n/EXPLORER_W+dys[d];
                    if(inside(x,y)) f.tiles[y*EXPLORER_W+x].survey_attempts=0;
                }
            }
            if(t.terrain==Terrain::Unknown) ++stats.revealed;
            if(passable(o.terrain[n]) && t.terrain!=o.terrain[n]) t.failures=0;
            t.terrain=o.terrain[n]; t.feature=o.features[n];
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
    if(prefs.hazard_routing && changed) {
        if(stats.revealed>revealed_before) frontier_stale=0;
        else if(previous.navigation && (previous.goal=="explore known frontier" ||
                previous.goal=="seek remembered item sprite") && ++frontier_stale>=32) {
            // Longer cycles can evade the four-step reversal detector. Bound
            // attempts using only our own knowledge growth and action history.
            if(previous.destination.x!=255) {
                auto& target=f.tiles[cell(previous.destination)];
                target.survey_attempts=uint8_t(std::min(3,int(target.survey_attempts)+1));
                deferred_objective=previous.destination; deferred_until=o.turns+16;
            }
            objective={}; frontier_stale=0; ++stats.cycles; ++stats.goal_failures;
        }
        auto& current=f.tiles[cell(o.player)];
        if(stats.revealed>revealed_before) current.survey_attempts=0;
        else if(previous.goal=="explore known frontier" && previous.destination==o.player)
            current.survey_attempts=uint8_t(std::min(255,int(current.survey_attempts)+1));
    }
    for(int i=0;i<o.ground_count;++i) {
        auto& g=o.ground[i]; auto& t=f.tiles[cell(g.pos)];
        // A generic sprite does not erase a detailed description learned earlier.
        if(!t.has_item || t.item.category!=g.item.category || g.item.appearance!=255 || g.item.type) t.item=g.item;
        t.has_item=true;
    }
    for(int n=0;n<EXPLORER_CELLS;++n) if(o.detected_objects[n]) f.tiles[n].has_item=true;
    // Once revealed, a mimic's disguise is no longer a remembered loot goal.
    for(int m=0;m<o.monster_count;++m) if(o.monsters[m].type==MIMIC) {
        auto& t=f.tiles[cell(o.monsters[m].pos)]; t.has_item=false; t.item={};
    }
    for(auto& t:f.tiles) if(t.has_item && !t.item.type && t.item.appearance<EXPLORER_APPEARANCES)
        t.item.type=identities[identity_group(t.item.category)][t.item.appearance];
    bool reassess=prefs.invisible_response && unseen_turns>=4 && (contact_damage>=std::max(3,int(o.max_hp)/5) || o.hunger<40 || o.hp*2<o.max_hp);
    if(!prefs.contact_aware && changed && (stalled>=unsigned(prefs.stationary_contact_turns) || reassess) && o.contact_reported && distance(o.player,o.contact)==1) {
        // An accepted attack may still block travel indefinitely. Our own contact
        // square and lack of movement justify a temporary detour, without knowing
        // which creature is there now or where it originated.
        blocked_contact=o.contact; blocked_contact_until=o.turns+std::min(64,prefs.stationary_contact_turns);
        shadow={}; frontier={}; stalled=0; unseen_turns=contact_damage=0;
    }
    for(auto& m:f.enemies) if(m.type && (o.turns>m.seen+prefs.monster_memory_turns ||
        (o.turns>m.seen+2 && o.visible[cell(m.pos)] && !sees_monster(o,m.pos)))) m={};
    for(int i=0;i<o.monster_count;++i) {
        auto& s=o.monsters[i]; int slot=-1;
        for(int j=0;j<12;++j) if(f.enemies[j].type==s.type && distance(f.enemies[j].pos,s.pos)<=1) { slot=j; break; }
        if(slot<0) for(int j=0;j<12;++j) if(!f.enemies[j].type) { slot=j; break; }
        if(slot<0) slot=i;
        f.enemies[slot]={s.pos,s.type,o.turns,s};
    }
    if(changed) {
        quiet_turns=o.monster_count || o.attacked ? 0 : quiet_turns+1;
        if(quiet_turns>prefs.retreat_reset_quiet_turns) retreat_budget=0;
        if(bow_cooldown) --bow_cooldown;
    }
    last_position=o.player; last_actions=o.actions; last_turns=o.turns;
}
void ExplorerAgent::paths(const ExplorerObservation& o,bool hazards) {
    const auto& prefs=parameters;
    costs.fill(prefs.hazard_routing ? INT32_MAX/4 : 30000); steps.fill(-1); first.fill(-1); route_hazards.fill(255);
    auto& f=floors[o.floor]; int root=cell(o.player);
    using Node=std::tuple<unsigned,int,int>;
    // Retain the backing allocation between decisions; no per-turn allocation.
    thread_local std::vector<Node> storage;
    storage.clear(); storage.reserve(EXPLORER_CELLS*4);
    auto push=[&](Node n) { storage.push_back(n); std::push_heap(storage.begin(),storage.end(),std::greater<Node>()); };
    costs[root]=0; steps[root]=0; route_hazards[root]=0; push({0,0,root});
    while(!storage.empty()) {
        std::pop_heap(storage.begin(),storage.end(),std::greater<Node>());
        auto [crossings,cost,at]=storage.back(); storage.pop_back();
        if(cost!=costs[at] || crossings!=route_hazards[at]) continue;
        // Occupants are reachable combat endpoints, never transit squares.
        if(prefs.hazard_routing && !hazards && at!=root && sees_monster(o,position(at))) continue;
        for(int d=0;d<4;++d) {
            int x=at%EXPLORER_W+dxs[d],y=at/EXPLORER_W+dys[d];
            if(!inside(x,y)) continue;
            int n=y*EXPLORER_W+x; auto& t=f.tiles[n];
            if(!passable(t.terrain) || t.failures>=2) continue;
            bool trap=!o.levitation && t.feature==FEATURE_SUMMONING_TRAP;
            if(prefs.hazard_routing && trap && !hazards) continue;
            if(o.turns<blocked_contact_until && position(n)==blocked_contact) continue;
            if(prefs.contact_aware && o.turns<blocked_contact_until && position(n)==blocked_origin) continue;
            int penalty=(o.levitation ? 0 : trap_penalty(prefs,t.feature))+(t.terrain==Terrain::ClosedDoor ? 1 : 0);
            if(prefs.hazard_routing && hazards && trap)
                penalty+=20*std::min(10,int(t.hazard_entries))+2*int(t.hazard_loss);
            if(!prefs.hazard_routing && prefs.contact_aware && trap && t.visits)
                penalty+=1000*std::min(10,int(t.visits));
            for(auto& m:f.enemies) if(m.type>=ORC && o.turns<=m.seen+prefs.monster_memory_turns && distance(position(n),m.pos)<=1) penalty+=prefs.remembered_threat_cost;
            if(prefs.tactical_risk && risk.danger>=Danger::Serious) for(int m=0;m<o.monster_count;++m)
                if(distance(position(n),o.monsters[m].pos)<=2) penalty+=80;
            int next=cost+1+penalty;
            unsigned count=crossings+(prefs.hazard_routing && trap ? 1 : 0);
            if(next<(prefs.hazard_routing ? INT32_MAX/4 : 30000) &&
               (count<route_hazards[n] || (count==route_hazards[n] && next<costs[n]))) {
                // A simple route cannot contain more hazards than map cells.
                // Saturation is safe: 255 means no usable bounded route.
                if(count>=255) continue;
                costs[n]=next; steps[n]=int16_t(steps[at]+1); route_hazards[n]=uint8_t(count);
                first[n]=at==root ? int8_t(d) : first[at]; push({count,next,n});
            }
        }
    }
}
ExplorerAction ExplorerAgent::navigate(SeenPosition p,const char* goal) {
    if(p.x!=255 && first[cell(p)]>=0) {
        if(parameters.hazard_routing && objective!=p) {
            if(objective.x!=255) ++stats.goal_changes;
            objective=p; objective_failures=0;
        }
        auto a=move(first[cell(p)],goal,p); a.navigation=true; return a;
    }
    ExplorerAction a; a.goal="no reachable known route"; a.no_path=true; ++stats.unreachable; return a;
}
ExplorerAction ExplorerAgent::hazardous_progress(const ExplorerObservation& o,SeenPosition stairs) {
    ++stats.no_safe_route;
    paths(o,true); // Reuse all buffers; optional goals never use this pass.
    auto& f=floors[o.floor];
    int target=-1;
    if(stairs.x!=255 && steps[cell(stairs)]>0) target=cell(stairs);
    if(target<0 && previous.goal=="essential progression through known summoning hazard" &&
       objective.x!=255 && steps[cell(objective)]>0) target=cell(objective);
    // If stairs remain unknown, crossing toward a new survey is progression.
    if(target<0) for(int n=0;n<EXPLORER_CELLS;++n) {
        // The only remaining frontier may itself be a summoning trap. Excluding
        // that endpoint loses an otherwise legal route to unexplored terrain.
        if(steps[n]<=0) continue;
        bool frontier_needed=!f.tiles[n].surveyed;
        for(int d=0;d<4;++d) {
            int x=n%64+dxs[d],y=n/64+dys[d];
            if(inside(x,y) && f.tiles[y*64+x].terrain==Terrain::Unknown) frontier_needed=true;
        }
        if(frontier_needed && (target<0 || std::make_pair(route_hazards[n],costs[n])<
            std::make_pair(route_hazards[target],costs[target]))) target=n;
    }
    for(int n=0;n<EXPLORER_CELLS;++n) if(f.tiles[n].has_item && steps[n]>0 && route_hazards[n] &&
        position(n)!=deferred_objective) {
        deferred_objective=position(n); deferred_until=o.turns+24; ++stats.hazard_abandoned;
        break;
    }
    if(target<0) { ExplorerAction a; a.no_path=true; a.goal="no progression route through observed terrain"; return a; }
    // Recover or escape before an avoidable serious crossing. Exhausted quiet
    // players may still make the only progression attempt instead of deadlocking.
    if((risk.danger>=Danger::Serious || o.hp*2<o.max_hp) && quantity(o,POTION_HEALING))
        return use(find(o,POTION_HEALING),"heal before essential hazard crossing");
    if(risk.danger>=Danger::Serious && find(o,SCROLL_TELEPORT)>=0)
        return use(find(o,SCROLL_TELEPORT),"escape before essential hazard crossing");
    int direction=first[target];
    if(direction>=0) {
        SeenPosition next{uint8_t(int(o.player.x)+dxs[direction]),uint8_t(int(o.player.y)+dys[direction])};
        auto& t=f.tiles[cell(next)];
        if(t.feature==FEATURE_SUMMONING_TRAP && t.hazard_entries && next==recent_hazard && o.turns<hazard_until &&
           (risk.danger>=Danger::Serious || t.hazard_loss>=o.max_hp/2)) {
            ExplorerAction pause; pause.goal="reassess costly repeated essential hazard"; return pause;
        }
    }
    auto a=navigate(position(target),"essential progression through known summoning hazard");
    a.essential_hazard=route_hazards[target]!=0;
    // Entry is permitted only after all safe useful destinations were exhausted;
    // repeated/expensive activations do not license optional return trips.
    return a;
}
ExplorerAction ExplorerAgent::pickup(const ExplorerObservation& o) {
    const auto& prefs=parameters;
    int best=-1,worth=0;
    for(int i=0;i<o.ground_count;++i) {
        auto& g=o.ground[i]; int v=value(prefs,o,g.item);
        bool survival=g.item.type==FOOD || g.item.type==POTION_HEALING || is_light(g.item.type);
        if(g.pos!=o.player || g.handle>=24 || (o.yendor && g.item.type!=YENDOR_AMULET && !survival)) continue;
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
    auto o=incoming;
    // Reproduce the frozen policy's older feedback semantics for comparisons.
    if(!prefs.contact_aware && o.melee_feedback==MeleeFeedback::Defeat) { o.contact_reported=false; o.contact={}; }
    remember(o);
    // Keep a shield briefly when an observed nearby enemy drops out of sight
    // after changing offhand gear. This is uncertainty from our own sightings,
    // not a fresh enemy position, and never creates a combat target.
    for(auto& m:floors[o.floor].enemies) if(m.type && o.turns<=m.seen+prefs.shield_threat_memory_turns &&
        distance(o.player,m.pos)<=prefs.shield_contact_distance) o.attacked=true;
    auto decide=[&]() -> ExplorerAction {
        auto& f=floors[o.floor];
        auto risk_observation=o;
        auto& contact=invisible_contact;
        if(prefs.contact_aware && contact.state!=ContactState::None && o.turns<=contact.threat+3 && !o.incoming_attack) {
            risk_observation.incoming_attack=true;
            risk_observation.anonymous_attacks=std::max<uint8_t>(1,contact.recent_attacks);
            risk_observation.hp_loss=uint8_t(std::min(255u,contact.recent_damage/4));
        }
        risk=estimate_risk(risk_observation,f,prefs);
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
        if(o.incoming_attack || (o.attacked && o.reported_attacker)) threats=std::max(1,threats);
        // Finish an observed opponent instead of alternating between adjacent
        // species. This uses our own action history, never the target's live HP.
        if(previous.kind==ExplorerActionKind::Move && previous.goal=="melee visible monster")
            for(int m=0;m<o.monster_count;++m) if(o.monsters[m].pos==previous.destination && distance(o.player,o.monsters[m].pos)==1) adjacent=m;
        bool emergency=threats && (o.hp<prefs.emergency_hp || o.hp*100<o.max_hp*prefs.emergency_percent || threats>=prefs.crowd_threshold);
        if(dragon_threat && o.hp*100<o.max_hp*prefs.dragon_escape_percent) emergency=true;
        if(prefs.tactical_risk) emergency=risk.danger>=Danger::Serious;
        bool lethal=prefs.tactical_risk && risk.danger==Danger::Lethal;
        bool disengaging=prefs.contact_aware && contact.state==ContactState::Disengaging;
        if(prefs.contact_aware && contact.state!=ContactState::None && !disengaging) {
            bool hazard=false;
            for(int y=std::max(0,int(o.player.y)-2);y<=std::min(31,int(o.player.y)+2);++y)
                for(int x=std::max(0,int(o.player.x)-2);x<=std::min(63,int(o.player.x)+2);++x)
                    if(f.tiles[y*64+x].feature==FEATURE_SUMMONING_TRAP) hazard=true;
            bool unsafe=(emergency && (lethal || contact.recent_damage>=std::max(4,int(o.hp)/3) || risk.status || risk.attackers>1)) ||
                (o.hp*2<o.max_hp && contact.recent_damage>=unsigned(std::max(6,int(o.hp)/2)));
            bool futile=(contact.misses>=64 && o.turns>=contact.progress+64) ||
                (contact.healing>=2 && contact.damage>=unsigned(o.max_hp)*2) ||
                (o.hunger<40 && find(o,FOOD)<0 && o.turns>=contact.started+16) ||
                (o.turns>=contact.started+32 && (contact.summons>=2 || (hazard && contact.defeats>=3))) ||
                o.turns>=contact.started+512 || stalled>=1024;
                // Final guards, independent of contact refresh and death-driven
                // episode resets. Ordinary long favorable melee stays committed.
            if(unsafe || futile) {
                contact.state=ContactState::Disengaging; contact.disengage_until=o.turns+64;
                blocked_contact=contact.square; blocked_origin=o.player; blocked_contact_until=contact.disengage_until;
                frontier={}; disengaging=true;
            }
        }
        int sight=find(o,POTION_SEE_INVISIBLE);
        bool ring_sight=false;
        for(int k=4;k<6;++k) if(o.equipped[k]<o.inventory_capacity) {
            const auto& i=o.inventory[o.equipped[k]];
            if(i.type==RING_SEE_INVISIBLE && !i.cursed) ring_sight=true;
        }
        if(sight>=0 && !o.see_invisible && !o.blindness && !ring_sight &&
           (risk.unseen || o.reported_attacker==PHANTOM || (o.floor==4 && threats)))
            return use(sight,"temporary sight against invisible contact");
        if((o.weakened || o.vitality_drained) && find(o,POTION_RESTORE_VITALITY)>=0) return use(find(o,POTION_RESTORE_VITALITY),"restore vitality");
        if(threats && !o.haste && find(o,POTION_HASTE)>=0) return use(find(o,POTION_HASTE),"haste in observed combat");
        if(!o.fire_resistance && find(o,POTION_FIRE_RESISTANCE)>=0) for(int m=0;m<o.monster_count;++m)
            if(o.monsters[m].traits&MON_FIRE_BREATH) return use(find(o,POTION_FIRE_RESISTANCE),"resist observed fire threat");
        if((o.confused || o.slowed) && find(o,POTION_EXTRA_HEALING)>=0) return use(find(o,POTION_EXTRA_HEALING),"clear harmful statuses with extra healing");
        if(adjacent>=0 && find(o,SCROLL_FIRE)>=0 && fire_safe(o,SeenItem{},o.player) && o.monsters[adjacent].type!=DRAGON) return use(find(o,SCROLL_FIRE),"fire burst with immunity");
        int healing=find(o,POTION_HEALING);
        if(healing<0 || o.confused || o.slowed) { int extra=find(o,POTION_EXTRA_HEALING); if(extra>=0) healing=extra; }
        int xp=find(o,POTION_EXPERIENCE);
        int xp_gain=o.equipped[3]<o.inventory_capacity && o.inventory[o.equipped[3]].type==AMULET_WISDOM && o.inventory[o.equipped[3]].cursed ? 25 : 50;
        bool xp_recovers=xp>=0 && o.level<50 && o.xp+xp_gain>=4+o.level*3;
        int minimum_heal=healing>=0 && o.inventory[healing].type==POTION_EXTRA_HEALING ? (o.max_hp*3+3)/4 : o.max_hp/4;
        bool heal_survives=o.hp+minimum_heal>risk.one;
        // Escape/control precedes a heal that cannot survive the following turn.
        // Directional tools require a currently visible cardinal target and a
        // known, unobstructed ray; remembered/unseen coordinates never aim them.
        auto control=[&]() -> ExplorerAction {
            if(o.monster_count && o.turns>=last_control+prefs.control_retry_turns) {
                int mass=find(o,SCROLL_MASS_TELEPORT);
                if(mass>=0) { last_control=o.turns; return use(mass,"displace observed threats"); }
                int hold=find(o,SCROLL_HOLD_MONSTER);
                if(hold>=0) for(int m=0;m<o.monster_count;++m)
                    if(std::abs(int(o.monsters[m].pos.x)-o.player.x)<=3 && std::abs(int(o.monsters[m].pos.y)-o.player.y)<=3) { last_control=o.turns; return use(hold,"hold observed nearby threats"); }
            }
            int teleport=find(o,SCROLL_TELEPORT);
            if(teleport>=0 && (lethal || risk.unseen || risk.attackers>1 || o.confused)) return use(teleport,"escape lethal observed pressure");
            if(o.monster_count && o.turns>=last_control+prefs.control_retry_turns) {
                int fear=find(o,SCROLL_FEAR);
                if(fear>=0) { last_control=o.turns; return use(fear,"fear under serious observed pressure"); }
            }
            for(int m=0;m<o.monster_count;++m) {
                auto& target=o.monsters[m];
                if(o.turns<last_control+prefs.control_retry_turns && target.pos==controlled_position) continue;
                for(int d=0;d<4;++d) {
                    bool clear=false;
                    for(int step=1;step<=6;++step) {
                        int x=int(o.player.x)+dxs[d]*step,y=int(o.player.y)+dys[d]*step;
                        if(!inside(x,y)) break;
                        int n=y*EXPLORER_W+x;
                        if(!o.visible[n] || f.tiles[n].terrain==Terrain::Wall || f.tiles[n].terrain==Terrain::ClosedDoor) break;
                        if(SeenPosition{uint8_t(x),uint8_t(y)}==target.pos) { clear=true; break; }
                        if(sees_monster(o,{uint8_t(x),uint8_t(y)})) break;
                    }
                    if(!clear) continue;
                    for(int t:{POTION_PARALYSIS,POTION_CONFUSION,POTION_BLINDNESS,WAND_TELEPORT,WAND_FORCE,WAND_ICE,POTION_SLOWING}) {
                        int s=find(o,t); if(s<0) continue;
                        auto i=o.inventory[s];
                        // Match ordinary ranged validation. A known wand type
                        // can still have unexamined, depleted instances; these
                        // belong to the bounded experiment path, not emergency
                        // retries that alternate targets without advancing time.
                        if(is_wand(t) && (!i.individual || !i.quantity || i.modifier==WAND_CURSED || i.modifier==WAND_UNRELIABLE)) continue;
                        ExplorerAction a; a.kind=is_wand(t) ? ExplorerActionKind::Wand : ExplorerActionKind::Throw;
                        a.slot=uint8_t(s); a.dx=dxs[d]; a.dy=dys[d]; a.destination=target.pos; a.goal="disable visible survival threat";
                        if(is_wand(t) && i.individual && (i.modifier==WAND_SPREADING || i.modifier==WAND_OVERPOWERED)) a.dx=a.dy=0;
                        last_control=o.turns; controlled_position=target.pos; return a;
                    }
                }
            }
            if(teleport>=0) return use(teleport,"escape serious observed pressure");
            bool sees_invisible=risk.unseen;
            for(int m=0;m<o.monster_count;++m) if(o.monsters[m].traits&MON_SEE_INVIS) sees_invisible=true;
            int invis=find(o,POTION_INVISIBILITY);
            if(invis>=0 && !o.invisible && !sees_invisible) return use(invis,"conceal from observed attackers");
            return {};
        };
        if(prefs.context_healing && (emergency || disengaging) && (lethal || !heal_survives || healing<0 || risk.status || risk.attackers>1 || disengaging)) {
            auto a=control(); if(a.kind!=ExplorerActionKind::Wait) return a;
        }
        if(healing>=0) {
            bool heal= o.hp*100<o.max_hp*prefs.healing_percent || (threats && o.hp*100<o.max_hp*prefs.combat_healing_percent);
            if(prefs.context_healing) {
                int threshold=adjacent>=0 || risk.unseen ? prefs.combat_healing_percent : risk.attackers ? prefs.approach_healing_percent : prefs.healing_percent;
                heal=(o.hp*100<o.max_hp*threshold || (emergency && heal_survives) || (o.weakened && risk.attackers)) &&
                    (o.max_hp-o.hp>=std::max(2,minimum_heal) || (o.weakened && risk.attackers));
            }
            if(heal) return use(healing,"known healing");
        }
        int strength=find(o,POTION_STRENGTH);
        if(o.weakened && strength>=0) return use(strength,"restore strength");
        if(xp>=0 && (!prefs.resource_planning || o.level<5 || (xp_recovers && (o.hp*4<o.max_hp*3 || emergency)))) return use(xp,"gain levels and recover HP");
        if((mass_target || threats>=prefs.crowd_threshold) && o.turns>=last_mass_scroll+prefs.mass_scroll_cooldown) {
            for(int t:{SCROLL_MASS_CONFUSE,SCROLL_TORMENT,SCROLL_MASS_POISON}) {
                int s=find(o,t); if(s>=0) { last_mass_scroll=o.turns; return use(s,"control observed dangerous group"); }
            }
        }
        if(emergency || disengaging) {
            if(prefs.context_healing) { auto a=control(); if(a.kind!=ExplorerActionKind::Wait) return a; }
            for(int t:{SCROLL_FEAR,SCROLL_TELEPORT,POTION_INVISIBILITY}) {
                if(prefs.context_healing) break;
                if(t==SCROLL_FEAR && !o.monster_count) continue;
                int s=find(o,t); if(s>=0 && (t!=POTION_INVISIBILITY || !o.invisible)) return use(s,"emergency control or escape");
            }
        }
        int food=find(o,FOOD);
        if(food>=0 && o.hunger<prefs.food_threshold) return use(food,"maintain hunger");
        if(o.turns<blocked_contact_until && (distance(o.player,blocked_contact)==1 || (prefs.contact_aware && disengaging))) {
            int best=-1,visits=256;
            for(int d=0;d<4;++d) {
                int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d]; if(!inside(x,y)) continue;
                auto p=SeenPosition{uint8_t(x),uint8_t(y)}; auto& t=f.tiles[cell(p)];
                bool safe=true;
                if(prefs.contact_aware) for(int m=0;m<o.monster_count;++m) if(distance(p,o.monsters[m].pos)<=1) safe=false;
                if(p!=blocked_contact && p!=blocked_origin && (!prefs.contact_aware || p!=shadow) && passable(t.terrain) && (!prefs.contact_aware || t.terrain!=Terrain::ClosedDoor) && !t.feature && !sees_monster(o,p) && safe && int(t.visits)<visits) {
                    best=d; visits=t.visits;
                }
            }
            if(best>=0 && (!prefs.contact_aware || retreat_budget<unsigned(prefs.retreat_steps))) {
                if(prefs.contact_aware) { ++retreat_budget; last_retreat=o.player; }
                return move(best,prefs.contact_aware ? "disengage from unseen encounter" : "detour around repeated melee contact",{uint8_t(int(o.player.x)+dxs[best]),uint8_t(int(o.player.y)+dys[best])});
            }
            // A corridor may offer no retreat. Keep defending the plausible
            // contact rather than waiting to die while an avoidance timer runs.
            if(!prefs.contact_aware) { ExplorerAction a; a.goal="wait for observed contact detour"; return a; }
        }
        if(pending_ring<o.inventory_capacity) {
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
        for(int s=0;s<o.inventory_capacity;++s) {
            auto i=o.inventory[s];
            int old=equipment_target(prefs,o,i);
            bool experiment=!i.type && (i.category==ItemCategory::Ring || i.category==ItemCategory::Amulet) && !threats &&
                quiet_turns>=prefs.jewelry_quiet_turns && o.hp*100>=o.max_hp*prefs.jewelry_experiment_health_percent &&
                (old==255 || !o.inventory[old].cursed) && score_slot(prefs,o,old)<=prefs.jewelry_experiment_max_score;
            if(equipped(o,s) || is_bow(i.type) || (!experiment && !upgrade(prefs,o,i))) continue;
            bool equipment=i.category==ItemCategory::Weapon||i.category==ItemCategory::Armor||i.category==ItemCategory::Light||i.category==ItemCategory::Shield||i.category==ItemCategory::Ring||i.category==ItemCategory::Amulet;
            if(!equipment) continue;
            bool offhand=i.category==ItemCategory::Light || i.category==ItemCategory::Shield;
            bool urgent_shield=offhand && is_shield(i.type) && (risk.unseen || risk.danger>=Danger::Serious);
            if(offhand && !urgent_shield && prefs.offhand_mode && old<o.inventory_capacity && score_slot(prefs,o,old)>0 && o.inventory[old].category!=ItemCategory::None &&
                (o.turns<last_offhand_switch+prefs.offhand_switch_turns || equipment_score(prefs,o,i)-score_slot(prefs,o,old)<prefs.offhand_switch_margin)) continue;
            // A switch costs a real enemy turn. Only a useful defensive upgrade
            // is considered in active combat; marginal jewelry waits for safety.
            if(prefs.tactical_risk && risk.danger>=Danger::Serious &&
                i.category!=ItemCategory::Weapon && i.category!=ItemCategory::Armor && i.category!=ItemCategory::Shield &&
                i.type!=RING_SEE_INVISIBLE && i.type!=RING_FIRE_IMMUNITY && i.type!=AMULET_CLARITY) continue;
            if(!i.type && (threats || quiet_turns<prefs.jewelry_quiet_turns)) continue;
            int gain=equipment_score(prefs,o,i)-score_slot(prefs,o,old);
            if(experiment) gain=std::max(1,gain);
            if(gain>upgrade_gain) { upgrade_gain=gain; upgrade_slot=s; }
        }
        if(upgrade_slot>=0) {
            int s=upgrade_slot; auto i=o.inventory[s]; int old=equipment_target(prefs,o,i);
            if(i.category==ItemCategory::Light || i.category==ItemCategory::Shield) last_offhand_switch=o.turns;
            if(i.category==ItemCategory::Ring && o.equipped[4]<o.inventory_capacity && o.equipped[5]<o.inventory_capacity) {
                if(!o.inventory[old].cursed) { pending_ring=uint8_t(s); return use(old,"remove weaker known ring"); }
            }
            return use(s,i.type ? "equip observed upgrade" : "try unknown jewelry");
        }
        int melee=best_weapon(prefs,o,false);
        if((adjacent>=0 || (shadow.x!=255 && distance(shadow,o.player)==1)) &&
            o.equipped[0]<o.inventory_capacity && is_bow(o.inventory[o.equipped[0]].type) && melee<o.inventory_capacity && !o.inventory[o.equipped[0]].cursed) {
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
                for(int s=0;s<o.inventory_capacity;++s) if(o.inventory[s].category==ItemCategory::Wand && !o.inventory[s].individual) {
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
            for(int t:{POTION_PARALYSIS,POTION_CONFUSION,POTION_BLINDNESS,POTION_SLOWING,POTION_POISON,WAND_ICE,WAND_STRIKING,WAND_FIRE,POTION_HARMING,POTION_FIRE,WAND_TELEPORT,WAND_FORCE,WAND_POLYMORPH}) {
                int s=find(o,t); if(s<0 || !danger) continue;
                auto i=o.inventory[s];
                if(is_wand(t) && (!i.individual || !i.quantity || i.modifier==WAND_CURSED || i.modifier==WAND_UNRELIABLE)) continue;
                if((t==WAND_FIRE || t==POTION_FIRE) && (monster.type==DRAGON || !fire_safe(o,i,monster.pos))) continue;
                if((t==WAND_TELEPORT || t==WAND_FORCE || t==WAND_POLYMORPH) && !emergency) continue;
                if(t==WAND_POLYMORPH && (monster.type==LORD || monster.type<INCUBUS)) continue;
                if(is_potion(t) && t!=POTION_HARMING && t!=POTION_FIRE && !emergency && monster.type<INCUBUS) continue;
                if(prefs.context_healing && is_potion(t) && t!=POTION_HARMING && t!=POTION_FIRE && o.turns<last_control+prefs.control_retry_turns && monster.pos==controlled_position) continue;
                ExplorerAction a; a.kind=is_wand(t) ? ExplorerActionKind::Wand : ExplorerActionKind::Throw;
                a.slot=uint8_t(s); a.dx=dxs[d]; a.dy=dys[d]; a.goal="target visible monster"; a.destination=monster.pos;
                if(is_wand(t) && (i.modifier==WAND_SPREADING || i.modifier==WAND_OVERPOWERED)) a.dx=a.dy=0;
                if(is_potion(t) && t!=POTION_HARMING && t!=POTION_FIRE) { last_control=o.turns; controlled_position=monster.pos; }
                return a;
            }
            int bow=best_weapon(prefs,o,true);
            if(bow<o.inventory_capacity && quantity(o,ARROWS) && !bow_cooldown && adjacent<0 && (range>=3 || o.equipped[0]==bow) && range<=ranged_weapon_definition(o.inventory[bow].type).range) {
                if(o.equipped[0]!=bow) {
                    if(o.equipped[0]<o.inventory_capacity && o.inventory[o.equipped[0]].cursed) continue;
                    return use(bow,"prepare visible ranged shot");
                }
                ExplorerAction a; a.kind=ExplorerActionKind::Throw; a.slot=uint8_t(find(o,ARROWS));
                a.dx=dxs[d]; a.dy=dys[d]; a.goal="shoot visible monster"; a.destination=monster.pos; return a;
            }
        }
        if(o.equipped[0]<o.inventory_capacity && is_bow(o.inventory[o.equipped[0]].type) && melee<o.inventory_capacity && !o.inventory[o.equipped[0]].cursed) {
            bow_cooldown=4; return use(melee,"restore melee weapon");
        }
        if(adjacent>=0 || (prefs.tactical_risk && emergency && nearest<=3) || (prefs.invisible_response && risk.unseen && unseen_turns>=4)) {
            if(emergency && retreat_budget<prefs.retreat_steps) {
                int best=-1,best_score=-10000;
                for(int d=0;d<4;++d) {
                    int x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d];
                    if(!inside(x,y)) continue;
                    auto p=SeenPosition{uint8_t(x),uint8_t(y)}; auto& t=f.tiles[cell(p)];
                    if(!passable(t.terrain) || t.terrain==Terrain::ClosedDoor || t.feature || sees_monster(o,p)) continue;
                    int separation=100,connections=0;
                    for(int m=0;m<o.monster_count;++m) separation=std::min(separation,distance(p,o.monsters[m].pos));
                    for(int k=0;k<4;++k) if(inside(x+dxs[k],y+dys[k]) && passable(f.tiles[(y+dys[k])*EXPLORER_W+x+dxs[k]].terrain)) ++connections;
                    int score=separation*prefs.retreat_separation_weight+connections*prefs.retreat_connectivity_weight-(p==last_retreat ? prefs.retreat_reversal_cost : 0);
                    bool faster=false;
                    for(int m=0;m<o.monster_count;++m) if(o.monsters[m].speed<o.speed) faster=true;
                    if((separation>nearest || (risk.unseen && p!=shadow)) && !faster && connections>=prefs.retreat_connections && score>best_score) { best=d; best_score=score; }
                }
                if(best>=0) { ++retreat_budget; last_retreat=o.player; return move(best,"bounded observed retreat"); }
            }
            if(adjacent>=0) {
                auto p=o.monsters[adjacent].pos;
                for(int d=0;d<4;++d) if(int(p.x)==int(o.player.x)+dxs[d] && int(p.y)==int(o.player.y)+dys[d]) return move(d,"melee visible monster",p);
            }
        }
        if(shadow.x!=255 && distance(shadow,o.player)==1 && o.turns<=shadow_turn+3 && (!prefs.contact_aware || !o.confused)) {
            for(int d=0;d<4;++d) if(int(shadow.x)==int(o.player.x)+dxs[d] && int(shadow.y)==int(o.player.y)+dys[d]) return move(d,"repeat reported melee contact",shadow);
        }
        if(prefs.contact_aware && contact.state!=ContactState::None && !disengaging && !o.confused && o.turns<=contact.threat+6) {
            // Known traversable hypotheses first, unknown terrain only as a
            // labelled tactile probe. Closed doors/walls/hazards are not melee probes.
            for(int unknown=0;unknown<2;++unknown) for(int k=0;k<4;++k) {
                int d=(contact.next_direction+k)%4,x=int(o.player.x)+dxs[d],y=int(o.player.y)+dys[d];
                if(!inside(x,y) || (contact.tested&(1u<<d))) continue;
                auto& t=f.tiles[y*64+x]; auto p=SeenPosition{uint8_t(x),uint8_t(y)};
                if(t.terrain==Terrain::Wall || t.terrain==Terrain::ClosedDoor || t.feature || sees_monster(o,p)) continue;
                if((t.terrain==Terrain::Unknown)!=bool(unknown)) continue;
                contact.tested|=uint8_t(1u<<d); contact.next_direction=uint8_t((d+1)%4); ++contact.probes;
                return move(d,"probe unseen contact candidates",p);
            }
            // Exhausting a local candidate set is not fresh evidence to repeat
            // it. Explore/reposition for a bounded interval before another set.
            contact.state=ContactState::Disengaging; contact.disengage_until=o.turns+8;
            blocked_origin=o.player; blocked_contact_until=contact.disengage_until;
        }
        if(!prefs.contact_aware && o.incoming_attack && !o.monster_count && !o.confused) {
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
            if(find(o,SCROLL_DETECT_TRAPS)>=0) return use(find(o,SCROLL_DETECT_TRAPS),"discover traps before exploration");
            if(find(o,SCROLL_DETECT_OBJECTS)>=0) return use(find(o,SCROLL_DETECT_OBJECTS),"inspect object positions");
            if(!o.levitation && find(o,POTION_LEVITATION)>=0) for(int d=0;d<4;++d) {
                int x=int(o.player.x)+dxs[d], y=int(o.player.y)+dys[d];
                if(inside(x,y) && feature_is_trap(f.tiles[y*64+x].feature)) return use(find(o,POTION_LEVITATION),"float over observed traps");
            }
            for(int t:{POTION_STRENGTH,POTION_DEXTERITY}) { int s=find(o,t); if(s>=0 && (t==POTION_STRENGTH ? o.strength : o.dexterity)<12) return use(s,"permanent stats"); }
            int unknown=-1,unknown_priority=-1;
            for(int s=0;s<o.inventory_capacity;++s) if(o.inventory[s].category!=ItemCategory::None && (!o.inventory[s].type || (!o.inventory[s].individual && o.inventory[s].category==ItemCategory::Wand))) {
                auto category=o.inventory[s].category;
                int priority=category==ItemCategory::Amulet ? 3 : category==ItemCategory::Ring ? 2 : category==ItemCategory::Wand ? 1 : 0;
                if(priority>unknown_priority) { unknown=s; unknown_priority=priority; }
            }
            int identify=find(o,SCROLL_IDENTIFY);
            if(unknown>=0 && identify>=0) return use(identify,"identify before experimenting",unknown);
            int uncurse=find(o,SCROLL_REMOVE_CURSE);
            for(int s=0;s<o.inventory_capacity;++s) if(o.inventory[s].cursed && uncurse>=0) return use(uncurse,"remove known curse",s);
            int enchant=find(o,SCROLL_ENCHANT);
            if(enchant>=0) {
                for(int k:{1,0,3,4,5}) if(o.equipped[k]<o.inventory_capacity && !o.inventory[o.equipped[k]].cursed) return use(enchant,"enchant equipped item",o.equipped[k]);
            }
            int mapping=find(o,SCROLL_MAPPING);
            if(mapping>=0 && (o.yendor ? f.up.x==255 : f.down.x==255)) return use(mapping,"legitimate magic mapping");
            if(!o.yendor && quiet_turns>=prefs.scroll_quiet_turns && o.hp>=prefs.scroll_min_hp && !o.blindness && !o.confused && o.turns>=last_experiment+prefs.scroll_interval) {
                for(int s=0;s<o.inventory_capacity;++s) if(!o.inventory[s].type && o.inventory[s].category==ItemCategory::Scroll) {
                    last_experiment=o.turns;
                    int target=unknown>=0 && unknown!=s ? unknown : o.equipped[1]<o.inventory_capacity ? o.equipped[1] : o.equipped[0];
                    return use(s,"safe unknown scroll experiment",target);
                }
            }
            if(!o.yendor && quiet_turns>=prefs.experiment_quiet_turns && o.hp>=prefs.potion_min_hp && o.hp*100>=o.max_hp*prefs.potion_health_percent && !o.blindness && !o.confused && !o.slowed && o.turns>=last_experiment+prefs.experiment_interval) {
                for(int s=0;s<o.inventory_capacity;++s) if(!o.inventory[s].type && o.inventory[s].category==ItemCategory::Potion) {
                    last_experiment=o.turns;
                    int target=unknown>=0 && unknown!=s ? unknown : o.equipped[1]<o.inventory_capacity ? o.equipped[1] : o.equipped[0];
                    return use(s,"bounded safe consumable experiment",target);
                }
            }
        }
        if(o.blindness) { ExplorerAction a; a.goal="wait out blindness using retained memory"; return a; }
        paths(o);
        SeenPosition stairs=o.yendor ? f.up : f.down;
        if(prefs.resource_planning && (o.hp*4<o.max_hp*3 || o.weakened) && !risk.attackers) {
            int recovery=-1,best_cost=INT32_MAX;
            for(int n=0;n<EXPLORER_CELLS;++n) {
                auto& t=f.tiles[n];
                bool spring=t.feature==FEATURE_HEALING_SPRING;
                bool fountain=t.feature==FEATURE_FOUNTAIN && o.hp>=10 && t.visits<2;
                if((spring || fountain) && steps[n]>0 && steps[n]<=prefs.recovery_detour_distance &&
                    costs[n]<best_cost && (!o.yendor || steps[n]<=12)) { recovery=n; best_cost=costs[n]; }
            }
            if(recovery>=0) return navigate(position(recovery),"seek observed recovery feature");
        }
        bool leave=prefs.resource_planning && !o.yendor && stairs.x!=255 &&
            (o.hp*100<o.max_hp*45 || (o.hunger<60 && !quantity(o,FOOD)));
        if(leave && steps[cell(stairs)]>=0) {
            if(stairs==o.player) { ExplorerAction a; a.kind=ExplorerActionKind::Stairs; a.goal="leave known floor with depleted reserves"; return a; }
            return navigate(stairs,"leave known floor with depleted reserves");
        }
        if(o.yendor && o.hunger<prefs.ascent_food_seek_hunger && quantity(o,FOOD)<prefs.ascent_food_reserve_units) {
            int food=-1,best_cost=INT32_MAX;
            for(int n=0;n<EXPLORER_CELLS;++n) if(f.tiles[n].has_item && f.tiles[n].item.category==ItemCategory::Food &&
                steps[n]>0 && steps[n]<=prefs.ascent_food_detour_distance && costs[n]<best_cost &&
                (fits(o,f.tiles[n].item) || replacement(prefs,o,f.tiles[n].item)>=0)) { food=n; best_cost=costs[n]; }
            if(food>=0) return navigate(position(food),"collect remembered food needed for ascent");
        }
        if(stairs==o.player && o.yendor) { ExplorerAction a; a.kind=ExplorerActionKind::Stairs; a.goal="ascend and escape"; return a; }
        if(o.yendor && stairs.x!=255 && steps[cell(stairs)]>=0) return navigate(stairs,"return to discovered up stairs");
        // Observable early experience opportunities, without chasing hidden enemies.
        if(!o.yendor && risk.danger<=Danger::Caution && o.floor<prefs.early_xp_floors && o.level<prefs.early_xp_level && o.hp*100>=o.max_hp*prefs.early_xp_health_percent) {
            for(int m=0;m<o.monster_count;++m) {
                auto& s=o.monsters[m];
                bool favorable=!prefs.tactical_risk || s.type<=RATTLESNAKE || score_slot(prefs,o,best_weapon(prefs,o,false))>=32;
                if(favorable && s.type>=SNAKE && s.type<=GOBLIN && steps[cell(s.pos)]>0 && steps[cell(s.pos)]<=prefs.early_xp_distance)
                    return navigate(s.pos,"approach visible early experience");
            }
        }
        int loot=-1,loot_score=0;
        if(!o.yendor) for(int n=0;n<EXPLORER_CELLS;++n) {
            auto& t=f.tiles[n]; if(!t.has_item || steps[n]<=0 || steps[n]>prefs.loot_max_distance || t.visits>=prefs.loot_max_visits || t.survey_attempts>=3) continue;
            if(prefs.hazard_routing && position(n)==deferred_objective && o.turns<deferred_until) continue;
            if(prefs.resource_planning && stairs.x!=255 && steps[n]>24) continue;
            if(prefs.resource_planning && stairs.x!=255 && steps[n]>12 &&
                (o.hp*2<o.max_hp || o.hunger<80) && t.item.category!=ItemCategory::Food && t.item.type!=POTION_HEALING) continue;
            if(!fits(o,t.item) && replacement(prefs,o,t.item)<0) continue;
            int score=(t.item.category==ItemCategory::None ? 40 : value(prefs,o,t.item))*100/(costs[n]+prefs.loot_distance_offset)-int(t.visits)*prefs.loot_visit_cost;
            if(prefs.hazard_routing && position(n)==objective && score>0) score+=std::max(1,score/5);
            if(score>loot_score) { loot=n; loot_score=score; }
        }
        if(loot>=0 && (stairs.x==255 || o.hunger>=60 || steps[loot]<=4)) return navigate(position(loot),"seek remembered item sprite");
        // Compare discovered stairs with observable frontier gain. Runtime weights
        // can favor gathering resources before descent, without unseen loot targets.
        if(!o.radius) for(int d=0;d<4;++d) {
            int x=int(o.player.x)+dxs[d], y=int(o.player.y)+dys[d];
            if(inside(x,y) && f.tiles[y*EXPLORER_W+x].terrain==Terrain::Unknown &&
               f.tiles[y*EXPLORER_W+x].failures<2) {
                ++stats.exploring_actions;
                return move(d,"explicit tactile probe at radius zero");
            }
        }
        int best=-1,best_score=-100000;
        int radius=std::max(1,int(o.radius));
        for(int n=0;n<EXPLORER_CELLS;++n) {
            auto& t=f.tiles[n]; if(steps[n]<=0 || !passable(t.terrain) || t.failures>=2) continue;
            if(prefs.hazard_routing && t.survey_attempts>=3) continue;
            if(prefs.hazard_routing && position(n)==deferred_objective && o.turns<deferred_until) continue;
            int x=n%EXPLORER_W,y=n/EXPLORER_W,adjacent_unknown=0,gain=0;
            auto needs_survey=[&](int at) { auto& tile=f.tiles[at]; return tile.failures<2 && (tile.terrain==Terrain::Unknown || (passable(tile.terrain) && !tile.surveyed)); };
            for(int d=0;d<4;++d) if(inside(x+dxs[d],y+dys[d]) && needs_survey((y+dys[d])*EXPLORER_W+x+dxs[d])) ++adjacent_unknown;
            if(!t.surveyed) ++adjacent_unknown;
            if(!adjacent_unknown) continue;
            gain=expected_information(f,position(n),radius,prefs.obstruction_gain!=0);
            if(!gain) continue;
            int score=prefs.frontier_adjacent_weight*adjacent_unknown+prefs.frontier_gain_weight*gain-prefs.frontier_distance_cost*costs[n]-prefs.frontier_visit_cost*t.visits;
            if(prefs.obstruction_gain) score+=prefs.frontier_gain_weight*gain*8/(steps[n]+4);
            if(position(n)==frontier) score+=prefs.frontier_hold_bonus;
            if(prefs.hazard_routing && position(n)==objective) score+=std::max(prefs.frontier_hold_bonus,std::abs(score)/5);
            if(o.hunger<40) score-=steps[n]*prefs.hunger_distance_cost;
            if(score>best_score) { best=n; best_score=score; }
        }
        if(stairs.x!=255 && steps[cell(stairs)]>=0) {
            int stair_score=prefs.stair_base_score-prefs.stair_distance_cost*costs[cell(stairs)];
            if(o.hp*2<o.max_hp || o.hunger<40) stair_score+=prefs.stair_low_health_bonus;
            if(prefs.resource_planning && o.hp*4>=o.max_hp*3 && o.hunger>=80 &&
                (quantity(o,FOOD)<std::min(prefs.food_stock_target,(31-int(o.floor))/2) || !quantity(o,POTION_HEALING))) stair_score-=700;
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
                if(t.terrain!=Terrain::Wall && t.failures<2 && t.visits<visits &&
                   (!prefs.hazard_routing || t.feature!=FEATURE_SUMMONING_TRAP)) { best_probe=d; visits=t.visits; }
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
        if(prefs.hazard_routing) return hazardous_progress(o,stairs);
        a.no_path=true; a.goal="exploration exhausted: no known stairs or reachable frontier"; return a;
    };
    auto a=decide();
    previous_healing=a.kind==ExplorerActionKind::Use && a.slot<o.inventory_capacity &&
        (o.inventory[a.slot].type==POTION_HEALING || o.inventory[a.slot].type==POTION_EXPERIENCE);
    previous=a; return a;
}
SeenFeatureChoice ExplorerAgent::choose_feature(const ExplorerObservation& o) const {
    const auto& prefs=parameters;
    SeenFeatureChoice choice;
    int type=o.features[cell(o.player)];
    if(type<FEATURE_FOUNTAIN || type>FEATURE_ALCHEMICAL_POOL || o.paralyzed) return choice;
    if(floors[o.floor].tiles[cell(o.player)].visits>1 && type!=FEATURE_HEALING_SPRING) return choice;
    bool pressure=o.attacked;
    for(int m=0;m<o.monster_count;++m) if(distance(o.monsters[m].pos,o.player)<=2) pressure=true;
    if(type==FEATURE_HEALING_SPRING) choice.accept=o.hp<o.max_hp || o.weakened || o.confused || o.slowed;
    else if(type==FEATURE_FOUNTAIN) choice.accept=o.hp>=10 && (o.hp<o.max_hp || o.hunger<180 || o.weakened);
    else choice.accept=!pressure && o.hp>=10;
    if(type==FEATURE_ENCHANTED_FORGE || type==FEATURE_ALCHEMICAL_POOL) {
        int best=-1;
        for(int s=0;s<o.inventory_capacity;++s) {
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
