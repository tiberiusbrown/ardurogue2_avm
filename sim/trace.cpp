#include "trace.hpp"
#include "metrics.hpp"
#include <ostream>
namespace sim {
void trace_explorer(std::ostream& out,const ExplorerObservation& o,const SurvivalRisk& r) {
    out<<"  OBS radius="<<int(o.radius)<<" danger="<<int(r.danger)<<" incoming="<<r.one<<'/'<<r.two
        <<" retreat="<<r.retreat<<" attacker="<<int(o.reported_attacker)<<" statuses="<<int(o.confused)<<','<<int(o.paralyzed)<<','<<int(o.weakened)<<','<<int(o.slowed)<<','<<int(o.blindness)<<" worn=";
    for(auto slot:o.equipped) out<<int(slot)<<',';
    out<<" contact="<<o.contact_reported<<':'<<int(o.melee_feedback)<<'@'<<int(o.contact.x)<<':'<<int(o.contact.y)
        <<" contact_origin="<<int(o.contact_origin.x)<<':'<<int(o.contact_origin.y)
        <<" attacks="<<int(o.incoming_attacks)<<':'<<int(o.anonymous_attacks)
        <<" temporary_sight="<<o.see_invisible<<" changed="<<o.encounter_changed<<" summoning="<<o.summoning_reported<<" visible=";
    for(int m=0;m<o.monster_count;++m) out<<int(o.monsters[m].type)<<'@'<<int(o.monsters[m].pos.x)<<':'<<int(o.monsters[m].pos.y)<<';';
    out<<" pack=";
    for(size_t s=0;s<o.inventory.size();++s) {
        auto& i=o.inventory[s]; if(i.category==ItemCategory::None) continue;
        out<<s<<':'<<int(i.category)<<':'<<int(i.type)<<':'<<int(i.appearance)<<':'<<int(i.quantity)<<':'<<i.individual<<':'<<i.cursed<<':'<<int(i.enchant)<<':'<<int(i.modifier)<<';';
    }
    out<<" sight=";
    for(int n=0;n<EXPLORER_CELLS;++n) if(o.visible[n]) out<<n<<':'<<int(o.terrain[n])<<':'<<int(o.features[n])<<';';
    out<<'\n';
}
std::string action_text(const Action& a) {
    switch(a.kind) {
    case ActionKind::Move:
        return a.dy < 0 ? "MOVE_NORTH" : a.dx > 0 ? "MOVE_EAST" : a.dy > 0 ? "MOVE_SOUTH" : "MOVE_WEST";
    case ActionKind::Wait: return "WAIT";
    case ActionKind::Take: return "TAKE ground="+std::to_string(a.target)+" swap_slot="+std::to_string(a.slot);
    case ActionKind::Swap: return "SWAP ground="+std::to_string(a.target)+" slot="+std::to_string(a.slot);
    case ActionKind::Stairs: return "STAIRS";
    case ActionKind::Use: return "USE slot="+std::to_string(a.slot)+" target="+std::to_string(a.target);
    case ActionKind::Throw: return std::string(a.slot<rogue::INVENTORY && rogue::is_ammo(rogue::game.inventory[a.slot].type) ?
        (rogue::game.weapon_slot<rogue::INVENTORY && rogue::is_bow(rogue::game.inventory[rogue::game.weapon_slot].type) ? "shoot arrow slot=" : "throw arrow slot=") : "throw potion slot=")+std::to_string(a.slot)+" direction="+std::to_string(a.dx)+","+std::to_string(a.dy);
    case ActionKind::Wand: return "WAND slot="+std::to_string(a.slot)+" direction="+std::to_string(a.dx)+","+std::to_string(a.dy);
    case ActionKind::Drop: return "DROP slot="+std::to_string(a.slot)+" discard="+std::to_string(a.discard);
    }
    return "INVALID";
}
void trace_action(std::ostream& out, uint64_t action, uint64_t turn, const Action& a) {
    const auto& g = rogue::game;
    out << "A" << action << " T" << turn << " F" << int(g.floor)
        << " HP " << int(g.hp) << '/' << int(rogue::player_max_hp())
        << " LV " << int(g.level) << " HUNGER " << int(g.hunger)
        << " POS " << int(g.player.x) << ',' << int(g.player.y)
        << " goal=" << a.goal;
    if(a.destination.x != rogue::NONE) out << " target=" << int(a.destination.x) << ',' << int(a.destination.y);
    if(a.slot < rogue::INVENTORY && g.inventory[a.slot].type)
        out << " item=" << item_name(g.inventory[a.slot].type);
    for(const auto& ground : g.ground) if(ground.item.type && ground.pos == a.destination)
        out << " loot=" << item_name(ground.item.type);
    for(const auto& monster : g.monsters) if(monster.type && monster.pos == a.destination)
        out << " monster=" << monster_name(monster.type) << " monster_hp=" << int(monster.hp);
    out << " action=" << action_text(a) << '\n';
}
void trace_event(std::ostream& out, EventKind kind, uint8_t, uint8_t type, uint16_t amount) {
    switch(kind) {
    case EventKind::MonsterTrapActivated: out << "  monster trap activated=" << int(type) << '\n'; break;
    case EventKind::MonsterTrapTeleport: out << "  monster trap teleported=" << amount << '\n'; break;
    case EventKind::FeatureActivated: out << "  feature activated=" << int(type) << '\n'; break;
    case EventKind::FeatureDiscovered: out << "  feature discovered=" << int(type) << '\n'; break;
    case EventKind::FeatureSpawn: out << "  feature spawned=" << monster_name(type) << '\n'; break;
    case EventKind::FloorEntered: out << "  enter floor=" << int(rogue::game.floor) << " ascent=" << int(rogue::game.has_amulet) << '\n'; break;
    case EventKind::FloorExited: out << "  exit floor=" << int(rogue::game.floor) << '\n'; break;
    case EventKind::PlayerDamage: out << "  player damage=" << amount << '\n'; break;
    case EventKind::MonsterDamage: out << "  damage " << monster_name(type) << '=' << amount << '\n'; break;
    case EventKind::MonsterKilled: out << "  killed " << monster_name(type) << '\n'; break;
    case EventKind::Pickup: out << "  picked up " << item_name(type) << " units=" << amount << '\n'; break;
    case EventKind::Equipped: out << "  equipped " << item_name(type) << '\n'; break;
    case EventKind::ItemUsed: out << "  used " << item_name(type) << '\n'; break;
    case EventKind::PotionThrown: out << "  threw " << item_name(type) << '\n'; break;
    case EventKind::ChargeUsed: out << "  wand " << item_name(type) << '\n'; break;
    case EventKind::Finished: out << "  ended result=" << int(type) << '\n'; break;
    default: break;
    }
}
}
