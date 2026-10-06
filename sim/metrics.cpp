#include "metrics.hpp"
#include "agent.hpp"
#include "game.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "game_internal.hpp"
#include "trace.hpp"
#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace sim {
namespace {
Collector* collector = nullptr;
Cause cause = Cause::Other;
uint8_t cause_type = 0;
template<class T> struct Field { const char* name; uint64_t T::* member; };
const Field<ItemMetrics> item_fields[] = {
    {"generated",&ItemMetrics::generated},{"reached",&ItemMetrics::reached},
    {"picked_up",&ItemMetrics::picked_up},{"used",&ItemMetrics::used},{"consumed",&ItemMetrics::consumed},
    {"equipped",&ItemMetrics::equipped},{"dropped",&ItemMetrics::dropped},{"discarded",&ItemMetrics::discarded},
    {"carried",&ItemMetrics::carried},{"charges_used",&ItemMetrics::charges_used},
    {"potions_drunk",&ItemMetrics::drunk},{"potions_thrown",&ItemMetrics::thrown},
    {"scrolls_read",&ItemMetrics::scrolls_read},{"turns_equipped",&ItemMetrics::turns_equipped},
    {"wands_picked_up",&ItemMetrics::wands_picked_up},{"wands_activated",&ItemMetrics::wands_activated}};
const Field<MonsterMetrics> monster_fields[] = {
    {"generated",&MonsterMetrics::generated},{"encountered",&MonsterMetrics::encountered},
    {"engaged",&MonsterMetrics::engaged},{"killed",&MonsterMetrics::killed},
    {"player_attacks",&MonsterMetrics::player_attacks},{"damage_to_monster",&MonsterMetrics::damage_taken},
    {"monster_attacks",&MonsterMetrics::attacks},{"monster_hits",&MonsterMetrics::hits},
    {"damage_to_player",&MonsterMetrics::player_damage},{"deaths_caused",&MonsterMetrics::deaths},
    {"poison",&MonsterMetrics::poison},{"confusion",&MonsterMetrics::confusion},
    {"paralysis",&MonsterMetrics::paralysis},{"fire",&MonsterMetrics::fire}};
template<class T, size_t N> void field_header(std::ostream& o,const Field<T> (&fields)[N]) {
    for(const auto& f:fields) o<<','<<f.name;
    o<<'\n';
}
template<class T, size_t N> void field_values(std::ostream& o,const T& metric,const Field<T> (&fields)[N]) {
    for(const auto& f:fields) o<<','<<metric.*(f.member);
    o<<'\n';
}
template<class T, size_t N> bool active(const T& metric,const Field<T> (&fields)[N]) {
    for(const auto& f:fields) if(metric.*(f.member)) return true;
    return false;
}
void key(std::ostream& o,const RunMetrics& r) { o<<r.seed<<','<<r.effective_seed<<','<<r.agent; }
void visit_key(std::ostream& o,const RunMetrics& r,const FloorMetrics& f) {
    key(o,r); o<<','<<f.visit<<','<<f.floor<<','<<(f.ascent ? "ascent" : "descent");
}
uint16_t units(rogue::Item i) {
    return rogue::is_stackable(i.type) ? rogue::item_value(i) : 1;
}
std::string csv(const std::string& text) {
    std::string result = "\"";
    for(char c : text) { if(c == '"') result += '"'; result += c; }
    return result+'"';
}
}
DamageScope::DamageScope(Cause c, uint8_t type) : previous_cause(cause), previous_type(cause_type) {
    cause = c; cause_type = type;
}
DamageScope::~DamageScope() { cause = previous_cause; cause_type = previous_type; }
CollectScope::CollectScope(Collector& c) : previous(collector) { collector = &c; }
CollectScope::~CollectScope() { collector = previous; }
void event(EventKind kind, uint8_t index, uint8_t type, uint16_t amount, uint8_t detail) {
    if(collector) collector->handle(kind,index,type,amount,detail);
}
void after_floor_generation() {
    if(!collector || !collector->experiment) return;
    auto rng=rogue::game.random_state;
    collector->experiment->apply(rogue::game,{rogue::game.run_seed,collector->data.floors_entered+1},collector->data.interventions);
    if(rogue::game.random_state!=rng) throw std::runtime_error("experiment consumed gameplay RNG");
}
const char* item_name(uint8_t type) {
    // Stable schema-2 labels, independent of renamed C++ enum identifiers.
    static const char* names[] = {"NO_ITEM","FOOD","ARROWS","HEALING","CONFUSION","POISON","HARMING",
        "STRENGTH","DEXTERITY","PARALYSIS","SLOWING","EXPERIENCE","INVISIBILITY",
        "LONG_SWORD","DAGGER","SPEAR","MACE","TWO_HANDED_SWORD","SHORT_BOW","LONG_BOW","CHAIN_MAIL","LEATHER_ARMOR",
        "RING_MAIL","SCALE_MAIL","SPLINT_MAIL","PLATE_MAIL","YENDOR_AMULET","RING_SEE_INVISIBLE",
        "RING_STRENGTH","RING_DEXTERITY","RING_PROTECTION","RING_FIRE_IMMUNITY","RING_ATTACK",
        "RING_SUSTENANCE","RING_INVISIBILITY","AMULET_SPEED","AMULET_CLARITY","AMULET_CONSERVATION",
        "AMULET_REGENERATION","AMULET_VAMPIRE","AMULET_IRONBLOOD","AMULET_VITALITY","AMULET_WISDOM",
        "SCROLL_IDENTIFY","SCROLL_ENCHANT","SCROLL_REMOVE_CURSE","SCROLL_TELEPORT","SCROLL_MAPPING",
        "SCROLL_FEAR","SCROLL_TORMENT","SCROLL_MASS_CONFUSE","SCROLL_MASS_POISON","WAND_FORCE",
        "WAND_TELEPORT","WAND_DIGGING","WAND_FIRE","WAND_STRIKING","WAND_ICE","WAND_POLYMORPH"};
    static_assert(sizeof(names)/sizeof(*names) == rogue::WAND_POLYMORPH+1, "item names changed");
    return type <= rogue::WAND_POLYMORPH ? names[type] : "INVALID";
}
const char* monster_name(uint8_t type) {
    static const char* names[] = {"NO_MONSTER","BAT","SNAKE","RATTLESNAKE","ZOMBIE","GOBLIN",
        "PHANTOM","ORC","TARANTULA","HOBGOBLIN","MIMIC","INCUBUS","TROLL","GRIFFIN","DRAGON","ANGEL","LORD"};
    return type <= rogue::LORD ? names[type] : "INVALID";
}
size_t Collector::new_wand() {
    wand_flags.push_back(0); return wand_flags.size()-1;
}
void Collector::prepare_action(const Action& a) {
    pickup_slot = drop_slot = rogue::NONE;
    if(!enabled) return;
    if(a.kind == ActionKind::Take || a.kind == ActionKind::Swap) {
        pickup_slot = a.slot;
        if(a.kind == ActionKind::Take)
            for(uint8_t s=0; s<rogue::INVENTORY; ++s)
                if(!rogue::game.inventory[s].type) { pickup_slot=s; break; }
    }
    if(a.kind == ActionKind::Drop) drop_slot = a.slot;
}
void Collector::enter_floor() {
    const auto& g = rogue::game;
    close_floor(true);
    ++data.floors_entered;
    data.deepest = std::max(data.deepest,int(g.floor));
    reached.fill(false); encountered.fill(false); engaged.fill(false);
    FloorMetrics f; f.floor = g.floor; f.visit = data.floors_entered;
    f.ascent = g.has_amulet; f.entry_hp = f.exit_hp = g.hp; f.entry_level = f.exit_level = g.level;
    f.entry_max_hp=rogue::player_max_hp(); f.entry_strength=rogue::player_strength();
    f.entry_dexterity=rogue::player_dexterity();
    // Exactly end_turn's effective enemy-turn budget (no separate speed model).
    int speed=int(g.speed)-rogue::amulet_bonus(rogue::AMULET_SPEED);
    if(g.slowed) speed*=2;
    f.entry_speed=uint8_t(std::max(1,speed)); f.entry_hunger=g.hunger;
    f.entry_armor_rating=rogue::player_armor_rating(); f.entry_armor_enchant=rogue::player_armor_enchant();
    if(g.weapon_slot<rogue::INVENTORY) {
        auto i=g.inventory[g.weapon_slot]; f.entry_weapon_type=i.type; f.entry_weapon_enchant=rogue::equipment_enchant(i);
    }
    if(g.armor_slot<rogue::INVENTORY) f.entry_armor_type=g.inventory[g.armor_slot].type;
    for(auto i:g.inventory) {
        if(i.type==rogue::FOOD) f.entry_food_units+=rogue::item_value(i);
        if(i.type==rogue::POTION_HEALING) f.entry_healing_units+=rogue::item_value(i);
        if(i.type==rogue::POTION_CONFUSION || i.type==rogue::POTION_PARALYSIS || i.type==rogue::POTION_SLOWING ||
           i.type==rogue::POTION_STRENGTH || i.type==rogue::POTION_INVISIBILITY || i.type==rogue::SCROLL_FEAR ||
           i.type==rogue::SCROLL_TELEPORT || i.type==rogue::SCROLL_MASS_CONFUSE)
                f.entry_control_units+=rogue::item_value(i);
        if(rogue::is_wand(i.type) && !rogue::wand_afflicted(i) && i.type!=rogue::WAND_DIGGING) {
            auto charges=rogue::wand_charges(i);
            f.entry_wand_charges+=charges;
            if(i.type==rogue::WAND_FIRE || i.type==rogue::WAND_STRIKING || i.type==rogue::WAND_ICE)
                f.entry_offensive_charges+=charges;
            if(i.type==rogue::WAND_FORCE || i.type==rogue::WAND_TELEPORT || i.type==rogue::WAND_POLYMORPH)
                f.entry_emergency_charges+=charges;
        }
    }
    for(auto slot:{g.ring_slots[0],g.ring_slots[1]})
        if(slot<rogue::INVENTORY && g.inventory[slot].type==rogue::RING_INVISIBILITY)
            f.entry_ring_invisibility=true;
    f.entry_speed_amulet=g.amulet_slot<rogue::INVENTORY &&
        g.inventory[g.amulet_slot].type==rogue::AMULET_SPEED;
    f.entry_invisible=rogue::player_is_invisible();
    const auto& d=rogue::generation::diagnostics;
    const char* archetypes[]={"CHAMBERS","WARREN","FORTRESS","RUINS"};
    f.archetype=archetypes[rogue::generation::archetype(rogue::generation::floor_seed(rogue::generation::LAYOUT))];
    f.floor_tiles=d.floor_tiles; f.major_features=d.major_features; f.corridors=d.corridors;
    f.loops=d.loops; f.open_connections=d.open_connections;
    std::copy(std::begin(d.families),std::end(d.families),f.families.begin());
    entry_items=data.items; entry_monsters=data.monsters;
    data.floors.push_back(f);
    if(!enabled) return;
    for(size_t s=0; s<rogue::INVENTORY; ++s) {
        if(!rogue::is_wand(g.inventory[s].type)) inventory_wands[s]=0;
        else if(!inventory_wands[s]) inventory_wands[s]=new_wand();
    }
    for(size_t s=0; s<rogue::GROUND_ITEMS; ++s) {
        const auto& i=g.ground[s];
        ground_wands[s]=rogue::is_wand(i.item.type) ? new_wand() : 0;
        if(i.item.type) data.items[i.item.type].generated += units(i.item);
        if(rogue::is_ammo(i.item.type)) {
            ++data.ranged.bundles; data.ranged.generated += units(i.item);
            ++data.floors.back().arrow_bundles; data.floors.back().arrow_generated += units(i.item);
            if(data.ranged.first_generated == 255) data.ranged.first_generated = g.floor;
        }
    }
    for(const auto& m : g.monsters) if(m.type) ++data.monsters[m.type].generated;
}
void Collector::close_floor(bool exited) {
    if(data.floors.empty() || data.floors.back().exited) return;
    auto& f = data.floors.back();
    if(f.visit != closed_ranged_visit && !f.ascent) {
        generation_gap = f.arrow_bundles ? 0 : generation_gap + 1;
        acquisition_gap = !f.bow_owned || f.arrow_picked ? 0 : acquisition_gap + 1;
        data.ranged.generation_gap = std::max(data.ranged.generation_gap, generation_gap);
        data.ranged.acquisition_gap = std::max(data.ranged.acquisition_gap, acquisition_gap);
        closed_ranged_visit = f.visit;
    }
    for(size_t i=0;i<data.items.size();++i) for(const auto& field:item_fields)
        f.items[i].*(field.member)=data.items[i].*(field.member)-entry_items[i].*(field.member);
    for(size_t i=0;i<data.monsters.size();++i) for(const auto& field:monster_fields)
        f.monsters[i].*(field.member)=data.monsters[i].*(field.member)-entry_monsters[i].*(field.member);
    f.exit_hp = rogue::game.hp; f.exit_level = rogue::game.level;
    if(exited) { f.exited = true; ++data.floors_exited; }
}
void Collector::observe() {
    if(!enabled) return;
    const auto& g = rogue::game;
    if(!data.floors.empty()) for(auto i:g.inventory)
        if(rogue::is_bow(i.type)) data.floors.back().bow_owned=true;
    for(int i = 0; i < rogue::GROUND_ITEMS; ++i)
        if(!reached[i] && g.ground[i].item.type && g.ground[i].pos == g.player) {
            reached[i] = true; data.items[g.ground[i].item.type].reached += units(g.ground[i].item);
        }
    for(int i = 0; i < rogue::MONSTERS; ++i)
        if(!encountered[i] && g.monsters[i].type && rogue::can_see(g.monsters[i].pos)) {
            encountered[i] = true; ++data.monsters[g.monsters[i].type].encountered;
        }
}
void Collector::handle(EventKind k, uint8_t index, uint8_t type, uint16_t amount, uint8_t detail) {
    if(trace) trace_event(*trace,k,index,type,amount);
    if(k == EventKind::FloorEntered) { enter_floor(); return; }
    if(k == EventKind::FloorExited) { close_floor(true); return; }
    if(k == EventKind::Turn) {
        ++data.turns;
        if(enabled) {
            uint8_t weapon = rogue::game.weapon_slot < rogue::INVENTORY ?
                rogue::game.inventory[rogue::game.weapon_slot].type : rogue::NO_ITEM;
            if(weapon != last_weapon) {
                if(rogue::is_bow(weapon)) ++data.ranged.switches_to;
                if(rogue::is_bow(last_weapon)) ++data.ranged.switches_away;
                last_weapon = weapon;
            }
            if(rogue::is_bow(weapon)) ++data.ranged.bow_turns;
        }
        if(!data.floors.empty()) ++data.floors.back().turns;
        if(enabled) for(uint8_t s : {rogue::game.weapon_slot,rogue::game.armor_slot,rogue::game.amulet_slot,
                                   rogue::game.ring_slots[0],rogue::game.ring_slots[1]})
            if(s < rogue::INVENTORY) ++data.items[rogue::game.inventory[s].type].turns_equipped;
        return;
    }
    if(k == EventKind::PlayerDamage && amount) {
        switch(cause) {
        case Cause::Monster: data.death_cause = monster_name(cause_type); break;
        case Cause::Starvation: data.death_cause = "starvation"; break;
        case Cause::Item: data.death_cause = std::string("item:")+item_name(cause_type); break;
        case Cause::Fire: data.death_cause = cause_type ? std::string("fire:")+monster_name(cause_type) : "fire"; break;
        default: data.death_cause = "other"; break;
        }
        if(enabled && cause_type && (cause == Cause::Monster || cause == Cause::Fire)) {
            data.monsters[cause_type].player_damage += amount;
            if(amount >= rogue::game.hp) ++data.monsters[cause_type].deaths;
        }
    }
    if(!enabled) return;
    FloorMetrics* f = data.floors.empty() ? nullptr : &data.floors.back();
    auto engage = [&] {
        if(index < rogue::MONSTERS && !engaged[index]) {
            engaged[index] = true; ++data.monsters[type].engaged;
            if(!encountered[index]) { encountered[index] = true; ++data.monsters[type].encountered; }
        }
    };
    switch(k) {
    case EventKind::ArrowFired:
        ++data.items[rogue::ARROWS].used; ++data.items[rogue::ARROWS].consumed;
        if(f) ++f->consumables;
        if(rogue::is_bow(type)) {
            ++data.ranged.fired;
            ++data.ranged.bows[type == rogue::LONG_BOW].shots;
            ++data.ranged.distances[std::min<unsigned>(amount,6)].shots;
        } else ++data.ranged.thrown;
        break;
    case EventKind::ArrowTarget:
        if(rogue::is_bow(type) && index < rogue::MONSTERS) {
            uint8_t target = rogue::game.monsters[index].type;
            bool hit = detail & 0x80;
            auto damage = std::min<uint16_t>(amount, rogue::game.monsters[index].hp);
            for(auto* shot : {&data.ranged.bows[type == rogue::LONG_BOW],
                             &data.ranged.distances[detail & 0x7f], &data.ranged.targets[target]}) {
                shot->hits += hit; shot->damage += damage;
                shot->kills += hit && amount >= rogue::game.monsters[index].hp;
            }
            ++data.ranged.targets[target].shots;
        }
        break;
    case EventKind::PlayerDamage: if(f) f->damage_taken += amount; break;
    case EventKind::PlayerAttack: ++data.monsters[type].player_attacks; engage(); break;
    case EventKind::MonsterDamage:
        data.monsters[type].damage_taken += amount;
        if(detail) { engage(); if(f) f->damage_dealt += amount; }
        break;
    case EventKind::MonsterKilled: ++data.monsters[type].killed; if(f) ++f->kills; break;
    case EventKind::MonsterAttack: ++data.monsters[type].attacks; engage(); break;
    case EventKind::MonsterHit: ++data.monsters[type].hits; break;
    case EventKind::Special:
        if(detail == uint8_t(Special::Poison)) ++data.monsters[type].poison;
        if(detail == uint8_t(Special::Confusion)) ++data.monsters[type].confusion;
        if(detail == uint8_t(Special::Paralysis)) ++data.monsters[type].paralysis;
        if(detail == uint8_t(Special::Fire)) ++data.monsters[type].fire;
        break;
    case EventKind::GeneratedItem:
        data.items[type].generated += amount;
        if(index < rogue::GROUND_ITEMS) {
            reached[index] = false; ground_wands[index] = rogue::is_wand(type) ? new_wand() : 0;
        }
        break;
    case EventKind::Pickup:
        data.items[type].picked_up += amount; if(f) f->pickups += amount;
        if(rogue::is_ammo(type)) {
            data.ranged.picked += amount; if(f) f->arrow_picked += amount;
            if(data.ranged.first_picked == 255) data.ranged.first_picked = rogue::game.floor;
        }
        if(f && rogue::is_bow(type)) f->bow_owned=true;
        if(index < rogue::GROUND_ITEMS) {
            reached[index] = false;
            bool swapped = rogue::game.ground[index].item.type != rogue::NO_ITEM;
            size_t incoming = ground_wands[index];
            if(rogue::is_wand(type)) {
                if(!incoming) incoming = new_wand();
                if(!(wand_flags[incoming]&1)) { ++data.items[type].wands_picked_up; wand_flags[incoming]|=1; }
            }
            size_t outgoing = swapped && pickup_slot < rogue::INVENTORY ? inventory_wands[pickup_slot] : 0;
            if(pickup_slot < rogue::INVENTORY && (rogue::is_wand(type) || swapped))
                inventory_wands[pickup_slot] = rogue::is_wand(type) ? incoming : 0;
            ground_wands[index] = outgoing;
        }
        break;
    case EventKind::Dropped:
        data.items[type].dropped += amount;
        if(index < rogue::GROUND_ITEMS) {
            reached[index] = false;
            if(drop_slot < rogue::INVENTORY)
                ground_wands[index] = rogue::is_wand(type) && amount ? inventory_wands[drop_slot] : 0;
        }
        if(drop_slot < rogue::INVENTORY) inventory_wands[drop_slot] = 0;
        break;
    case EventKind::Discarded: data.items[type].discarded += amount; break;
    case EventKind::Equipped: ++data.items[type].equipped; break;
    case EventKind::ItemUsed:
        ++data.items[type].used;
        if(rogue::is_potion(type)) ++data.items[type].drunk;
        if(rogue::is_scroll(type)) ++data.items[type].scrolls_read;
        if(f && (type == rogue::FOOD || rogue::is_potion(type) || rogue::is_scroll(type))) ++f->consumables;
        break;
    case EventKind::Consumed: data.items[type].consumed += amount; break;
    case EventKind::PotionThrown:
        ++data.items[type].used; ++data.items[type].thrown;
        if(f) ++f->consumables;
        break;
    case EventKind::ChargeUsed:
        ++data.items[type].used; ++data.items[type].charges_used;
        if(index < rogue::INVENTORY) {
            size_t& id = inventory_wands[index];
            if(!id) id = new_wand();
            if(!(wand_flags[id]&2)) { ++data.items[type].wands_activated; wand_flags[id]|=2; }
        }
        break;
    case EventKind::MonsterChanged: if(index < rogue::MONSTERS) encountered[index] = engaged[index] = false; break;
    default: break;
    }
}
void write_runs_header(std::ostream& o) {
    o << "seed,effective_seed,agent,result,actions,turns,score,deepest_floor,final_floor,level,hp,max_hp,has_yendor,floors_entered,floors_exited,stuck,reason,death_cause,action_hash\n";
}
void write_floors_header(std::ostream& o) {
    o << "seed,effective_seed,agent,visit,floor,direction,entry_hp,exit_hp,entry_level,exit_level,actions,turns,monsters_killed,damage_taken,damage_dealt,items_picked_up,consumables_used,exited,entry_max_hp,entry_strength,entry_dexterity,entry_speed,entry_hunger,entry_armor_rating,entry_food_units,entry_healing_units,entry_weapon_type,entry_weapon_enchant,entry_armor_type,entry_armor_enchant,arrow_bundles,arrow_generated,arrow_picked,bow_owned,archetype,floor_tiles,major_features,corridors,loops,open_connections";
    for(int i=0;i<16;++i) o<<",feature_family_"<<i;
    o<<'\n';
}
void write_items_header(std::ostream& o) {
    o << "seed,effective_seed,agent,item_type,item"; field_header(o,item_fields);
}
void write_monsters_header(std::ostream& o) {
    o << "seed,effective_seed,agent,monster_type,monster"; field_header(o,monster_fields);
}
void write_run(std::ostream& o, const RunMetrics& r) {
    o << r.seed << ',' << r.effective_seed << ',' << r.agent << ',' << r.result << ',' << r.actions << ',' << r.turns
      << ',' << r.score << ',' << r.deepest << ',' << r.final_floor << ',' << r.level << ',' << r.hp << ',' << r.max_hp
      << ',' << r.has_yendor << ',' << r.floors_entered << ',' << r.floors_exited << ',' << r.stuck << ','
      << csv(r.reason) << ',' << csv(r.death_cause) << ',' << r.action_hash << '\n';
}
void write_floors(std::ostream& o, const RunMetrics& r) {
    for(const auto& f : r.floors) { visit_key(o,r,f); o << ',' << f.entry_hp << ',' << f.exit_hp << ',' << f.entry_level << ','
      << f.exit_level << ',' << f.actions << ',' << f.turns << ',' << f.kills << ',' << f.damage_taken << ','
      << f.damage_dealt << ',' << f.pickups << ',' << f.consumables << ',' << f.exited
      << ',' << f.entry_max_hp << ',' << f.entry_strength << ',' << f.entry_dexterity << ',' << f.entry_speed
      << ',' << f.entry_hunger << ',' << f.entry_armor_rating << ',' << f.entry_food_units << ',' << f.entry_healing_units
      << ',' << f.entry_weapon_type << ',' << f.entry_weapon_enchant << ',' << f.entry_armor_type << ',' << f.entry_armor_enchant
      << ',' << f.arrow_bundles << ',' << f.arrow_generated << ',' << f.arrow_picked << ',' << f.bow_owned
      << ',' << f.archetype << ',' << f.floor_tiles << ',' << f.major_features << ',' << f.corridors << ',' << f.loops << ',' << f.open_connections;
      for(auto family:f.families) o<<','<<int(family);
      o<<'\n';
    }
}
void write_items(std::ostream& o, const RunMetrics& r) {
    for(size_t i = 1; i < r.items.size(); ++i) {
        key(o,r); o<<','<<i<<','<<item_name(uint8_t(i)); field_values(o,r.items[i],item_fields);
    }
}
void write_monsters(std::ostream& o, const RunMetrics& r) {
    for(size_t i = 1; i < r.monsters.size(); ++i) {
        key(o,r); o<<','<<i<<','<<monster_name(uint8_t(i)); field_values(o,r.monsters[i],monster_fields);
    }
}
void write_visit_items_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,visit,floor,direction,item_type,item"; field_header(o,item_fields);
}
void write_visit_monsters_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,visit,floor,direction,monster_type,monster"; field_header(o,monster_fields);
}
void write_visit_items(std::ostream& o,const RunMetrics& r) {
    for(const auto& f:r.floors) for(size_t i=1;i<f.items.size();++i) if(active(f.items[i],item_fields)) {
        visit_key(o,r,f); o<<','<<i<<','<<item_name(uint8_t(i)); field_values(o,f.items[i],item_fields);
    }
}
void write_visit_monsters(std::ostream& o,const RunMetrics& r) {
    for(const auto& f:r.floors) for(size_t i=1;i<f.monsters.size();++i) if(active(f.monsters[i],monster_fields)) {
        visit_key(o,r,f); o<<','<<i<<','<<monster_name(uint8_t(i)); field_values(o,f.monsters[i],monster_fields);
    }
}
void write_interventions_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,visit,floor,direction,experiment,variant,operation,from_type,to_type,count\n";
}
void write_interventions(std::ostream& o,const RunMetrics& r) {
    for(const auto& i:r.interventions) {
        key(o,r); o<<','<<i.visit<<','<<i.floor<<','<<(i.ascent ? "ascent" : "descent")<<','
            <<csv(r.experiment)<<','<<csv(r.variant)<<','<<i.operation<<','<<i.from<<','<<i.to<<','<<i.count<<'\n';
    }
}
void write_entry_state_header(std::ostream& o) {
    o<<"seed,effective_seed,agent,visit,floor,direction,entry_control_units,entry_wand_charges,"
        "entry_offensive_charges,entry_emergency_charges,entry_ring_invisibility,entry_speed_amulet,entry_invisible\n";
}
void write_entry_state(std::ostream& o,const RunMetrics& r) {
    for(const auto& f:r.floors) {
        visit_key(o,r,f);
        o<<','<<f.entry_control_units<<','<<f.entry_wand_charges<<','<<f.entry_offensive_charges
         <<','<<f.entry_emergency_charges<<','<<f.entry_ring_invisibility<<','<<f.entry_speed_amulet
         <<','<<f.entry_invisible<<'\n';
    }
}
void write_ranged_header(std::ostream& o) {
    o << "seed,effective_seed,agent,bundles,generated,picked,fired,thrown,carried,bow_turns,switches_to,switches_away,first_generated,first_picked,generation_gap,acquisition_gap";
    for(const char* bow:{"short","long"}) for(const char* field:{"shots","hits","damage","kills"}) o << ',' << bow << '_' << field;
    for(int d=0;d<=6;++d) for(const char* field:{"shots","hits","damage","kills"}) o << ",distance" << d << '_' << field;
    for(int t=1;t<=rogue::LORD;++t) for(const char* field:{"shots","hits","damage","kills"}) o << ',' << monster_name(t) << '_' << field;
    o << '\n';
}
void write_ranged(std::ostream& o,const RunMetrics& r) {
    key(o,r); const auto& m=r.ranged;
    o << ',' << m.bundles << ',' << m.generated << ',' << m.picked << ',' << m.fired << ',' << m.thrown
      << ',' << m.carried << ',' << m.bow_turns << ',' << m.switches_to << ',' << m.switches_away
      << ',' << m.first_generated << ',' << m.first_picked << ',' << m.generation_gap << ',' << m.acquisition_gap;
    auto shot=[&](const ShotMetrics& s) { o << ',' << s.shots << ',' << s.hits << ',' << s.damage << ',' << s.kills; };
    for(auto s:m.bows) shot(s);
    for(auto s:m.distances) shot(s);
    for(int t=1;t<=rogue::LORD;++t) shot(m.targets[t]);
    o << '\n';
}
const std::array<CsvStream,CSV_STREAM_COUNT> csv_streams{{
    {"runs.csv",write_runs_header,write_run},{"floors.csv",write_floors_header,write_floors},
    {"items.csv",write_items_header,write_items},{"monsters.csv",write_monsters_header,write_monsters},
    {"visit_items.csv",write_visit_items_header,write_visit_items},
    {"visit_monsters.csv",write_visit_monsters_header,write_visit_monsters},
    {"interventions.csv",write_interventions_header,write_interventions},
    {"ranged.csv",write_ranged_header,write_ranged}}};
}
