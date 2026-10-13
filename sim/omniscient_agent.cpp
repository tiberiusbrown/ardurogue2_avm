#include "agent.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "spells.hpp"
#include "spell_preferences.hpp"
#include "sim_hooks.hpp"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace sim {
using namespace rogue;
namespace {
constexpr int8_t dxs[] = {0, 1, 0, -1}, dys[] = {-1, 0, 1, 0};
int cell(Position p) { return p.y * MAP_W + p.x; }
int distance(Position a, Position b) { return std::abs(int(a.x)-b.x)+std::abs(int(a.y)-b.y); }
bool equipped(int s) { return s == game.weapon_slot || s == game.armor_slot ||
    s == game.offhand_slot || s == game.amulet_slot || s == game.ring_slots[0] || s == game.ring_slots[1]; }
Action use(int s, const char* goal, int target = NONE) {
    Action a; a.kind = ActionKind::Use; a.slot = static_cast<uint8_t>(s);
    a.target = static_cast<uint8_t>(target); a.goal = goal; return a;
}
Action cast(uint8_t spell,int direction=-1,uint8_t target=NONE) {
    Action a; a.kind=ActionKind::Cast; a.slot=spell; a.target=target; a.goal="cast learned spell";
    if(direction>=0) { a.dx=dxs[direction]; a.dy=dys[direction]; } return a;
}
Action planned_lightning() {
    Action best; int best_value=0;
    for(uint8_t s=0;s<inventory_capacity();++s) {
        Item wand=game.inventory[s];
        if(wand.type!=WAND_LIGHTNING || wand_afflicted(wand) || !wand_charges(wand)) continue;
        const auto* profile=wand_powerful(wand) ? &LIGHTNING_POWERFUL : &LIGHTNING_WAND;
        for(int d=0;d<(wand_spreads(wand) ? 1 : 4);++d) {
            uint16_t mask=0; int value=0,hits=0;
            for(int ray=0;ray<(wand_spreads(wand) ? 4 : 1);++ray) {
                int direction=wand_spreads(wand) ? ray : d;
                auto first=scan_ray(game.player,dxs[direction],dys[direction],profile->range);
                uint8_t hit=first.monster;
                if(hit==NONE || (mask&(1u<<hit))) continue;
                for(int n=0;n<profile->targets && hit!=NONE;++n) {
                    mask|=uint16_t(1u<<hit); ++hits;
                    const auto& m=game.monsters[hit];
                    value+=std::min<int>(m.hp,profile->minimum[n]+(profile->spread[n]-1)/2);
                    if(square_contains(game.player,m.pos,1) && m.hp<=profile->minimum[n]) value+=10;
                    hit=lightning_target(m.pos,mask);
                }
            }
            if(hits<2 || value<=best_value) continue;
            best_value=value; best.kind=ActionKind::Wand; best.slot=s;
            best.dx=wand_needs_direction(wand) ? dxs[d] : 0; best.dy=wand_needs_direction(wand) ? dys[d] : 0;
            best.goal="efficient lightning chain";
        }
    }
    return best;
}
Action plan_magic() {
    auto available=[](uint8_t s) { return (game.known_spells & spell_bit(s)) && game.mana>=active_spell_cost(s); };
    int nearby=0,adjacent=0,escapes=0;
    for(const auto& m:game.monsters) if(m.type) {
        if(square_contains(game.player,m.pos,1)) ++adjacent;
        if(distance(m.pos,game.player)<=3) ++nearby;
    }
    for(int d=0;d<4;++d) { Position p{uint8_t(game.player.x+dxs[d]),uint8_t(game.player.y+dys[d])};
        if(!blocked(p.x,p.y) && monster_at(p)==NONE && !feature_is_trap(observed_feature_at(game,p))) ++escapes;
    }
    bool urgent=(game.hp*3<player_max_hp() && nearby) || adjacent>=3;
    bool fast_mana=game.amulet_slot<INVENTORY && game.inventory[game.amulet_slot].type==AMULET_MEDITATION && !item_is_cursed(game.inventory[game.amulet_slot]);
    bool slow_mana=game.amulet_slot<INVENTORY && game.inventory[game.amulet_slot].type==AMULET_MEDITATION && item_is_cursed(game.inventory[game.amulet_slot]);
    uint8_t reserve=0;
    for(uint8_t s : {uint8_t(BLINK),uint8_t(SHOCKWAVE),uint8_t(BANISH),uint8_t(EXCHANGE)})
        if(game.known_spells & spell_bit(s)) reserve=std::max(reserve,active_spell_cost(s));
    if(available(CLEANSE) && (game.blindness || (nearby && (game.confused || game.slowed)))) return cast(CLEANSE);
    if(urgent && available(SHOCKWAVE) && adjacent>=2) return cast(SHOCKWAVE);
    if(urgent && available(EXCHANGE)) for(uint8_t i=0;i<MONSTERS;++i) if(exchange_target(i) && distance(game.player,game.monsters[i].pos)>=2) {
        Position p=game.monsters[i].pos;
        if(feature_is_trap(observed_feature_at(game,p))) continue;
        int enemies=0; for(uint8_t n=0;n<MONSTERS;++n) if(n!=i && game.monsters[n].type && square_contains(p,game.monsters[n].pos,1)) ++enemies;
        if(enemies<adjacent) return cast(EXCHANGE,-1,i);
    }
    if((urgent || (adjacent && game.hp*2<player_max_hp())) && available(PHASE_STEP)) {
        auto threats=[](Position p) { int n=0; for(const auto& m:game.monsters) if(m.type && square_contains(p,m.pos,2)) ++n; return n; };
        int current=threats(game.player),walk=current;
        for(int d=0;d<4;++d) { Position p{uint8_t(game.player.x+dxs[d]),uint8_t(game.player.y+dys[d])}; if(!blocked(p.x,p.y) && monster_at(p)==NONE) walk=std::min(walk,threats(p)); }
        for(int d=0;d<4;++d) {
            Position destination=game.player;
            for(int step=1;step<=3;++step) {
                Position p{uint8_t(game.player.x+step*dxs[d]),uint8_t(game.player.y+step*dys[d])};
                if(blocked(p.x,p.y)) break;
                if(step>=2 && monster_at(p)==NONE) destination=p;
            }
            if(destination!=game.player && !feature_is_trap(observed_feature_at(game,destination)) &&
               threats(destination)<current && threats(destination)<walk) return cast(PHASE_STEP,d);
        }
    }
    if(urgent && available(BLINK) && (!escapes || adjacent>=2)) return cast(BLINK);
    auto lightning=planned_lightning(); if(lightning.kind==ActionKind::Wand) return lightning;
    for(int d=0;d<4;++d) {
        RayResult ray=scan_ray(game.player,dxs[d],dys[d],5);
        if(ray.monster==NONE || (!urgent && (!player_can_see_monster(ray.monster) || !can_see(ray.end)))) continue;
        const auto& m=game.monsters[ray.monster];
        if(urgent && adjacent && ray.steps>1) continue;
        if(urgent && available(BANISH)) return cast(BANISH,d);
        Position pushed{uint8_t(m.pos.x+dxs[d]),uint8_t(m.pos.y+dys[d])};
        if(available(REPULSION) && ray.steps<=2 && !blocked(pushed.x,pushed.y) && monster_at(pushed)==NONE &&
           feature_is_trap(observed_feature_at(game,pushed)) && feature_type(observed_feature_at(game,pushed))!=FEATURE_SUMMONING_TRAP) return cast(REPULSION,d);
        if(available(FIREBURST) && (player_fire_effect()>0 || ray.steps>=2)) {
            int cluster=0; for(const auto& n:game.monsters) if(n.type && n.type!=DRAGON && square_contains(ray.end,n.pos,1)) ++cluster;
            if(cluster>=2 && (urgent || game.mana>=active_spell_cost(FIREBURST)+reserve)) return cast(FIREBURST,d);
        }
        if(available(CHAIN_LIGHTNING) && lightning_target(ray.end,uint16_t(1u<<ray.monster))!=NONE &&
           (urgent || game.mana>=active_spell_cost(CHAIN_LIGHTNING)+reserve)) return cast(CHAIN_LIGHTNING,d);
        if(available(SLUMBER) && ray.steps<=4 && !m.stun &&
           (urgent || (ray.steps<=2 && monster_strength(m.type)>=player_strength()+2))) return cast(SLUMBER,d);
        int incoming=0; for(const auto& n:game.monsters) if(n.type && !n.stun && square_contains(game.player,n.pos,2)) incoming+=monster_strength(n.type)+2;
        bool emergency_resource=false; for(const auto& i:game.inventory) if(i.type==POTION_HEALING || i.type==POTION_EXTRA_HEALING || i.type==SCROLL_TELEPORT) emergency_resource=true;
        if(available(BLOOD_BOLT) && m.hp>4 && m.hp<=6 && game.hp>incoming*2+15 && game.hp*4>=player_max_hp()*3 &&
           emergency_resource && (ray.steps>=2 || monster_armor(m.type)>=3) && monster_strength(m.type)>=player_strength()+2)
            return cast(BLOOD_BOLT,d);
        if(available(PIERCING_BOLT) && ray.steps<5) {
            auto second=scan_ray(ray.end,dxs[d],dys[d],5-ray.steps);
            if(second.monster!=NONE && player_can_see_monster(second.monster) && can_see(second.end) &&
               (urgent || game.mana>=active_spell_cost(PIERCING_BOLT)+std::max<uint8_t>(slow_mana ? 3 : fast_mana ? 1 : 2,reserve))) return cast(PIERCING_BOLT,d);
        }
        if(ray.steps<=2 && (urgent || monster_strength(m.type)>=player_strength()+2)) {
            if(available(CONFOUND) && !monster_effect(m,MON_CONFUSED) && !m.stun) return cast(CONFOUND,d);
            if(available(FROSTBIND) && !monster_effect(m,MON_SLOWED) && !m.stun) return cast(FROSTBIND,d);
            if(urgent && available(REPULSION)) return cast(REPULSION,d);
        }
        if(available(MAGIC_DART) && (ray.steps>=2 || (urgent && m.hp<=4)) &&
           (urgent || game.mana>=active_spell_cost(MAGIC_DART)+std::max<uint8_t>(slow_mana ? 3 : fast_mana ? 1 : 2,reserve))) return cast(MAGIC_DART,d);
    }
    return {};
}

int find(uint8_t type) {
    for(int s = 0; s < inventory_capacity(); ++s) if(game.inventory[s].type == type) return s;
    return -1;
}
int quantity(uint8_t type) {
    int n = 0;
    for(const Item& i : game.inventory) if(i.type == type) n += item_value(i);
    return n;
}
// Scores are policy preferences, not alternate combat resolution.
int equipment_score(Item i) {
    if(!i.type || item_is_cursed(i)) return 0;
    if(is_weapon(i.type)) {
        auto w = weapon_definition(i.type);
        int score = 5 * (w.minimum_damage+w.maximum_damage) + 3*w.accuracy + 3*equipment_enchant(i);
        if(i.type == STORMBRINGER) score += 10; // Splash, less the life cost.
        if(i.type == HAMMER_OF_RUIN) score += 18; // Heavy hits and control.
        if(i.type == GLASS_SWORD) score -= 4; // Keep a fallback when it breaks.
        return score;
    }
    if(is_armor(i.type)) {
        int score = 15*armor_definition(i.type).rating + 3*equipment_enchant(i);
        if(i.type == DRAGONHIDE) score += game.floor >= 11 ? 45 : 20;
        return score;
    }
    if(is_light(i.type)) {
        if(is_fueled_light(i.type) && !i.info) return 0;
        int sight = sim::full_light() ? 6 : 1 + artifact_ring_bonus(RING_ILLUMINATION, 2, -1) +
            (game.amulet_slot < INVENTORY && game.inventory[game.amulet_slot].type == AMULET_BRILLIANCE ? 3 : 0);
        return (sight < 3 ? 70 : 5) + 15*light_bonus(i.type) +
            (i.type == GLOWING_ORB ? 25 : std::min(20, int(i.info)*fuel_denominator(i.type)/30));
    }
    if(is_shield(i.type)) {
        int sight = sim::full_light() ? 6 : 1 + artifact_ring_bonus(RING_ILLUMINATION, 2, -1) +
            (game.amulet_slot < INVENTORY && game.inventory[game.amulet_slot].type == AMULET_BRILLIANCE ? 3 : 0);
        return (sight >= 3 ? 65 : 0) + 15*shield_rating(i.type) + 3*equipment_enchant(i);
    }
    int n = item_value(i);
    switch(i.type) {
    case RING_ILLUMINATION: return 100;
    case AMULET_BRILLIANCE: return 135;
    case RING_PROTECTION: return 65+15*n;
    case RING_INVISIBILITY: return 100;
    case RING_STRENGTH: return 45+8*n;
    case RING_DEXTERITY: return 50+8*n;
    case RING_ATTACK: return 40+8*n;
    case RING_REPRISAL: return 125; // Protection and counters.
    case RING_HUNT: return quantity(ARROWS) ? 130 : 95;
    case RING_FIRE_IMMUNITY: return game.floor >= 11 ? 85 : 55;
    case RING_SUSTENANCE: return 35;
    case RING_SEE_INVISIBLE: return 15;
    case AMULET_SPEED: return 100+25*std::min(n,3);
    case AMULET_IRONBLOOD: return 90;
    case AMULET_CLARITY: return 65;
    case AMULET_VAMPIRE: return 80;
    case AMULET_VITALITY: return 55+10*n;
    case AMULET_PHOENIX_HEART: return 185;
    case AMULET_HEART_OF_GIANT: return 190;
    case AMULET_WISDOM: return 60;
    case AMULET_MEDITATION: return game.known_spells & ~spell_bit(MAGIC_DART) ? 75 : 45;
    case AMULET_REGENERATION: return 40;
    case AMULET_CONSERVATION: return 35;
    default: return 0;
    }
}
int slot_score(int s) { return s < inventory_capacity() ? equipment_score(game.inventory[s]) : 0; }
int best_melee() {
    if(game.weapon_slot < INVENTORY && is_bow(game.inventory[game.weapon_slot].type) && item_is_cursed(game.inventory[game.weapon_slot])) return NONE;
    int best = NONE;
    for(int s=0;s<inventory_capacity();++s) if(is_weapon(game.inventory[s].type) && !is_bow(game.inventory[s].type) &&
        equipment_score(game.inventory[s]) > slot_score(best)) best=s;
    return best;
}
int glass_fallback() {
    if(find(GLASS_SWORD)<0) return NONE;
    int best=NONE;
    for(int s=0;s<inventory_capacity();++s) {
        auto i=game.inventory[s];
        if(is_weapon(i.type) && !is_bow(i.type) && i.type!=GLASS_SWORD &&
           equipment_score(i)>slot_score(best)) best=s;
    }
    return best;
}
int bow_score(Item i) {
    if(!is_bow(i.type) || item_is_cursed(i)) return 0;
    auto w=ranged_weapon_definition(i.type);
    return 5*(w.minimum_damage+w.maximum_damage)+3*w.accuracy+2*w.range+3*equipment_enchant(i);
}
int best_bow() {
    int best=NONE;
    for(int s=0;s<inventory_capacity();++s) if(bow_score(game.inventory[s]) > (best==NONE ? 0 : bow_score(game.inventory[best]))) best=s;
    return best;
}
bool bow_equipped() { return game.weapon_slot < INVENTORY && is_bow(game.inventory[game.weapon_slot].type); }
bool upgrade(Item i) {
    if(!equipment_score(i)) return false;
    int s = NONE;
    if(is_bow(i.type)) return false;
    if(is_weapon(i.type)) s = bow_equipped() ? best_melee() : game.weapon_slot;
    else if(is_armor(i.type)) s = game.armor_slot;
    else if(is_offhand(i.type)) s = game.offhand_slot;
    else if(is_amulet(i.type)) s = game.amulet_slot;
    else if(is_ring(i.type)) {
        if(game.ring_slots[0] == NONE || game.ring_slots[1] == NONE) return true;
        s = slot_score(game.ring_slots[0]) < slot_score(game.ring_slots[1]) ?
            game.ring_slots[0] : game.ring_slots[1];
    }
    return (s == NONE || !item_is_cursed(game.inventory[s])) && equipment_score(i) > slot_score(s);
}
int value(Item i) {
    if(i.type==SPELLBOOK) {
        return remaining_book_value(i.info, game.known_spells, game.floor, best_bow()!=NONE,legacy_spell_rules());
    }
    if(i.type == YENDOR_AMULET) return 10000;
    if(is_bow(i.type)) {
        int best=best_bow();
        return bow_score(i) > (best==NONE ? 0 : bow_score(game.inventory[best])) ? 100+bow_score(i) : 0;
    }
    if(is_ammo(i.type)) {
        int n=quantity(ARROWS);
        return n >= 24 ? 0 : best_bow()!=NONE ? (n<5 ? 170 : 95) : n<8 ? 45 : 0;
    }
    if(is_light(i.type)) {
        int best = 0, reserve = 0;
        for(const Item& owned : game.inventory) if(is_light(owned.type)) {
            best = std::max(best, equipment_score(owned));
            reserve += owned.type == GLOWING_ORB ? 10000 : int(owned.info)*fuel_denominator(owned.type);
        }
        if(equipment_score(i) > best) return 100+equipment_score(i);
        return reserve < 600 ? 120 + equipment_score(i)/2 : 0;
    }
    if(is_equipment(i.type) || is_ring(i.type) || is_amulet(i.type))
        return upgrade(i) ? 100+equipment_score(i) : 0;
    if(is_wand(i.type)) {
        if(wand_afflicted(i) || !wand_charges(i)) return 0;
        switch(i.type) {
        case WAND_LIGHTNING: return 70 + 8*wand_charges(i);
        case WAND_STRIKING: return 70 + 8*wand_charges(i);
        case WAND_ICE: return 65 + 6*wand_charges(i);
        case WAND_FIRE: return 55 + 5*wand_charges(i);
        case WAND_FORCE: return 45 + 4*wand_charges(i);
        case WAND_TELEPORT: return 65 + 5*wand_charges(i);
        case WAND_POLYMORPH: return 30 + 3*wand_charges(i);
        // Digging carves terrain but does not move/control anyone. The policy
        // deliberately gives speculative shortcuts no tactical value.
        case WAND_DIGGING: return 0;
        default: return 0;
        }
    }
    switch(i.type) {
    case FOOD: return quantity(FOOD) >= 8 ? 0 : (game.hunger < 60 ? 180 : 70);
    case POTION_MANA_RESTORATION: return 120;
    case POTION_EXTRA_HEALING: return 220;
    case POTION_RESTORE_VITALITY: return game.vamp_drain || game.weakened ? 160 : 70;
    case POTION_HASTE: case POTION_LEVITATION: case POTION_FIRE_RESISTANCE: return 100;
    case POTION_FIRE: return 85;
    case SCROLL_HOLD_MONSTER: return 160; case SCROLL_MASS_TELEPORT: return 180;
    case SCROLL_DETECT_TRAPS: return 100; case SCROLL_FIRE: return 45;
    case POTION_HEALING: return 180;
    case POTION_EXPERIENCE: return 220;
    case POTION_STRENGTH: return game.strength < 12 || game.weakened ? 160 : 0;
    case POTION_DEXTERITY: return game.dexterity < 12 ? 150 : 0;
    case POTION_PARALYSIS: return 100;
    case POTION_CONFUSION: case POTION_BLINDNESS: return 65;
    case POTION_SLOWING: return 60;
    case POTION_POISON: return 60;
    case POTION_HARMING: return 55;
    case POTION_INVISIBILITY: return 70;
    case POTION_SEE_INVISIBLE: return 90;
    case SCROLL_ENCHANT: return 190;
    case SCROLL_TORMENT: return 95;
    case SCROLL_MASS_CONFUSE: return 100;
    case SCROLL_MASS_POISON: return 75;
    case SCROLL_FEAR: return 60;
    case SCROLL_TELEPORT: return 45;
    // The oracle never acquires afflicted gear/wands, so this is not useful.
    case SCROLL_REMOVE_CURSE: return 0;
    default: return 0;
    }
}
int acquisition_value(Item i) {
    if(i.type==POTION_MANA_RESTORATION && quantity(POTION_MANA_RESTORATION)>=3) return 0;
    if(i.type==SPELLBOOK) for(const auto& owned:game.inventory)
        if(owned.type==SPELLBOOK && owned.info==i.info) return 0;
    return value(i);
}
int discard_value(int s) {
    if(!inventory_item_removable(static_cast<uint8_t>(s)) || equipped(s)) return 100000;
    Item i = game.inventory[s];
    // Retaining stocked food has value even when the desired supply cap was
    // reached. Using acquisition value here caused food/scroll swap cycles.
    if(i.type == FOOD) return 70 + 20*item_value(i);
    // Keep a useful bow at least as valuable as reacquiring that same bow.
    // Otherwise a full pack can swap bow -> wand -> bow forever without ammo.
    if(is_bow(i.type)) return s==best_bow() ? (quantity(ARROWS) ? 150 : 100)+bow_score(i) : 0;
    if(is_ammo(i.type)) return best_bow()!=NONE ? 170+std::min(24,int(item_value(i)))*4 : 45;
    if(is_light(i.type)) return 100 + equipment_score(i);
    if(bow_equipped() && s==best_melee()) return 180+equipment_score(i);
    if(s==glass_fallback()) return 150+equipment_score(i);
    return value(i) + (is_stackable(i.type) ? item_value(i)*5 : 0);
}
bool fits(Item incoming) {
    int capacity = 0;
    bool stack = is_stackable(incoming.type);
    for(uint8_t s=0;s<inventory_capacity();++s) {
        Item i=game.inventory[s];
        if(!i.type) return true;
        if(stack && i.type == incoming.type) capacity += maximum_stack(i.type)-item_value(i);
    }
    return stack && capacity >= item_value(incoming);
}
int replacement(Item i) {
    int best = -1, worst = value(i);
    for(int s = 0; s < inventory_capacity(); ++s) {
        // Reserve acquisition can exceed a depleted candle's retention value.
        // Replacing a light with one that improves neither capability nor fuel
        // cannot help and otherwise permits a full-pack reverse-swap cycle.
        const auto owned=game.inventory[s];
        if(is_light(i.type) && is_light(owned.type) &&
           equipment_score(i)<=equipment_score(owned) &&
           (owned.type==GLOWING_ORB || (i.type!=GLOWING_ORB &&
            int(i.info)*fuel_denominator(i.type)<=int(owned.info)*fuel_denominator(owned.type)))) continue;
        // Bow and ammo are complementary slots. An ammo acquisition must not
        // remove its only useful launcher; a bow must not remove its last ammo.
        // Otherwise their acquisition values reverse after each accepted swap.
        if(is_ammo(i.type) && s==best_bow()) continue;
        // Hunt and its arrows are complementary too. Trading either for the
        // other changes Hunt's value and otherwise produces reverse-swap loops.
        if(is_ammo(i.type) && game.inventory[s].type==RING_HUNT) continue;
        if(i.type==RING_HUNT && is_ammo(game.inventory[s].type) &&
           quantity(ARROWS)==item_value(game.inventory[s])) continue;
        if(is_bow(i.type) && is_ammo(game.inventory[s].type) &&
           quantity(ARROWS)==item_value(game.inventory[s])) continue;
        if(game.inventory[s].type && discard_value(s) < worst) { best = s; worst = discard_value(s); }
    }
    return best;
}
Action pickup(int index) {
    Action a; a.target = static_cast<uint8_t>(index); a.goal = "collect supplies";
    a.destination = game.ground[index].pos;
    Item i = game.ground[index].item;
    a.kind = ActionKind::Take;
    // A failed take costs no turn. The runner follows PICKUP_NEEDS_SWAP with
    // this deterministic removable slot through the production swap API.
    if(i.type != YENDOR_AMULET && !fits(i)) a.slot = static_cast<uint8_t>(replacement(i));
    return a;
}

bool controlled(const Monster& m) {
    return m.stun || (m.state & MON_AFRAID) || monster_effect(m,MON_CONFUSED);
}
bool hostile(const Monster& m) {
    uint16_t flags = monster_flags(m.type);
    return ((flags & MON_MEAN) || (m.state & MON_AGGRO)) &&
        (!player_is_invisible() || (flags & MON_SEE_INVIS));
}
// A deliberately rough danger preference, not a roll or a combat simulation.
int offensive_pressure(const Monster& m) {
    auto info = monster_info(m.type);
    int pressure = std::max(1,int(info.strength)+1-player_armor_rating()/2);
    if(monster_effect(m,MON_WEAKENED)) pressure = (pressure+1)/2;
    if(info.flags & MON_FIRE_BREATH) pressure += player_fire_effect() > 0 ? 0 : player_fire_effect() < 0 ? 8 : 4;
    if((info.flags & MON_PARALYZE_HIT) && amulet_bonus(AMULET_IRONBLOOD) <= 0) pressure += 2;
    if((info.flags & MON_CONFUSE_HIT) && amulet_bonus(AMULET_CLARITY) <= 0) pressure += 2;
    int cost = player_speed_cost();
    int speed = std::max(1,int(info.speed)*(monster_effect(m,MON_SLOWED) ? 2 : 1));
    if(cost > speed) pressure += pressure/2;
    return pressure;
}
// Deterministic expected-value estimates; only production resolves attacks.
int estimated_damage(Item weapon, uint8_t target, bool ranged) {
    int low,high;
    if(ranged) { auto w=ranged_weapon_definition(weapon.type); low=w.minimum_damage; high=w.maximum_damage; }
    else { auto w=weapon_definition(weapon.type); low=w.minimum_damage; high=w.maximum_damage; }
    int mean=(low+high)*2 + equipment_enchant(weapon); // quarters of HP
    int damage = std::max(4,mean+4*strength_damage_bonus(player_strength()) +
        (ranged ? 4*artifact_ring_bonus(RING_HUNT, 6, -4) : 0) -3*monster_armor(target));
    if(!ranged && weapon.type == HAMMER_OF_RUIN) damage += damage/3;
    return damage;
}
bool bow_opportunity(Action& choice, int bow) {
    if(game.weapon_slot != bow && game.weapon_slot < INVENTORY && item_is_cursed(game.inventory[game.weapon_slot])) return false;
    if(bow==NONE || quantity(ARROWS)==0) return false;
    int supply=quantity(ARROWS), best=0, melee=best_melee();
    auto w=ranged_weapon_definition(game.inventory[bow].type);
    bool ready=game.weapon_slot==bow;
    for(int d=0;d<4;++d) {
        auto ray=scan_ray(game.player,dxs[d],dys[d],w.range);
        if(ray.monster==NONE || !player_can_see_monster(ray.monster)) continue;
        const auto& m=game.monsters[ray.monster];
        if(!can_see(m.pos) || !hostile(m) || m.type==BAT) continue;
        int pressure=offensive_pressure(m);
        if((supply<=4 && pressure<7 && m.type!=LORD) || (supply<=12 && pressure<4 && m.type!=LORD)) continue;
        int damage=estimated_damage(game.inventory[bow],m.type,true);
        int melee_damage=estimated_damage(melee==NONE ? Item{NO_ITEM,0} : game.inventory[melee],m.type,false);
        // Pay both equipment turns only when the approach leaves useful firing time.
        int budget=player_speed_cost();
        int enemy_speed=std::max(1,int(monster_speed(m.type))*(monster_effect(m,MON_SLOWED) ? 2 : 1));
        int approach=controlled(m) || (monster_flags(m.type)&MON_NOMOVE) ? 0 : (budget+enemy_speed-1)/enemy_speed;
        if(!ready && ray.steps < 2*approach+2) continue;
        if(ray.steps<=2 && melee!=NONE && damage<melee_damage) continue;
        int accuracy=player_ranged_accuracy(game.inventory[bow].type);
        int expected=damage*(2*accuracy+1)/(2*accuracy+monster_dexterity(m.type)+1);
        // Armor can make a shot little more than a provocation (notably Dragons).
        // Reserve that shot for a likely finishing blow, rather than wasting a swap.
        if(expected<8 && 4*m.hp>expected) continue;
        int score=expected+2*pressure+ray.steps-(ready ? 0 : 8);
        if(!ready && score<20) continue;
        if(score<=best) continue;
        best=score;
        choice=ready ? Action{} : use(bow,"equip bow for visible target");
        if(ready) { choice.kind=ActionKind::Throw; choice.slot=static_cast<uint8_t>(find(ARROWS)); choice.dx=dxs[d]; choice.dy=dys[d]; choice.goal="shoot visible threat"; }
        choice.destination=m.pos;
    }
    return best>0;
}
struct Danger {
    int adjacent=0, nearby=0, pressure=0;
    bool emergency=false;
};
Danger danger() {
    Danger result;
    for(const auto& m : game.monsters) if(m.type && hostile(m)) {
        int d = distance(m.pos,game.player);
        if(d <= 4 && can_see(m.pos)) ++result.nearby;
        if(controlled(m)) continue;
        if(d <= 2 && can_see(m.pos)) result.pressure += offensive_pressure(m)*(d == 1 ? 2 : 1);
        if(d == 1) ++result.adjacent;
    }
    // Nearby monsters matter, but do not burn escape resources on a lone weak
    // healthy encounter. Low HP, multiple attackers and status impairments do.
    result.emergency = result.pressure > 0 &&
        (game.hp <= result.pressure+3 ||
         (result.adjacent > 0 && game.hp*100 <= player_max_hp()*35 && result.pressure >= 10) ||
         (result.adjacent >= 2 && game.hp*3 <= player_max_hp()*2) ||
         ((game.confused || game.weakened || game.slowed) && result.adjacent && game.hp*2 < player_max_hp()));
    return result;
}
bool fire_safe(Item wand, int d) {
    if(wand_afflicted(wand) || !wand_charges(wand)) return false;
    if(player_fire_effect() > 0) return true;
    int rays = wand_spreads(wand) ? 4 : 1;
    for(int n = 0; n < rays; ++n) {
        int direction = rays == 4 ? n : d;
        auto ray = scan_ray(game.player,dxs[direction],dys[direction],6);
        if(square_contains(ray.end,game.player,wand_fire_radius(wand_powerful(wand)))) return false;
    }
    return true;
}
uint16_t affected_monsters(Item wand, int d) {
    uint16_t mask = 0;
    int rays = wand_spreads(wand) ? 4 : 1;
    for(int n = 0; n < rays; ++n) {
        int direction = rays == 4 ? n : d;
        auto ray = scan_ray(game.player,dxs[direction],dys[direction],6);
        bool area = wand_powerful(wand) && (wand.type == WAND_TELEPORT || wand.type == WAND_POLYMORPH);
        if(area) {
            for(int i = 0; i < MONSTERS; ++i) if(game.monsters[i].type && square_contains(ray.end,game.monsters[i].pos,1))
                mask |= static_cast<uint16_t>(1u << i);
        } else if(ray.monster != NONE) mask |= static_cast<uint16_t>(1u << ray.monster);
    }
    return mask;
}
Action wand_action(int s, int d, const char* goal, Position target) {
    Action a; a.kind = ActionKind::Wand; a.slot = static_cast<uint8_t>(s);
    if(wand_needs_direction(game.inventory[s])) { a.dx = dxs[d]; a.dy = dys[d]; }
    a.goal = goal; a.destination = target; return a;
}
bool emergency_wand(uint8_t type, Action& choice) {
    int best_score = 0;
    for(int s = 0; s < inventory_capacity(); ++s) {
        Item wand = game.inventory[s];
        if(wand.type != type || wand_afflicted(wand) || !wand_charges(wand)) continue;
        for(int d = 0; d < (wand_spreads(wand) ? 1 : 4); ++d) {
            uint16_t mask = affected_monsters(wand,d);
            int score = 0; Position target{NONE,NONE}; bool unsuitable = false;
            for(int i = 0; i < MONSTERS; ++i) if(mask & (1u << i)) {
                const auto& m = game.monsters[i];
                if(type == WAND_POLYMORPH && (m.type < INCUBUS || m.type == LORD || m.hp < monster_health(m.type)/2)) {
                    unsuitable = true; break;
                }
                if(!hostile(m) || controlled(m) || distance(m.pos,game.player) > 2) continue;
                if(type == WAND_POLYMORPH && game.hp*100 > player_max_hp()*35 && game.hp > offensive_pressure(m)*2) continue;
                if(type == WAND_FORCE) {
                    int direction = d;
                    if(wand_spreads(wand)) {
                        direction = m.pos.x == game.player.x ? (m.pos.y < game.player.y ? 0 : 2) :
                            (m.pos.x > game.player.x ? 1 : 3);
                    }
                    auto pushed = scan_ray(m.pos,dxs[direction],dys[direction],wand_powerful(wand) ? 16 : 8);
                    auto end = pushed.monster != NONE ? pushed.before : pushed.end;
                    if(!pushed.blocker && pushed.monster == NONE && distance(end,game.player) < distance(m.pos,game.player)+2) continue;
                }
                score += offensive_pressure(m)*(distance(m.pos,game.player) == 1 ? 2 : 1);
                if(target.x == NONE) target = m.pos;
            }
            if(!unsuitable && score > best_score) {
                best_score = score;
                choice = wand_action(s,d,type == WAND_TELEPORT ? "emergency teleport enemy" :
                    type == WAND_FORCE ? "emergency force enemy away" : "emergency polymorph threat",target);
            }
        }
    }
    return best_score > 0;
}
int retreat_pressure(Position p) {
    int pressure = 0;
    for(const auto& m : game.monsters) if(m.type && hostile(m) && !controlled(m)) {
        int d = distance(m.pos,p);
        if(d <= 4) pressure += offensive_pressure(m)*(d <= 1 ? 6 : d == 2 ? 3 : 1);
    }
    return pressure;
}
bool retreat(Position previous, Action& choice) {
    if(game.confused) return false; // Directional movement can be randomized.
    Position goal = game.has_amulet ? game.up : game.down;
    int current = retreat_pressure(game.player), best_score = 0;
    for(int d = 0; d < 4; ++d) {
        Position p{static_cast<uint8_t>(game.player.x+dxs[d]),static_cast<uint8_t>(game.player.y+dys[d])};
        // A closed door costs a turn without moving; an occupied cell attacks.
        if(p == previous || blocked(p.x,p.y) || monster_at(p) != NONE) continue;
        int exits = 0;
        for(int n = 0; n < 4; ++n) {
            Position next{static_cast<uint8_t>(p.x+dxs[n]),static_cast<uint8_t>(p.y+dys[n])};
            if(!blocked(next.x,next.y) && monster_at(next) == NONE) ++exits;
        }
        if(exits < 2) continue;
        int reduction = current-retreat_pressure(p);
        if(reduction < 6) continue;
        int score = reduction+3*exits+distance(game.player,goal)-distance(p,goal);
        if(score > best_score) {
            best_score = score; choice.kind = ActionKind::Move; choice.dx = dxs[d]; choice.dy = dys[d];
            choice.destination = p; choice.goal = "emergency retreat to safer space";
        }
    }
    return best_score > 0;
}
bool emergency_control(Action& choice) {
    // Hard control first. Production ray/visibility checks still decide hits.
    for(uint8_t type : {uint8_t(POTION_PARALYSIS),uint8_t(POTION_CONFUSION),uint8_t(POTION_BLINDNESS)}) {
        int s = find(type); if(s < 0) continue;
        for(int d = 0; d < 4; ++d) {
            auto ray = scan_ray(game.player,dxs[d],dys[d],6);
            if(ray.monster == NONE || ray.steps > 2) continue;
            const auto& m = game.monsters[ray.monster];
            if(!hostile(m) || controlled(m)) continue;
            choice.kind = ActionKind::Throw; choice.slot = static_cast<uint8_t>(s);
            choice.dx = dxs[d]; choice.dy = dys[d]; choice.destination = m.pos;
            choice.goal = "emergency control"; return true;
        }
    }
    for(uint8_t type : {uint8_t(SCROLL_MASS_CONFUSE),uint8_t(SCROLL_FEAR)}) {
        int s = find(type); if(s < 0) continue;
        for(int i = 0; i < MONSTERS; ++i) {
            const auto& m = game.monsters[i];
            if(m.type && hostile(m) && !controlled(m) && distance(m.pos,game.player) <= 2 &&
               player_can_see_monster(static_cast<uint8_t>(i)) && can_see(m.pos)) {
                choice = use(s,"emergency visible control"); return true;
            }
        }
    }
    // Invisibility is useful control only if every immediate threat lacks sight.
    int invisible = find(POTION_INVISIBILITY); bool usable_invisibility = invisible >= 0 && !player_is_invisible();
    for(const auto& m : game.monsters) if(m.type && distance(m.pos,game.player) <= 3 && hostile(m) &&
        (monster_flags(m.type) & MON_SEE_INVIS)) usable_invisibility = false;
    if(usable_invisibility) { choice = use(invisible,"emergency invisibility"); return true; }
    int teleport = find(SCROLL_TELEPORT);
    if(teleport >= 0) { choice = use(teleport,"emergency teleport player"); return true; }
    for(uint8_t type : {uint8_t(WAND_TELEPORT),uint8_t(WAND_FORCE),uint8_t(WAND_POLYMORPH)})
        if(emergency_wand(type,choice)) return true;
    return false;
}
}

