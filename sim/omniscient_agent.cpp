#include "agent.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <algorithm>
#include <cstdlib>

namespace sim {
using namespace rogue;
namespace {
constexpr int8_t dxs[] = {0, 1, 0, -1}, dys[] = {-1, 0, 1, 0};
int cell(Position p) { return p.y * MAP_W + p.x; }
int distance(Position a, Position b) { return std::abs(int(a.x)-b.x)+std::abs(int(a.y)-b.y); }
bool equipped(int s) { return s == game.weapon_slot || s == game.armor_slot ||
    s == game.amulet_slot || s == game.ring_slots[0] || s == game.ring_slots[1]; }
Action use(int s, const char* goal, int target = NONE) {
    Action a; a.kind = ActionKind::Use; a.slot = static_cast<uint8_t>(s);
    a.target = static_cast<uint8_t>(target); a.goal = goal; return a;
}
int find(uint8_t type) {
    for(int s = 0; s < INVENTORY; ++s) if(game.inventory[s].type == type) return s;
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
        return 5 * (w.minimum_damage+w.maximum_damage) + 3*w.accuracy + 3*equipment_enchant(i);
    }
    if(is_armor(i.type)) return 15*armor_definition(i.type).rating + 3*equipment_enchant(i);
    int n = item_value(i);
    switch(i.type) {
    case RING_PROTECTION: return 65+15*n;
    case RING_INVISIBILITY: return 100;
    case RING_STRENGTH: return 45+8*n;
    case RING_DEXTERITY: return 50+8*n;
    case RING_ATTACK: return 40+8*n;
    case RING_FIRE_IMMUNITY: return game.floor >= 11 ? 85 : 55;
    case RING_SUSTENANCE: return 35;
    case RING_SEE_INVISIBLE: return 15;
    case AMULET_SPEED: return 100+25*std::min(n,3);
    case AMULET_IRONBLOOD: return 90;
    case AMULET_CLARITY: return 65;
    case AMULET_VAMPIRE: return 80;
    case AMULET_VITALITY: return 55+10*n;
    case AMULET_WISDOM: return 60;
    case AMULET_REGENERATION: return 40;
    case AMULET_CONSERVATION: return 35;
    default: return 0;
    }
}
int slot_score(int s) { return s < INVENTORY ? equipment_score(game.inventory[s]) : 0; }
bool upgrade(Item i) {
    if(!equipment_score(i)) return false;
    int s = NONE;
    if(is_weapon(i.type)) s = game.weapon_slot;
    else if(is_armor(i.type)) s = game.armor_slot;
    else if(is_amulet(i.type)) s = game.amulet_slot;
    else if(is_ring(i.type)) {
        if(game.ring_slots[0] == NONE || game.ring_slots[1] == NONE) return true;
        s = slot_score(game.ring_slots[0]) < slot_score(game.ring_slots[1]) ?
            game.ring_slots[0] : game.ring_slots[1];
    }
    return (s == NONE || !item_is_cursed(game.inventory[s])) && equipment_score(i) > slot_score(s);
}
int value(Item i) {
    if(i.type == YENDOR_AMULET) return 10000;
    if(is_equipment(i.type) || is_ring(i.type) || is_amulet(i.type))
        return upgrade(i) ? 100+equipment_score(i) : 0;
    if(is_wand(i.type)) {
        if(wand_afflicted(i) || !wand_charges(i)) return 0;
        switch(i.type) {
        case WAND_STRIKING: return 70 + 8*wand_charges(i);
        case WAND_ICE: return 65 + 6*wand_charges(i);
        case WAND_FIRE: return 55 + 5*wand_charges(i);
        case WAND_FORCE: return 30 + 3*wand_charges(i);
        case WAND_TELEPORT: return 35 + 3*wand_charges(i);
        case WAND_POLYMORPH: return 20 + 3*wand_charges(i);
        default: return 0;
        }
    }
    switch(i.type) {
    case FOOD: return quantity(FOOD) >= 8 ? 0 : (game.hunger < 60 ? 180 : 70);
    case HEALING: return 180;
    case EXPERIENCE: return 220;
    case STRENGTH: return game.strength < 12 || game.weakened ? 160 : 0;
    case DEXTERITY: return game.dexterity < 12 ? 150 : 0;
    case PARALYSIS: return 100;
    case CONFUSION: return 65;
    case SLOWING: return 60;
    case POISON: return 60;
    case HARMING: return 55;
    case INVISIBILITY: return 70;
    case SCROLL_ENCHANT: return 190;
    case SCROLL_TORMENT: return 95;
    case SCROLL_MASS_CONFUSE: return 100;
    case SCROLL_MASS_POISON: return 75;
    case SCROLL_FEAR: return 60;
    case SCROLL_TELEPORT: return 45;
    case SCROLL_REMOVE_CURSE: return 20;
    default: return 0;
    }
}
int discard_value(int s) {
    if(!inventory_item_removable(static_cast<uint8_t>(s)) || equipped(s)) return 100000;
    Item i = game.inventory[s];
    // Retaining stocked food has value even when the desired supply cap was
    // reached. Using acquisition value here caused food/scroll swap cycles.
    if(i.type == FOOD) return 70 + 20*item_value(i);
    return value(i) + ((i.type == FOOD || is_potion(i.type) || is_scroll(i.type)) ? item_value(i)*5 : 0);
}
bool fits(Item incoming) {
    int capacity = 0;
    bool stack = incoming.type == FOOD || is_potion(incoming.type) || is_scroll(incoming.type);
    for(Item i : game.inventory) {
        if(!i.type) return true;
        if(stack && i.type == incoming.type) capacity += ITEM_VALUE_MASK-item_value(i);
    }
    return stack && capacity >= item_value(incoming);
}
int replacement(Item i) {
    int best = -1, worst = value(i);
    for(int s = 0; s < INVENTORY; ++s)
        if(game.inventory[s].type && discard_value(s) < worst) { best = s; worst = discard_value(s); }
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
}

Paths::Paths(bool allow_monsters) {
    distance.fill(-1); first.fill(-1);
    std::array<bool, MAP_W*MAP_H> occupied{};
    for(const Monster& m : game.monsters) if(m.type) occupied[cell(m.pos)] = true;
    std::array<int, MAP_W*MAP_H> queue{};
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

Action OmniscientAgent::choose_action(const DecisionContext&) {
    if(game.paralyzed) { Action a; a.goal = "wait for paralysis"; return a; }
    int adjacent = -1, threats = 0;
    for(int i = 0; i < MONSTERS; ++i) if(game.monsters[i].type) {
        const Monster& m = game.monsters[i];
        int d = distance(m.pos,game.player);
        if(d <= 5 && can_see(m.pos)) ++threats;
        if(d == 1 && (adjacent < 0 || m.hp < game.monsters[adjacent].hp)) adjacent = i;
    }
    int healing = find(HEALING);
    if(healing >= 0 && (game.hp*100 <= player_max_hp()*(adjacent >= 0 ? 65 : 45) ||
                      (game.weakened && game.hp < player_max_hp())))
        return use(healing,"heal and restore strength");
    int food = find(FOOD);
    if(food >= 0 && game.hunger < (adjacent >= 0 ? 15 : 100)) return use(food,"eat before starvation");
    // Equipping even in danger is worthwhile for an obvious weapon/armor gain.
    int upgrade_slot = -1, upgrade_score = 0;
    for(int s = 0; s < INVENTORY; ++s) {
        Item i = game.inventory[s];
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
    int xp = find(EXPERIENCE);
    if(xp >= 0) return use(xp,"gain levels and recover HP");
    if(game.weakened && find(STRENGTH) >= 0) return use(find(STRENGTH),"restore strength");

    // Real ray scanning determines the first hittable monster, including mimics.
    for(int d = 0; d < 4; ++d) {
        auto ray = scan_ray(game.player,dxs[d],dys[d],6);
        if(ray.monster == NONE) continue;
        const Monster& m = game.monsters[ray.monster];
        bool dangerous = m.type >= ORC || game.hp < 12 || threats > 1;
        if(!dangerous || (ray.steps > 4 && m.type != LORD)) continue;
        for(uint8_t type : {uint8_t(PARALYSIS),uint8_t(POISON),uint8_t(CONFUSION),uint8_t(SLOWING)}) {
            bool needed = type == PARALYSIS ? m.stun == 0 && ray.steps <= 2 :
                type == POISON ? !monster_effect(m,MON_WEAKENED) :
                type == CONFUSION ? !monster_effect(m,MON_CONFUSED) && !m.stun :
                !monster_effect(m,MON_SLOWED);
            int s = find(type);
            if(s >= 0 && needed && (m.type >= INCUBUS || (adjacent >= 0 && game.hp < player_max_hp()*3/4))) {
                Action a; a.kind = ActionKind::Throw; a.slot = static_cast<uint8_t>(s);
                a.dx = dxs[d]; a.dy = dys[d]; a.goal = "control dangerous monster"; a.destination = m.pos; return a;
            }
        }
        for(uint8_t type : {uint8_t(WAND_STRIKING),uint8_t(WAND_ICE),uint8_t(WAND_FIRE),uint8_t(HARMING)}) {
            int s = find(type); if(s < 0) continue;
            Item i = game.inventory[s];
            if(is_wand(type) && (wand_afflicted(i) || !wand_charges(i))) continue;
            if(type == WAND_FIRE && (m.type == DRAGON ||
                (ray.steps <= (wand_powerful(i) ? 2 : 1) && ring_bonus(RING_FIRE_IMMUNITY) <= 0))) continue;
            Action a; a.kind = is_wand(type) ? ActionKind::Wand : ActionKind::Throw;
            a.slot = static_cast<uint8_t>(s); a.dx = dxs[d]; a.dy = dys[d];
            if(is_wand(type) && !wand_needs_direction(i)) a.dx = a.dy = 0;
            a.goal = "ranged damage"; a.destination = m.pos; return a;
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
    for(uint8_t type : {uint8_t(STRENGTH),uint8_t(DEXTERITY)}) {
        int s = find(type);
        if(s >= 0 && (type == STRENGTH ? game.strength : game.dexterity) < 12) return use(s,"permanent stat improvement");
    }
    int enchant = find(SCROLL_ENCHANT);
    if(enchant >= 0) {
        int target = -1;
        if(game.amulet_slot < INVENTORY && game.inventory[game.amulet_slot].type == AMULET_SPEED &&
           item_value(game.inventory[game.amulet_slot]) < 3) target = game.amulet_slot;
        for(uint8_t s : game.ring_slots) if(target < 0 && s < INVENTORY &&
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
    // On ascent preserve resources and go directly to the surface.
    if(game.has_amulet) {
        if(game.player == game.up) { Action a; a.kind = ActionKind::Stairs; a.goal = "ascend and escape"; return a; }
        if(paths.to(game.up) > 0) return paths.move_to(game.up,"ascend");
        return Paths(true).move_to(game.up,"ascend through blocking monster");
    }
    int best = -1, best_score = 0;
    for(int i = 0; i < GROUND_ITEMS; ++i) {
        const GroundItem& g = game.ground[i]; int worth = value(g.item), d = paths.to(g.pos);
        if(worth <= 0 || d < 0 || (g.item.type != YENDOR_AMULET && !fits(g.item) && replacement(g.item) < 0)) continue;
        int score = worth*100/(d+8);
        if(score > best_score) { best = i; best_score = score; }
    }
    if(best >= 0) {
        if(game.player == game.ground[best].pos) return pickup(best);
        return paths.move_to(game.ground[best].pos,"seek useful loot");
    }
    if(game.floor == FLOORS-1) {
        for(const Monster& m : game.monsters) if(m.type == LORD)
            return Paths(true).move_to(m.pos,"defeat Lord");
        for(int i = 0; i < GROUND_ITEMS; ++i) if(game.ground[i].item.type == YENDOR_AMULET)
            return Paths(true).move_to(game.ground[i].pos,"recover Yendor");
    }
    // Early, weak monsters provide levels and HP recovery. Avoid chasing bats.
    if(game.floor < 4 && !game.weakened && game.hp > player_max_hp()/2) {
        int target = -1, nearest = 10000;
        for(int i = 0; i < MONSTERS; ++i) {
            const Monster& m = game.monsters[i]; int d = paths.to(m.pos);
            if(m.type > BAT && m.type <= GOBLIN && d > 0 && d <= 16 && d < nearest) { nearest = d; target = i; }
        }
        if(target >= 0) return paths.move_to(game.monsters[target].pos,"gain early combat experience");
    }
    if(game.player == game.down && game.floor < FLOORS-1) {
        Action a; a.kind = ActionKind::Stairs; a.goal = "descend"; return a;
    }
    if(paths.to(game.down) > 0) return paths.move_to(game.down,"descend");
    return Paths(true).move_to(game.down,"descend through blocking monster");
}
}
