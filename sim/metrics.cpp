#include "metrics.hpp"
#include "game.hpp"
#include "world.hpp"
#include "trace.hpp"
#include <algorithm>
#include <ostream>

namespace sim {
namespace {
Collector* collector = nullptr;
Cause cause = Cause::Other;
uint8_t cause_type = 0;
uint16_t units(rogue::Item i) {
    return i.type == rogue::FOOD || rogue::is_potion(i.type) || rogue::is_scroll(i.type) ? rogue::item_value(i) : 1;
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
const char* item_name(uint8_t type) {
    static const char* names[] = {"NO_ITEM","FOOD","HEALING","CONFUSION","POISON","HARMING",
        "STRENGTH","DEXTERITY","PARALYSIS","SLOWING","EXPERIENCE","INVISIBILITY",
        "LONG_SWORD","DAGGER","SPEAR","MACE","TWO_HANDED_SWORD","CHAIN_MAIL","LEATHER_ARMOR",
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
void Collector::enter_floor() {
    const auto& g = rogue::game;
    close_floor(true);
    ++data.floors_entered;
    data.deepest = std::max(data.deepest,int(g.floor));
    reached.fill(false); encountered.fill(false); engaged.fill(false);
    FloorMetrics f; f.floor = g.floor; f.visit = data.floors_entered;
    f.ascent = g.has_amulet; f.entry_hp = f.exit_hp = g.hp; f.entry_level = f.exit_level = g.level;
    data.floors.push_back(f);
    if(!enabled) return;
    for(const auto& i : g.ground) if(i.item.type) data.items[i.item.type].generated += units(i.item);
    for(const auto& m : g.monsters) if(m.type) ++data.monsters[m.type].generated;
}
void Collector::close_floor(bool exited) {
    if(data.floors.empty() || data.floors.back().exited) return;
    auto& f = data.floors.back();
    f.exit_hp = rogue::game.hp; f.exit_level = rogue::game.level;
    if(exited) { f.exited = true; ++data.floors_exited; }
}
void Collector::observe() {
    if(!enabled) return;
    const auto& g = rogue::game;
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
    case EventKind::GeneratedItem: data.items[type].generated += amount; if(index < rogue::GROUND_ITEMS) reached[index] = false; break;
    case EventKind::Pickup: data.items[type].picked_up += amount; if(f) f->pickups += amount; if(index < rogue::GROUND_ITEMS) reached[index] = false; break;
    case EventKind::Dropped: data.items[type].dropped += amount; if(index < rogue::GROUND_ITEMS) reached[index] = false; break;
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
    case EventKind::ChargeUsed: ++data.items[type].used; ++data.items[type].charges_used; break;
    case EventKind::MonsterChanged: if(index < rogue::MONSTERS) encountered[index] = engaged[index] = false; break;
    default: break;
    }
}
void write_runs_header(std::ostream& o) {
    o << "seed,effective_seed,agent,result,actions,turns,score,deepest_floor,final_floor,level,hp,max_hp,has_yendor,floors_entered,floors_exited,stuck,reason,death_cause,action_hash\n";
}
void write_floors_header(std::ostream& o) {
    o << "seed,agent,visit,floor,direction,entry_hp,exit_hp,entry_level,exit_level,actions,turns,monsters_killed,damage_taken,damage_dealt,items_picked_up,consumables_used,exited\n";
}
void write_items_header(std::ostream& o) {
    o << "seed,agent,item_type,item,generated,reached,picked_up,used,consumed,equipped,dropped,discarded,carried,charges_used,potions_drunk,potions_thrown,scrolls_read,turns_equipped\n";
}
void write_monsters_header(std::ostream& o) {
    o << "seed,agent,monster_type,monster,generated,encountered,engaged,killed,player_attacks,damage_taken,attacks,hits,player_damage,deaths,poison,confusion,paralysis,fire\n";
}
void write_run(std::ostream& o, const RunMetrics& r) {
    o << r.seed << ',' << r.effective_seed << ',' << r.agent << ',' << r.result << ',' << r.actions << ',' << r.turns
      << ',' << r.score << ',' << r.deepest << ',' << r.final_floor << ',' << r.level << ',' << r.hp << ',' << r.max_hp
      << ',' << r.has_yendor << ',' << r.floors_entered << ',' << r.floors_exited << ',' << r.stuck << ','
      << csv(r.reason) << ',' << csv(r.death_cause) << ',' << r.action_hash << '\n';
}
void write_floors(std::ostream& o, const RunMetrics& r) {
    for(const auto& f : r.floors) o << r.seed << ',' << r.agent << ',' << f.visit << ',' << f.floor << ','
      << (f.ascent ? "ascent" : "descent") << ',' << f.entry_hp << ',' << f.exit_hp << ',' << f.entry_level << ','
      << f.exit_level << ',' << f.actions << ',' << f.turns << ',' << f.kills << ',' << f.damage_taken << ','
      << f.damage_dealt << ',' << f.pickups << ',' << f.consumables << ',' << f.exited << '\n';
}
void write_items(std::ostream& o, const RunMetrics& r) {
    for(size_t i = 1; i < r.items.size(); ++i) {
        const auto& m = r.items[i];
        o << r.seed << ',' << r.agent << ',' << i << ',' << item_name(static_cast<uint8_t>(i)) << ',' << m.generated
          << ',' << m.reached << ',' << m.picked_up << ',' << m.used << ',' << m.consumed << ',' << m.equipped << ','
          << m.dropped << ',' << m.discarded << ',' << m.carried << ',' << m.charges_used << ',' << m.drunk << ','
          << m.thrown << ',' << m.scrolls_read << ',' << m.turns_equipped << '\n';
    }
}
void write_monsters(std::ostream& o, const RunMetrics& r) {
    for(size_t i = 1; i < r.monsters.size(); ++i) {
        const auto& m = r.monsters[i];
        o << r.seed << ',' << r.agent << ',' << i << ',' << monster_name(static_cast<uint8_t>(i)) << ',' << m.generated
          << ',' << m.encountered << ',' << m.engaged << ',' << m.killed << ',' << m.player_attacks << ',' << m.damage_taken
          << ',' << m.attacks << ',' << m.hits << ',' << m.player_damage << ',' << m.deaths << ',' << m.poison << ','
          << m.confusion << ',' << m.paralysis << ',' << m.fire << '\n';
    }
}
}