Paths::Paths(bool allow_monsters, bool allow_teleports) {
    distance.fill(-1); first.fill(-1);
    std::array<bool, MAP_W*MAP_H> occupied{};
    for(const Monster& m : game.monsters) if(m.type) occupied[cell(m.pos)] = true;
    std::array<int, MAP_W*MAP_H> queue{};
    std::array<int, MAP_W*MAP_H> penalty{};
    bool traps = false;
    std::array<bool, MAP_W*MAP_H> teleport{};
    int pressure = 0;
    for(const Monster& m : game.monsters) if(m.type && square_contains(m.pos, game.player, 2)) ++pressure;
    for(const MapFeature& f : observed_features(game)) if(feature_is_trap(f.id)) {
        teleport[cell(f.pos)] = f.id == FEATURE_TELEPORT_TRAP && !allow_teleports;
        penalty[cell(f.pos)] = f.id == FEATURE_TELEPORT_TRAP ? 60 : pressure ? 3 :
            f.id == FEATURE_CONFUSION_TRAP ? 25 : game.hp < 10 ? 20 : 10;
        if(f.id == FEATURE_FLAME_TRAP && player_fire_effect() > 0) penalty[cell(f.pos)] = 0;
        traps = traps || penalty[cell(f.pos)] != 0;
    }
    if(traps) {
        using Node = std::pair<int, int>;
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pending;
        std::array<int, MAP_W * MAP_H> costs; costs.fill(-1);
        int root = cell(game.player); distance[root] = 0; costs[root] = 0; pending.push({0, root});
        while(!pending.empty()) {
            auto [cost, at] = pending.top(); pending.pop();
            if(cost != costs[at]) continue;
            if(at != root && occupied[at] && !allow_monsters) continue;
            for(int d = 0; d < 4; ++d) {
                int nx = at % MAP_W + dxs[d], ny = at / MAP_W + dys[d];
                if(wall_at(int16_t(nx), int16_t(ny))) continue;
                int next = ny * MAP_W + nx;
                if(teleport[next]) continue;
                int new_cost = cost + 1 + penalty[next];
                if(costs[next] >= 0 && costs[next] <= new_cost) continue;
                costs[next] = new_cost;
                distance[next] = static_cast<int16_t>(distance[at] + 1);
                first[next] = at == root ? int8_t(d) : first[at];
                pending.push({new_cost, next});
            }
        }
        return;
    }
    int begin = 0, end = 0, root = cell(game.player);
    queue[end++] = root; distance[root] = 0;
    while(begin < end) {
        int at = queue[begin++], x = at % MAP_W, y = at / MAP_W;
        for(int d = 0; d < 4; ++d) {
            int nx = x+dxs[d], ny = y+dys[d];
            if(wall_at(static_cast<int16_t>(nx), static_cast<int16_t>(ny))) continue;
            int next = ny*MAP_W+nx;
            if(distance[next] >= 0) continue;
            distance[next] = static_cast<int16_t>(distance[at]+1);
            first[next] = at == root ? static_cast<int8_t>(d) : first[at];
            if(!occupied[next] || allow_monsters) queue[end++] = next;
        }
    }
}
int Paths::to(Position p) const { return p.x < MAP_W && p.y < MAP_H ? distance[cell(p)] : -1; }
Action Paths::move_to(Position p, const std::string& goal) const {
    Action a; a.goal = goal; a.destination = p;
    int d = to(p) > 0 ? first[cell(p)] : -1;
    if(d >= 0) { a.kind = ActionKind::Move; a.dx = dxs[d]; a.dy = dys[d]; }
    else { a.goal = "unable to find path"; a.diagnostic = Diagnostic::NoPath; }
    return a;
}

