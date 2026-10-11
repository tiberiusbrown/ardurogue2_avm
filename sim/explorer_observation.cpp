#include "explorer_observation.hpp"
#include "explorer_events.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <algorithm>

namespace sim {
using namespace rogue;
namespace {
int cell(Position p) { return p.y*MAP_W+p.x; }
SeenPosition pos(Position p) { return {p.x,p.y}; }
ItemCategory category(uint8_t t) {
    if(is_ammo(t)) return ItemCategory::Ammo;
    if(t==FOOD) return ItemCategory::Food;
    if(is_potion(t)) return ItemCategory::Potion;
    if(is_weapon(t)) return ItemCategory::Weapon;
    if(is_armor(t)) return ItemCategory::Armor;
    if(is_ring(t)) return ItemCategory::Ring;
    if(is_amulet(t)||t==YENDOR_AMULET) return ItemCategory::Amulet;
    if(is_scroll(t)) return ItemCategory::Scroll;
    if(is_wand(t)) return ItemCategory::Wand;
    if(is_light(t)) return ItemCategory::Light;
    if(is_shield(t)) return ItemCategory::Shield;
    return ItemCategory::None;
}
int identity_group(ItemCategory c) {
    switch(c) { case ItemCategory::Potion:return 0; case ItemCategory::Scroll:return 1;
    case ItemCategory::Ring:return 2; case ItemCategory::Amulet:return 3; case ItemCategory::Wand:return 4;
    default:return 5; }
}
SeenItem describe(Item i, bool detailed) {
    SeenItem s; s.category=category(i.type);
    if(s.category==ItemCategory::None || !detailed) return s; // Empty slots carry no details; sprites communicate category only.
    bool shuffled=is_potion(i.type)||is_scroll(i.type)||is_ring(i.type)||is_amulet(i.type)||is_wand(i.type);
    s.type=shuffled && !item_type_identified(i.type) ? uint8_t(NO_ITEM) : i.type;
    s.appearance=shuffled ? item_appearance(i.type) : NONE;
    s.individual=item_is_identified(i);
    s.quantity=is_stackable(i.type)||is_light(i.type) ? item_value(i) : 1;
    s.cursed=s.individual && item_is_cursed(i);
    s.enchant=s.individual ? equipment_enchant(i) : 0;
    // Jewelry magnitude is not printed even after identification.
    s.magnitude=0;
    if(is_wand(i.type) && s.type && s.individual) {
        s.quantity=wand_charges(i); s.modifier=wand_modifier(i);
    } else if(is_wand(i.type)) s.quantity=0;
    return s;
}
Terrain terrain(Position p) {
    if(wall_at(p.x,p.y)) return Terrain::Wall;
    uint8_t d=door_at(p);
    if(d!=NONE) return door_open(d) ? Terrain::OpenDoor : Terrain::ClosedDoor;
    if(p==game.up) return Terrain::Up;
    if(game.floor<FLOORS-1 && p==game.down) return Terrain::Down;
    return Terrain::Floor;
}
}
ExplorerObservation observe_explorer(uint64_t actions, uint64_t turns) {
    static_assert(MAP_W==EXPLORER_W && MAP_H==EXPLORER_H && FLOORS==16);
    ExplorerObservation o;
    o.actions=actions; o.turns=turns; o.player=pos(game.player); o.floor=game.floor;
    o.hp=game.hp; o.max_hp=player_max_hp(); o.level=game.level; o.xp=game.xp;
    o.strength=game.strength; o.dexterity=game.dexterity; o.speed=game.speed; o.hunger=game.hunger;
    // The interface announces effects, but does not disclose their remaining timers.
    o.weakened=game.weakened!=0; o.confused=game.confused!=0; o.paralyzed=game.paralyzed!=0;
    o.slowed=game.slowed!=0; o.invisible=game.invisible!=0; o.blindness=game.blindness!=0;
    o.radius=player_light_radius(); o.yendor=game.has_amulet; o.pending_pickups=session.pending_pickups;
    o.equipped={game.weapon_slot,game.armor_slot,game.offhand_slot,game.amulet_slot,game.ring_slots[0],game.ring_slots[1]};
    for(int s=0;s<INVENTORY;++s) o.inventory[s]=describe(game.inventory[s],true);
    // This translates only already identified public knowledge, including consumed objects.
    for(uint8_t t=POTION_FIRST;t<=WAND_LAST;++t) if(item_type_identified(t)) {
        auto c=category(t); int g=identity_group(c); auto a=item_appearance(t);
        if(g<5 && a<16) o.identities[g][a]=t;
    }
    uint16_t opaque[13]{}, sight[13]{};
    int left=int(game.player.x)-6, top=int(game.player.y)-6;
    for(int sy=0;sy<13;++sy) for(int sx=0;sx<13;++sx)
        if(blocked(int16_t(left+sx),int16_t(top+sy))) opaque[sy]|=uint16_t(1u<<sx);
    ray_sight(opaque,sight);
    for(int sy=0;sy<13;++sy) {
        sight[sy]&=light_mask(o.radius,uint8_t(sy));
        for(int sx=0;sx<13;++sx) if(left+sx<0||left+sx>=MAP_W||top+sy<0||top+sy>=MAP_H)
            sight[sy]&=uint16_t(~(1u<<sx));
    }
    // Same cardinal wall-surface halo as render.cpp. Opaque cells cannot extend it.
    for(int sy=0;sy<13;++sy) {
        uint16_t floor=sight[sy]&uint16_t(~opaque[sy]);
        uint16_t adjacent=uint16_t((floor<<1)|(floor>>1));
        if(sy) adjacent|=sight[sy-1]&uint16_t(~opaque[sy-1]);
        if(sy<12) adjacent|=sight[sy+1]&uint16_t(~opaque[sy+1]);
        sight[sy]|=adjacent&opaque[sy]&light_mask(o.radius,uint8_t(sy));
        for(int sx=0;sx<13;++sx) if(left+sx>=0 && left+sx<MAP_W && top+sy>=0 && top+sy<MAP_H && (sight[sy]&(1u<<sx)))
            o.visible.set((top+sy)*MAP_W+left+sx);
    }
    o.visible.set(cell(game.player));
    // Native explored bits are written only by mapping/digging. Renderer is not run.
    for(int n=0;n<EXPLORER_CELLS;++n) if(o.visible[n] || (game.explored[n>>3]&(1u<<(n&7))))
        o.terrain[n]=terrain({uint8_t(n%MAP_W),uint8_t(n/MAP_W)});
    for(const auto& f:game.features) if(f.id && !feature_is_hidden(f.id) &&
        (o.visible[cell(f.pos)] || (game.explored[cell(f.pos)>>3]&(1u<<(cell(f.pos)&7)))))
        o.features[cell(f.pos)]=feature_type(f.id);
    // The renderer overwrites ground sprites with visible monster/disguise sprites.
    // The occupied square still offers the normal pickup inspection interface.
    std::bitset<EXPLORER_CELLS> covered;
    for(uint8_t i=0;i<MONSTERS;++i) {
        const auto& m=game.monsters[i];
        if(m.type && player_can_see_monster(i) && o.visible[cell(m.pos)] && m.pos!=game.player)
            covered.set(cell(m.pos));
    }
    for(uint8_t i=0;i<GROUND_ITEMS;++i) {
        const auto& g=game.ground[i];
        if(g.item.type && o.visible[cell(g.pos)] && !covered[cell(g.pos)]) {
            auto& s=o.ground[o.ground_count++]; s.pos=pos(g.pos);
            s.item=describe(g.item,g.pos==game.player);
            s.handle=g.pos==game.player ? i : NONE;
        }
    }
    for(uint8_t i=0;i<MONSTERS;++i) {
        const auto& m=game.monsters[i];
        if(!m.type || !player_can_see_monster(i) || !o.visible[cell(m.pos)]) continue;
        if(m.type==MIMIC && !(m.state&MON_AGGRO)) {
            auto& s=o.ground[o.ground_count++]; s.pos=pos(m.pos);
            constexpr ItemCategory disguises[]={ItemCategory::Scroll,ItemCategory::Potion,ItemCategory::Amulet,ItemCategory::Ring,ItemCategory::Wand};
            s.item.category=disguises[mimic_appearance(m)];
        } else {
            auto info=monster_info(m.type); auto& s=o.monsters[o.monster_count++];
            s={pos(m.pos),m.type,info.strength,info.speed,info.armor,info.flags};
        }
    }
    explorer_message_observation(o);
    if(o.reported_attacker) {
        auto info=monster_info(o.reported_attacker);
        o.reported_species={{},o.reported_attacker,info.strength,info.speed,info.armor,info.flags};
    }
    return o;
}
ExplorerObservation ExplorerObserver::observe(uint64_t actions,uint64_t turns) {
    auto o=observe_explorer(actions,turns);
    if(floor!=o.floor) { floor=o.floor; ascent=o.yendor; explicit_reveals.reset(); previous_hp=o.hp; }
    // Mapping/digging yield snapshots, not a remote live-map subscription.
    // Yendor acquisition on floor 15 does not itself regenerate that floor.
    for(int n=0;n<EXPLORER_CELLS;++n) {
        bool revealed=(game.explored[n>>3]&(1u<<(n&7)))!=0;
        if(!o.visible[n] && explicit_reveals[n]) { o.terrain[n]=Terrain::Unknown; o.features[n]=0; }
        explicit_reveals[n]=revealed;
    }
    o.hp_loss=o.hp<previous_hp ? previous_hp-o.hp : 0;
    o.attacked=o.attacked || o.hp_loss!=0; previous_hp=o.hp;
    return o;
}
}