Action OmniscientAgent::navigate(const Paths& paths, Position destination, const char* goal, bool optional) {
    // Only observed traps enable history. Hidden positions/types never enter it.
    bool known = false;
    for(const auto& f : observed_features(game)) known |= f.id == FEATURE_TELEPORT_TRAP;
    if(known) {
        int d = paths.to(destination);
        if(destination != navigation_goal) {
            navigation_goal = destination; navigation_stale = navigation_cursor = 0;
            navigation_best = 10000; navigation_history.fill({NONE, NONE});
        }
        if(d >= 0 && d < navigation_best) { navigation_best = d; navigation_stale = 0; }
        else ++navigation_stale;
        unsigned returns = 0;
        for(auto p : navigation_history) returns += p == game.player;
        navigation_history[navigation_cursor++ % navigation_history.size()] = game.player;
        if(optional && (navigation_stale > 96 || returns >= 3)) {
            for(unsigned i=0;i<GROUND_ITEMS;++i) if(game.ground[i].pos==destination) abandoned_loot[i]=true;
            for(unsigned i=0;i<MAP_FEATURES;++i) {
                auto f=observed_feature(game.features[i]);
                if(f.id && f.pos==destination) feature_attempted[i]=true;
            }
            return navigate(paths, game.has_amulet ? game.up : game.down, "abandon looping optional destination");
        }
    }
    if(paths.to(destination) >= 0) return paths.move_to(destination, goal);
    // One bounded relaxed calculation, only for mandatory unreachable goals.
    // The ordinary tree remains authoritative wherever a detour exists.
    return Paths(true, true).move_to(destination, goal);
}
Action OmniscientAgent::choose_pickup(const DecisionContext&) {
    int best = -1, worth = 0;
    for(int i = 0; i < GROUND_ITEMS; ++i) {
        const GroundItem& g = game.ground[i];
        if(g.pos != game.player || acquisition_value(g.item) <= worth ||
           (g.item.type != YENDOR_AMULET && !fits(g.item) && replacement(g.item) < 0)) continue;
        best = i; worth = acquisition_value(g.item);
    }
    return best >= 0 ? pickup(best) : Action{};
}
Action OmniscientAgent::choose_action(const DecisionContext& context) {
    if(previous_study!=NONE && !studying() && !(game.known_spells & spell_bit(previous_study))) study_defer_until=context.turns+128;
    previous_study=studying() ? session.study_spell : uint8_t(NONE);
    if(studying()) { Action a; a.goal="continue spellbook study"; return a; }
    if(game.paralyzed) { Action a; a.goal = "wait for paralysis"; return a; }
    int adjacent = -1, threats = 0;
    for(int i = 0; i < MONSTERS; ++i) if(game.monsters[i].type) {
        const Monster& m = game.monsters[i];
        int d = distance(m.pos,game.player);
        if(d <= 5 && can_see(m.pos)) ++threats;
        if(d == 1 && (adjacent < 0 || m.hp < game.monsters[adjacent].hp)) adjacent = i;
    }
    if(game.known_spells && game.hp>=15 && game.mana*3<=player_max_mana() && find(POTION_MANA_RESTORATION)>=0 &&
       (threats || (game.has_amulet && game.known_spells & (spell_bit(BLINK)|spell_bit(PHASE_STEP)|spell_bit(BANISH)))))
        return use(find(POTION_MANA_RESTORATION),"restore casting and escape mana");
    Action magic=plan_magic(); if(magic.kind==ActionKind::Cast || magic.kind==ActionKind::Wand) return magic;
    if(!threats && game.floor>=2 && context.turns>=sense_until && game.mana==player_max_mana() &&
       (game.known_spells & spell_bit(SENSE_TRAPS))) {
        bool known=false; for(const auto& f:observed_features(game)) if(feature_is_trap(f.id) && distance(f.pos,game.player)<=4) known=true;
        if(!known) { sense_until=context.turns+96; return cast(SENSE_TRAPS); }
    }
    if(!game.blindness && !game.confused && game.hp*4>=player_max_hp()*3 && context.turns>=study_defer_until && study_budget_available(game.known_spells,game.floor)) {
        bool safe=true; for(const auto& m:game.monsters) if(m.type && distance(m.pos,game.player)<10) safe=false;
        int exits=0; for(int d=0;d<4;++d) { Position p{uint8_t(game.player.x+dxs[d]),uint8_t(game.player.y+dys[d])};
            if(!blocked(p.x,p.y) && !feature_is_trap(observed_feature_at(game,p))) ++exits;
        }
        Item light=game.offhand_slot<INVENTORY ? game.inventory[game.offhand_slot] : Item{};
        int best_book=-1, best_priority=79;
        for(uint8_t s=0;s<inventory_capacity();++s) if(game.inventory[s].type==SPELLBOOK) {
            uint8_t spell=preferred_book_spell(game.inventory[s].info,game.known_spells,game.floor,best_bow()!=NONE,legacy_spell_rules());
            int priority=study_priority(spell,game.known_spells,game.floor,best_bow()!=NONE,legacy_spell_rules());
            if(priority>best_priority) { best_book=s; best_priority=priority; }
        }
        for(uint8_t s=0;s<INVENTORY && safe && exits>=2;++s) {
            if(s!=best_book) continue;
            Item book=game.inventory[s];
            if(book.type!=SPELLBOOK || game.has_amulet) continue;
            uint8_t spell=preferred_book_spell(book.info,game.known_spells,game.floor,best_bow()!=NONE,legacy_spell_rules());
            if(spell==NONE || study_priority(spell,game.known_spells,game.floor,best_bow()!=NONE,legacy_spell_rules())<80) continue;
            bool fuel=light.type==GLOWING_ORB || (is_fueled_light(light.type) && light.info*fuel_denominator(light.type)>=active_study_turns(spell)+24);
            if((!fuel || player_light_radius()<2) && !item_is_cursed(light)) {
                for(uint8_t l=0;l<inventory_capacity();++l) {
                    const Item& candidate=game.inventory[l];
                    if(l!=game.offhand_slot && (candidate.type==GLOWING_ORB ||
                       (is_fueled_light(candidate.type) && candidate.info*fuel_denominator(candidate.type)>=active_study_turns(spell)+24)))
                        return use(l,"light for spellbook study");
                }
            }
            if(fuel && !feature_is_trap(observed_feature_at(game,game.player))) {
                if(game.hunger>=active_study_food_required(spell)) {
                    previous_study=spell; return use(s,"study selected spellbook spell",spell);
                }
                int food=find(FOOD);
                if(food>=0 && quantity(FOOD)>=2) return use(food,"prepare food for spellbook study");
            }
        }
    }
    if(game.confused && !threats && feature_is_trap(observed_feature_at(game, game.player))) {
        Action a; a.goal = "recover senses on a discovered trap"; return a;
    }
    if(feature_floor != game.floor) { feature_floor = game.floor; holy_entries = feature_steps = summoned_combat_steps = 0; summoned_origin = {NONE,NONE}; feature_attempted.fill(false); abandoned_loot.fill(false); navigation_goal = {NONE, NONE}; navigation_stale = navigation_cursor = 0; navigation_best = 10000; navigation_history.fill({NONE, NONE}); }
    // A replenishing summoning trap can sustain endless, zero-XP combat when
    // shields and regeneration absorb the damage. Leave after a bounded fight;
    // only discovered traps inform this decision, never hidden feature metadata.
    if(summoned_origin != game.player) { summoned_origin = game.player; summoned_combat_steps = 0; }
    if(adjacent >= 0 && (game.monsters[adjacent].state & MON_TRAP_SUMMONED)) {
        for(const MapFeature& f : observed_features(game)) {
            if(f.id != FEATURE_SUMMONING_TRAP || distance(f.pos, game.player) > 6) continue;
            if(++summoned_combat_steps <= 64) break;
            int escape = find(SCROLL_TELEPORT);
            if(escape >= 0) return use(escape,"leave replenishing summoning fight");
            escape = find(POTION_INVISIBILITY);
            if(escape >= 0 && !player_is_invisible()) return use(escape,"leave replenishing summoning fight");
            escape = find(WAND_TELEPORT);
            if(escape >= 0 && !wand_afflicted(game.inventory[escape]) && wand_charges(game.inventory[escape])) {
                const Monster& m=game.monsters[adjacent];
                for(int d=0;d<4;++d) if(game.player.x+dxs[d]==m.pos.x && game.player.y+dys[d]==m.pos.y)
                    return wand_action(escape,d,"clear replenishing summoning blockade",m.pos);
            }
            int best = distance(f.pos, game.player), direction = -1;
            for(int d=0; d<4; ++d) {
                Position next{uint8_t(game.player.x+dxs[d]), uint8_t(game.player.y+dys[d])};
                if(blocked(next.x,next.y) || monster_at(next)!=NONE ||
                   feature_is_trap(observed_feature_at(game,next))) continue;
                int separation = distance(f.pos,next);
                if(separation > best) { best=separation; direction=d; }
            }
            if(direction >= 0) {
                Action a; a.kind=ActionKind::Move; a.dx=dxs[direction]; a.dy=dys[direction];
                a.goal="leave replenishing summoning fight"; return a;
            }
            break;
        }
    }
    // A bounded nearby holy opportunity can beat a dangerous melee exchange.
    if(holy_entries < 3) for(const MapFeature& f : observed_features(game)) {
        if(f.id != FEATURE_HOLY_SYMBOL || distance(f.pos, game.player) != 1) continue;
        bool target = false;
        for(const Monster& m : game.monsters)
            if(undead_or_demonic(m.type) && square_contains(f.pos, m.pos, 2) && !(m.state & MON_AFRAID)) target = true;
        if(target && monster_at(f.pos) == NONE) {
            ++holy_entries; Action a; a.kind = ActionKind::Move;
            a.dx = int8_t(int(f.pos.x) - game.player.x); a.dy = int8_t(int(f.pos.y) - game.player.y);
            a.goal = "holy pulse against unholy threats"; a.destination = f.pos; return a;
        }
    }
    int sight = find(POTION_SEE_INVISIBLE);
    if(sight>=0 && !game.see_invisible && !game.blindness && ring_bonus(RING_SEE_INVISIBLE)<=0)
        for(const Monster& m:game.monsters) if(m.type && distance(m.pos,game.player)<=2 &&
            ((monster_flags(m.type)&MON_NATURAL_INVIS) || monster_effect(m,MON_INVISIBLE)))
            return use(sight,"temporary sight against invisible contact");
    int restoration = find(POTION_RESTORE_VITALITY);
    if(restoration >= 0 && (game.vamp_drain || game.weakened)) return use(restoration,"restore drained vitality");
    if((game.confused || game.slowed) && find(POTION_EXTRA_HEALING)>=0) return use(find(POTION_EXTRA_HEALING),"clear harmful statuses with extra healing");
    int healing = find(POTION_HEALING);
    if(healing < 0 || game.confused || game.slowed) {
        int extra = find(POTION_EXTRA_HEALING);
        if(extra >= 0) healing = extra;
    }
    Danger risk = danger();
    if(retreat_floor != game.floor) { retreat_floor = game.floor; retreat_origin = {NONE,NONE}; retreat_steps = 0; retreat_threats = 0; }
    // Track current hostile identities, not a distance/visibility threshold.
    // Stepping beyond sight or around a corner must not renew the same retreat.
    bool encounter_alive = false;
    for(int i=0; i<MONSTERS; ++i) if((retreat_threats & (1u<<i)) &&
        game.monsters[i].type && hostile(game.monsters[i])) encounter_alive = true;
    if(!encounter_alive) { retreat_origin = {NONE,NONE}; retreat_steps = 0; retreat_threats = 0; }
    if(healing >= 0 && (game.hp*100 <= player_max_hp()*(adjacent >= 0 ? 65 : 45) ||
                      (game.weakened && game.hp < player_max_hp())))
        return use(healing,"heal and restore strength");
    if(threats && !game.haste && find(POTION_HASTE)>=0) return use(find(POTION_HASTE),"haste in combat");
    if(!game.fire_resistance && find(POTION_FIRE_RESISTANCE)>=0)
        for(const auto& m:game.monsters) if(m.type==DRAGON && can_see(m.pos)) return use(find(POTION_FIRE_RESISTANCE),"resist dragon fire");
    if(adjacent>=0 && player_can_see_monster(uint8_t(adjacent)) && can_see(game.monsters[adjacent].pos)) {
        if(find(SCROLL_HOLD_MONSTER)>=0 && !game.monsters[adjacent].stun && (risk.emergency || threats>1)) return use(find(SCROLL_HOLD_MONSTER),"hold nearby threats");
        if(find(SCROLL_MASS_TELEPORT)>=0 && risk.emergency) return use(find(SCROLL_MASS_TELEPORT),"displace visible threats");
        if(find(SCROLL_FIRE)>=0 && player_fire_effect()>0 && game.monsters[adjacent].type!=DRAGON) return use(find(SCROLL_FIRE),"fire burst with immunity");
    }
    if(risk.emergency) {
        int experience = find(POTION_EXPERIENCE);
        if(experience >= 0 && game.level <= 12) return use(experience,"emergency experience recovery");
        Action choice;
        if(emergency_control(choice)) return choice;
        // Bounded retreat avoids ping-pong, indefinite kiting and stalling.
        if(retreat_steps < 3 && retreat(retreat_origin,choice)) {
            for(int i=0; i<MONSTERS; ++i) if(game.monsters[i].type && hostile(game.monsters[i]) &&
                distance(game.monsters[i].pos,game.player)<=4) retreat_threats |= static_cast<uint16_t>(1u<<i);
            retreat_origin = game.player; ++retreat_steps; return choice;
        }
    }
    int uncurse = find(SCROLL_REMOVE_CURSE);
    if(uncurse >= 0) for(uint8_t s = 0; s < inventory_capacity(); ++s)
        if(equipped(s) && item_is_cursed(game.inventory[s])) return use(uncurse, "free cursed equipment", s);
    int food = find(FOOD);
    if(food >= 0 && game.hunger < (adjacent >= 0 ? 15 : 100)) return use(food,"eat before starvation");
    // Equipping even in danger is worthwhile for an obvious weapon/armor gain.
    int upgrade_slot = -1, upgrade_score = 0;
    for(int s = 0; s < inventory_capacity(); ++s) {
        Item i = game.inventory[s];
        if(bow_equipped() && is_weapon(i.type)) continue;
        if(!equipped(s) && upgrade(i) && equipment_score(i) > upgrade_score) {
            upgrade_slot = s; upgrade_score = equipment_score(i);
        }
    }
    if(upgrade_slot >= 0) {
        Item i = game.inventory[upgrade_slot];
        if(is_ring(i.type) && game.ring_slots[0] < INVENTORY && game.ring_slots[1] < INVENTORY) {
            int weaker = slot_score(game.ring_slots[0]) < slot_score(game.ring_slots[1]) ?
                game.ring_slots[0] : game.ring_slots[1];
            return use(weaker,"remove weaker ring");
        }
        return use(upgrade_slot,"equip upgrade");
    }
    int xp = find(POTION_EXPERIENCE);
    if(xp >= 0) return use(xp,"gain levels and recover HP");
    if(game.weakened && find(POTION_STRENGTH) >= 0) return use(find(POTION_STRENGTH),"restore strength");

    // Real ray scanning determines the first hittable monster, including mimics.
    for(int d = 0; d < 4; ++d) {
        auto ray = scan_ray(game.player,dxs[d],dys[d],6);
        if(ray.monster == NONE) continue;
        const Monster& m = game.monsters[ray.monster];
        bool dangerous = m.type >= ORC || game.hp < 12 || threats > 1;
        if(!dangerous || (ray.steps > 4 && m.type != LORD)) continue;
        for(uint8_t type : {uint8_t(POTION_PARALYSIS),uint8_t(POTION_POISON),uint8_t(POTION_CONFUSION),uint8_t(POTION_BLINDNESS),uint8_t(POTION_SLOWING)}) {
            bool needed = type == POTION_PARALYSIS ? m.stun == 0 && ray.steps <= 2 :
                type == POTION_POISON ? !monster_effect(m,MON_WEAKENED) :
                (type == POTION_CONFUSION || type == POTION_BLINDNESS) ? !monster_effect(m,MON_CONFUSED) && !m.stun :
                !monster_effect(m,MON_SLOWED);
            int s = find(type);
            if(s >= 0 && needed && (m.type >= INCUBUS || (adjacent >= 0 && game.hp < player_max_hp()*3/4))) {
                Action a; a.kind = ActionKind::Throw; a.slot = static_cast<uint8_t>(s);
                a.dx = dxs[d]; a.dy = dys[d]; a.goal = "control dangerous monster"; a.destination = m.pos; return a;
            }
        }
        for(uint8_t type : {uint8_t(WAND_LIGHTNING),uint8_t(WAND_STRIKING),uint8_t(WAND_ICE),uint8_t(WAND_FIRE),uint8_t(POTION_HARMING),uint8_t(POTION_FIRE)}) {
            int s = find(type); if(s < 0) continue;
            Item i = game.inventory[s];
            if(is_wand(type) && (wand_afflicted(i) || !wand_charges(i))) continue;
            if(type == WAND_FIRE && (m.type == DRAGON || !fire_safe(i,d))) continue;
            if(type == POTION_FIRE && (m.type == DRAGON || (player_fire_effect()<=0 && square_contains(ray.end,game.player,1)))) continue;
            Action a; a.kind = is_wand(type) ? ActionKind::Wand : ActionKind::Throw;
            a.slot = static_cast<uint8_t>(s); a.dx = dxs[d]; a.dy = dys[d];
            if(is_wand(type) && !wand_needs_direction(i)) a.dx = a.dy = 0;
            a.goal = "ranged damage"; a.destination = m.pos; return a;
        }
    }
    // This branch is unreachable in old-content replacement controls.
    int bow=best_bow();
    if(bow!=NONE) {
        Action shot;
        if(bow_cooldown) --bow_cooldown;
        if(!bow_cooldown && bow_opportunity(shot,bow) && (bow_equipped() || adjacent<0)) { bow_idle=0; return shot; }
        if(bow_equipped()) {
            ++bow_idle;
            int melee=best_melee();
            if(melee!=NONE && (adjacent>=0 || bow_idle>=2)) { bow_idle=0; bow_cooldown=4; return use(melee,"restore melee weapon"); }
        }
    }
    if(adjacent >= 0) {
        const Monster& m = game.monsters[adjacent];
        for(uint8_t t : {uint8_t(SCROLL_MASS_CONFUSE),uint8_t(SCROLL_TORMENT),uint8_t(SCROLL_MASS_POISON)}) {
            int s = find(t);
            bool needed = t == SCROLL_MASS_CONFUSE ? !monster_effect(m,MON_CONFUSED) && !m.stun :
                t == SCROLL_MASS_POISON ? !monster_effect(m,MON_WEAKENED) : m.hp >= 20;
            if(s >= 0 && needed && player_can_see_monster(static_cast<uint8_t>(adjacent)) &&
               (m.type >= INCUBUS || threats > 2)) return use(s,"control nearby threats");
        }
        if(game.hp < 12 && m.type >= ORC && !m.stun && !monster_effect(m,MON_CONFUSED)) {
            if(find(SCROLL_FEAR) >= 0 && player_can_see_monster(static_cast<uint8_t>(adjacent)))
                return use(find(SCROLL_FEAR),"escape unfavorable combat");
            if(find(SCROLL_TELEPORT) >= 0) return use(find(SCROLL_TELEPORT),"escape unfavorable combat");
        }
        Action a; a.kind = ActionKind::Move; a.dx = static_cast<int8_t>(int(m.pos.x)-game.player.x);
        a.dy = static_cast<int8_t>(int(m.pos.y)-game.player.y); a.goal = "melee combat"; a.destination = m.pos; return a;
    }
    for(uint8_t type : {uint8_t(POTION_STRENGTH),uint8_t(POTION_DEXTERITY)}) {
        int s = find(type);
        if(s >= 0 && (type == POTION_STRENGTH ? game.strength : game.dexterity) < 12) return use(s,"permanent stat improvement");
    }
    if(!threats && find(SCROLL_DETECT_TRAPS)>=0) return use(find(SCROLL_DETECT_TRAPS),"discover traps before travel");
    if(!game.levitation && find(POTION_LEVITATION)>=0) for(const auto& f:game.features)
        if(feature_is_trap(f.id) && !feature_is_hidden(f.id) && square_contains(game.player,f.pos,1)) return use(find(POTION_LEVITATION),"float over known traps");
    int enchant = find(SCROLL_ENCHANT);
    if(enchant >= 0) {
        int target = -1;
        if(game.amulet_slot < INVENTORY && game.inventory[game.amulet_slot].type == AMULET_SPEED &&
           item_value(game.inventory[game.amulet_slot]) < 3) target = game.amulet_slot;
        for(uint8_t s : game.ring_slots) if(target < 0 && s < inventory_capacity() &&
            game.inventory[s].type == RING_PROTECTION && item_value(game.inventory[s]) < 6) target = s;
        if(target < 0 && game.armor_slot < INVENTORY && equipment_enchant(game.inventory[game.armor_slot]) < 5)
            target = game.armor_slot;
        if(target < 0 && game.weapon_slot < INVENTORY && equipment_enchant(game.inventory[game.weapon_slot]) < 5)
            target = game.weapon_slot;
        if(target >= 0) return use(enchant,"enchant equipment",target);
    }

    // Replan shortest geometric routes through occupied cells. Adjacent
    // blockers are resolved by the combat priorities above. Treating moving
    // monsters as impassable made alternative corridor routes oscillate.
    Paths paths(true);
    // Limit optional detours to nearby visible features and available food.
    auto features = observed_features(game);
    if(!threats && game.hunger > 60 && feature_steps < 16) for(unsigned i = 0; i < features.size(); ++i) {
        const MapFeature& f = features[i];
        if(feature_attempted[i]) continue;
        if(f.id < FEATURE_FOUNTAIN || f.id > FEATURE_ALCHEMICAL_POOL || f.pos == game.player) continue;
        int d = paths.to(f.pos);
        if(d <= 0 || d > 8 || !choose_feature_choice(f.id).accept) continue;
        ++feature_steps;
        if(d == 1) feature_attempted[i] = true;
        return navigate(paths, f.pos, "use nearby dungeon feature", true);
    }
    // On ascent preserve resources and go directly to the surface.
    if(game.has_amulet) {
        if(game.player == game.up) { Action a; a.kind = ActionKind::Stairs; a.goal = "ascend and escape"; return a; }
        return navigate(paths, game.up, "ascend");
    }
    int best = -1, best_score = 0;
    for(int i = 0; i < GROUND_ITEMS; ++i) {
        const GroundItem& g = game.ground[i]; int worth = acquisition_value(g.item), d = paths.to(g.pos);
        if(abandoned_loot[i] || worth <= 0 || d < 0 || (g.item.type != YENDOR_AMULET && !fits(g.item) && replacement(g.item) < 0)) continue;
        int score = worth*100/(d+8);
        if(score > best_score) { best = i; best_score = score; }
    }
    if(best >= 0) {
        if(game.player == game.ground[best].pos) return pickup(best);
        return navigate(paths, game.ground[best].pos, "seek useful loot", game.ground[best].item.type != YENDOR_AMULET);
    }
    if(game.floor == FLOORS-1) {
        for(const Monster& m : game.monsters) if(m.type == LORD)
            return navigate(paths, m.pos, "defeat Lord");
        for(int i = 0; i < GROUND_ITEMS; ++i) if(game.ground[i].item.type == YENDOR_AMULET)
            return navigate(paths, game.ground[i].pos, "recover Yendor");
    }
    // Early, weak monsters provide levels and HP recovery. Avoid chasing bats.
    if(game.floor < 4 && !game.weakened && game.hp > player_max_hp()/2) {
        int target = -1, nearest = 10000;
        for(int i = 0; i < MONSTERS; ++i) {
            const Monster& m = game.monsters[i]; int d = paths.to(m.pos);
            if(!(m.state & MON_TRAP_SUMMONED) && m.type > BAT && m.type <= GOBLIN && d > 0 && d <= 16 && d < nearest) { nearest = d; target = i; }
        }
        if(target >= 0) return paths.move_to(game.monsters[target].pos,"gain early combat experience");
    }
    if(game.player == game.down && game.floor < FLOORS-1) {
        Action a; a.kind = ActionKind::Stairs; a.goal = "descend"; return a;
    }
    return navigate(paths, game.down, "descend");
}
}

namespace sim {
rogue::FeatureChoice choose_feature_choice(uint8_t type) {
    using namespace rogue;
    FeatureChoice choice;
    if(type < FEATURE_FOUNTAIN || type > FEATURE_ALCHEMICAL_POOL || game.paralyzed) return choice;
    bool pressure = false;
    for(const Monster& m : game.monsters)
        if(m.type && distance(m.pos, game.player) <= 2) pressure = true;
    if(type == FEATURE_HEALING_SPRING) choice.accept = game.hp < player_max_hp() || game.weakened || game.confused || game.slowed;
    else if(type == FEATURE_FOUNTAIN) choice.accept = game.hp >= 10 && (game.hp < player_max_hp() || game.hunger < 180 || game.weakened);
    else choice.accept = !pressure && game.hp >= 10;
    if(type == FEATURE_ENCHANTED_FORGE || type == FEATURE_ALCHEMICAL_POOL) {
        int best = -1;
        for(uint8_t s = 0; s < inventory_capacity(); ++s) if(feature_accepts_item(type, s)) {
            const Item& i = game.inventory[s];
            if(type == FEATURE_ENCHANTED_FORGE && equipment_enchant(i) == MAX_EQUIPMENT_ENCHANT) continue;
            int score = is_equipment(i.type) ? equipment_score(i) + (equipped(s) ? 100 : 0) :
                is_wand(i.type) && !wand_afflicted(i) ? 60 - wand_charges(i) : 0;
            if(score > best) { best = score; choice.slot = s; }
        }
        choice.accept = choice.accept && choice.slot != NONE;
    } else if(type == FEATURE_SACRIFICIAL_ALTAR) {
        int least = 10000;
        for(uint8_t s = 0; s < inventory_capacity(); ++s) if(feature_accepts_item(type, s) && !equipped(s)) {
            Item i = game.inventory[s];
            if(i.type == FOOD || i.type == POTION_HEALING || is_artifact(i.type)) continue;
            int score = value(i);
            if(score < least) { least = score; choice.slot = s; }
        }
        choice.accept = choice.accept && least <= 0;
    }
    return choice;
}
}
