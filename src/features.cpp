#include "features.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "status.hpp"
#include "sim_hooks.hpp"

namespace rogue {
uint8_t trap_tuning_depth() {
#if defined(ARDUROGUE2_SIM)
    if(sim::original_features(sim::FeatureVariant::Early) || sim::original_features(sim::FeatureVariant::Damage)) return 4;
#endif
    return game.floor;
}
uint8_t feature_at(Position pos) {
    for(uint8_t i = 0; i < MAP_FEATURES; ++i)
        if(game.features[i].id && game.features[i].pos == pos) return i;
    return NONE;
}
uint8_t observed_feature_at(const Game& source, Position pos) {
    for(const MapFeature& feature : source.features) {
        MapFeature seen = observed_feature(feature);
        if(seen.id && seen.pos == pos) return seen.id;
    }
    return FEATURE_NONE;
}
void reveal_feature(uint8_t index) {
    if(index >= MAP_FEATURES || !feature_is_trap(game.features[index].id) ||
       !feature_is_hidden(game.features[index].id)) return;
    game.features[index].id = feature_type(game.features[index].id);
    SIM_EVENT(sim::EventKind::FeatureDiscovered, index, game.features[index].id);
}
void remove_feature(uint8_t index) {
    if(index < MAP_FEATURES) game.features[index] = {};
}
void resolve_forced_entry() {
    if(!session.forced_entry) return;
    session.forced_entry = false;
    activate_entry_feature(feature_at(game.player), true);
}
void search_traps() {
    bool found = false;
    for(uint8_t i = 0; i < MAP_FEATURES; ++i) {
        const MapFeature& f = game.features[i];
        if(!feature_is_trap(f.id) || !feature_is_hidden(f.id) || f.pos == game.player ||
           !square_contains(game.player, f.pos, 1)) continue;
        if(!roll(4)) { reveal_feature(i); found = true; }
    }
    if(found) status(F("You discover a hidden trap!"));
}
void wait_search() {
    if(!game.paralyzed) search_traps();
    end_turn();
}
static void physical_damage(uint8_t raw) {
    hurt_player(physical_damage_after_armor(raw,
        armor_absorption(player_armor_rating(), player_armor_enchant())));
}
// The four damaging player traps share their depth rolls with monsters.
static uint8_t trap_damage_roll(uint8_t type) {
    uint8_t depth = trap_tuning_depth();
    switch(type) {
    case FEATURE_SPIKE_TRAP: return depth < 2 ? OPENING_SPIKE_MIN + roll(OPENING_SPIKE_ROLL) : 3 + roll(5);
    case FEATURE_POISON_DART_TRAP: return 1 + roll(depth < 2 ? OPENING_DART_ROLL : 3);
    case FEATURE_FLAME_TRAP: return depth < 4 ? MIDDLE_FLAME_MIN + roll(MIDDLE_FLAME_ROLL) : 5 + roll(5);
    case FEATURE_FROST_TRAP: return depth < 4 ? MIDDLE_FROST_MIN + roll(MIDDLE_FROST_ROLL) : 3 + roll(4);
    case FEATURE_MAGIC_DRAIN_TRAP: return 3 + roll(4);
    case FEATURE_RUST_TRAP: return 2 + roll(3);
    default: return 0;
    }
}
static void weaken() { if(game.weakened < 3) ++game.weakened; }
static void confuse(bool trap = false) {
    if(amulet_bonus(AMULET_CLARITY) <= 0) {
        uint8_t duration = static_cast<uint8_t>(trap && trap_tuning_depth() < 2 ? OPENING_CONFUSION_MIN + roll(OPENING_CONFUSION_ROLL) : 5 + roll(4));
        if(game.confused < duration) game.confused = duration;
    }
}
static void cure() { game.weakened = game.confused = game.slowed = 0; }
static void blessing(uint8_t divisor = 4) {
    uint8_t amount = player_max_hp() / divisor;
    heal_player(amount ? amount : 1);
    status(F("You feel renewed."));
}
static void set_curse(Item& item, bool cursed) {
    if(uses_full_info(item.type)) return;
    if(is_wand(item.type)) {
        if(cursed) set_wand_modifier(item, WAND_CURSED);
        else if(wand_afflicted(item)) set_wand_modifier(item, WAND_NORMAL);
    } else if(!is_ammo(item.type)) {
        if(cursed) item.info |= ITEM_CURSED;
        else item.info &= static_cast<uint8_t>(~ITEM_CURSED);
    }
}
// Reservoir selection needs no inventory scratch array.
static uint8_t random_item(uint8_t mode) {
    uint8_t selected = NONE, count = 0;
    for(uint8_t s = 0; s < INVENTORY; ++s) {
        const Item& i = game.inventory[s];
        bool eligible = i.type && (mode == 0 ? !item_is_identified(i) :
            mode == 1 ? item_is_cursed(i) : is_wand(i.type) && wand_charges(i));
        if(eligible && !roll(++count)) selected = s;
    }
    return selected;
}
static void random_identify() {
    uint8_t slot = random_item(0);
    if(slot == NONE) { blessing(); return; }
    identify_item(slot);
    status(F("You identify")); status(game.inventory[slot], '.');
}
static void random_uncurse() {
    uint8_t slot = random_item(1);
    if(slot == NONE) { blessing(); return; }
    set_curse(game.inventory[slot], false);
    status(F("The curse lifts from")); status(game.inventory[slot], '.');
}
static uint8_t random_equipped() {
    bool weapon = game.weapon_slot < INVENTORY && is_weapon(game.inventory[game.weapon_slot].type);
    bool armor = game.armor_slot < INVENTORY && is_armor(game.inventory[game.armor_slot].type);
    bool shield = game.offhand_slot < INVENTORY && is_shield(game.inventory[game.offhand_slot].type);
    if(shield) {
        uint8_t choice = roll(static_cast<uint8_t>(1 + weapon + armor));
        if(weapon && choice-- == 0) return game.weapon_slot;
        if(armor && choice-- == 0) return game.armor_slot;
        return game.offhand_slot;
    }
    // Preserve the existing RNG stream when no shield participates.
    if(weapon && armor) return roll(2) ? game.weapon_slot : game.armor_slot;
    return weapon ? game.weapon_slot : armor ? game.armor_slot : NONE;
}
static void improve_equipped() {
    uint8_t slot = random_equipped();
    if(slot == NONE) blessing();
    else set_equipment_enchant(game.inventory[slot], equipment_enchant(game.inventory[slot]) + 1);
}
// Only bounded local work during activation, never a map-sized gameplay scan.
static bool summon(Position origin, bool trap, bool undead = false, bool visible = true) {
    uint8_t slot = NONE;
    for(uint8_t i = 0; i < MONSTERS; ++i) if(!game.monsters[i].type) { slot = i; break; }
    if(slot == NONE) return false;
    uint8_t start = roll(25);
    for(uint8_t n = 0; n < 25; ++n) {
        uint8_t cell = static_cast<uint8_t>((start + n) % 25);
        int16_t x = origin.x + static_cast<int16_t>(cell % 5) - 2;
        int16_t y = origin.y + static_cast<int16_t>(cell / 5) - 2;
        Position pos{static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
        if(blocked(x, y) || pos == game.player || monster_at(pos) != NONE ||
           (visible && !can_see(pos))) continue;
        uint8_t type = undead ? (game.floor >= 4 ? PHANTOM : ZOMBIE) :
            generation::encounter_type(game.floor, game.random_state);
        Monster monster{pos, type, monster_health(type), 0, {0, 0},
            static_cast<uint8_t>(MON_AGGRO | (trap ? MON_TRAP_SUMMONED : 0))};
        if(type == MIMIC) set_mimic_appearance(monster, static_cast<MimicAppearance>(roll(MIMIC_APPEARANCE_COUNT)));
        game.monsters[slot] = monster;
        SIM_EVENT(sim::EventKind::FeatureSpawn, slot, type);
        return true;
    }
    return false;
}
static void gift_item() {
    uint8_t slot = NONE;
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) if(!game.ground[i].item.type) { slot = i; break; }
    if(slot == NONE || item_at(game.player) != NONE) { blessing(); return; }
    Item item = generation::useful_item(game.random_state);
    game.ground[slot] = {game.player, item};
    SIM_EVENT(sim::EventKind::GeneratedItem, slot, item.type, is_stackable(item.type) ? item_value(item) : 1);
    status(F("You discover")); status(item, '.');
}
// Formatting and paging run after effect/placement temporaries have unwound.
static void visible_trap_status(const Monster& monster, const char PROGMEM* message) {
#if !defined(__AVM__)
    // Native status adapters retain their existing public-feedback protocol.
    monster_status(monster, message);
#else
    status_capitalize();
    status_visible_monster(monster);
    status(message);
#endif
}
__attribute__((noinline)) static void monster_trap_message(uint8_t index, uint8_t type, bool lethal, bool visible) {
    const Monster& monster = game.monsters[index];
    if(!visible) return;
    if(!player_can_see_monster(index)) {
        status(F("A trap activates!"));
        return;
    }
    if(lethal) { visible_trap_status(monster, F("dies in a trap!")); return; }
    switch(type) {
    case FEATURE_SPIKE_TRAP: visible_trap_status(monster, F("is struck by spikes!")); break;
    case FEATURE_POISON_DART_TRAP: visible_trap_status(monster, F("is poisoned!")); break;
    case FEATURE_FLAME_TRAP: visible_trap_status(monster, monster.type == DRAGON ? F("ignores the flames!") : F("is burned!")); break;
    case FEATURE_FROST_TRAP: visible_trap_status(monster, F("is chilled!")); break;
    case FEATURE_SNARE_TRAP: visible_trap_status(monster, F("is caught in a snare!")); break;
    case FEATURE_TELEPORT_TRAP: visible_trap_status(monster, F("triggers a teleport trap!")); break;
    case FEATURE_CONFUSION_TRAP: visible_trap_status(monster, F("is confused!")); break;
    case FEATURE_MAGIC_DRAIN_TRAP: visible_trap_status(monster, F("is struck by magic!")); break;
    case FEATURE_SUMMONING_TRAP: status(F("A trap calls monsters!")); break;
    case FEATURE_RUST_TRAP: visible_trap_status(monster, F("is splashed by acid!")); break;
    default: break;
    }
}
static void trap_effect(Monster& monster, MonsterEffect effect, uint8_t duration, uint8_t type) {
    (void)type;
    if(monster_effect(monster, effect) < duration || (effect == MON_CONFUSED && (monster.state & MON_BLINDED))) {
        set_monster_effect(monster, effect, duration);
        SIM_EVENT(sim::EventKind::MonsterTrapStatus, NONE, type, 1, effect);
    }
}
__attribute__((noinline)) void activate_monster_trap(uint8_t index) {
#if defined(ARDUROGUE2_SIM)
    if(!sim::monster_traps_enabled()) return;
#endif
    if(index >= MONSTERS || !game.monsters[index].type) return;
    Monster& monster = game.monsters[index];
    uint8_t feature = feature_at(monster.pos);
    if(feature == NONE || !feature_is_trap(game.features[feature].id)) return;
    uint8_t type = feature_type(game.features[feature].id);
    bool visible = can_see(monster.pos);
    if(visible) reveal_feature(feature);
    SIM_EVENT(sim::EventKind::MonsterTrapActivated, index, type);
#if defined(ARDUROGUE2_SIM)
    sim::DamageScope source(sim::Cause::MonsterTrap, type);
#endif
    if(type == FEATURE_TELEPORT_TRAP) {
        // Report at the origin while the monster is still observable.
        monster_trap_message(index, type, false, visible);
        bool moved = teleport_monster(index, false);
        SIM_EVENT(sim::EventKind::MonsterTrapTeleport, index, type, moved);
        (void)moved;
        return;
    }
    if(type == FEATURE_SUMMONING_TRAP) {
        monster_trap_message(index, type, false, visible);
        uint8_t count = 1 + roll(2);
        while(count--) summon(monster.pos, true, false, false);
        return;
    }
    uint8_t damage = trap_damage_roll(type);
    if(type == FEATURE_SPIKE_TRAP || type == FEATURE_POISON_DART_TRAP || type == FEATURE_RUST_TRAP)
        damage = physical_damage_after_armor(damage, armor_absorption(monster_armor(monster.type), 0));
    if(type == FEATURE_FLAME_TRAP && monster.type == DRAGON) damage = 0;
    bool lethal = damage >= monster.hp;
    // Status formats the live monster; death clears its type after the message.
    monster_trap_message(index, type, lethal, visible);
    if(damage || !monster.hp) damage_monster(index, damage, false);
    if(!monster.type) return;
    switch(type) {
    case FEATURE_POISON_DART_TRAP: trap_effect(monster, MON_WEAKENED, 15, type); break;
    case FEATURE_FROST_TRAP: trap_effect(monster, MON_SLOWED, 6 + roll(5), type); break;
    case FEATURE_CONFUSION_TRAP: trap_effect(monster, MON_CONFUSED, 5 + roll(4), type); break;
    case FEATURE_SNARE_TRAP: {
        uint8_t duration = 2 + roll(2);
        if(monster.stun < duration) {
            monster.stun = duration;
            SIM_EVENT(sim::EventKind::MonsterTrapStatus, index, type, 1, 4);
        }
        break;
    }
    default: break;
    }
}
__attribute__((noinline)) void activate_entry_feature(uint8_t index, bool forced) {
    if(index >= MAP_FEATURES || session.ended) return;
    uint8_t type = feature_type(game.features[index].id);
    if(feature_is_trap(type)) {
        reveal_feature(index);
        SIM_EVENT(sim::EventKind::FeatureActivated, index, type);
#if defined(ARDUROGUE2_SIM)
        sim::DamageScope source(sim::Cause::Trap, type);
#endif
        switch(type) {
        case FEATURE_SPIKE_TRAP:
            status(F("You step on a spike trap!")); physical_damage(trap_damage_roll(type)); break;
        case FEATURE_POISON_DART_TRAP:
            status(F("A poisoned dart strikes you!")); physical_damage(trap_damage_roll(type));
            if(!session.ended) weaken(); break;
        case FEATURE_FLAME_TRAP:
            status(F("Flames erupt beneath you!")); player_take_fire_damage(trap_damage_roll(type), 6); break;
        case FEATURE_FROST_TRAP:
            status(F("Freezing air surrounds you!")); player_take_magic_damage(trap_damage_roll(type), 6);
            if(!session.ended && !magic_save(game.magic_resistance, 6)) {
                uint8_t duration = 4 + roll(4);
                if(game.slowed < duration) game.slowed = duration;
            } break;
        case FEATURE_SNARE_TRAP:
            status(F("You become caught in a snare!"));
            if(amulet_bonus(AMULET_IRONBLOOD) <= 0) {
                uint8_t duration = (trap_tuning_depth() < 4 ? MIDDLE_SNARE_MIN : 2) + roll(2);
                if(game.paralyzed < duration) game.paralyzed = duration;
            } break;
        case FEATURE_TELEPORT_TRAP:
            status(F("Space twists around you!")); teleport_player();
            SIM_EVENT(sim::EventKind::FeatureDisplacement, index, type, game.player != game.features[index].pos); break;
        case FEATURE_CONFUSION_TRAP:
            status(F("Your senses become scrambled!")); confuse(true); break;
        case FEATURE_MAGIC_DRAIN_TRAP: {
            status(F("A magical force drains your power!")); uint8_t slot = random_item(2);
            if(slot == NONE) player_take_magic_damage(3 + roll(4), 6);
            else {
                Item& wand = game.inventory[slot]; uint8_t drain = 1 + roll(2);
                uint8_t charges = wand_charges(wand);
                set_wand_charges(wand, charges > drain ? charges - drain : 0);
            } break;
        }
        case FEATURE_SUMMONING_TRAP: {
            status(F("Monsters answer a sinister call!")); uint8_t count = 1 + roll(2);
            while(count--) summon(game.player, true); break;
        }
        case FEATURE_RUST_TRAP: {
            status(F("Corrosive liquid splashes over your equipment!")); uint8_t slot = random_equipped();
            if(slot == NONE) physical_damage(1 + roll(2));
            else set_equipment_enchant(game.inventory[slot], equipment_enchant(game.inventory[slot]) - 1);
            break;
        }
        default: break;
        }
    } else if(type == FEATURE_HOLY_SYMBOL) {
        SIM_EVENT(sim::EventKind::FeatureActivated, index, type);
        status(F("Holy energy pulses around you!"));
        for(uint8_t i = 0; i < MONSTERS; ++i) {
            Monster& m = game.monsters[i];
            if(!undead_or_demonic(m.type) || !square_contains(game.player, m.pos, 2)) continue;
            uint8_t damage = 3 + roll(3);
            SIM_EVENT(sim::EventKind::HolyDamage, i, type, damage < m.hp ? damage : m.hp);
            damage_monster(i, damage, true);
            if(m.type && m.type != LORD) m.state |= MON_AFRAID;
        }
    } else if(type && !forced) {
        session.pending_feature = index;
    }
}
bool feature_accepts_item(uint8_t type, uint8_t slot) {
    if(slot >= INVENTORY) return false;
    const Item& item = game.inventory[slot];
    if(!item.type || item.type == YENDOR_AMULET) return false;
    if(type == FEATURE_SACRIFICIAL_ALTAR) return inventory_item_removable(slot);
    if(type == FEATURE_ENCHANTED_FORGE) return is_equipment(item.type);
    if(type == FEATURE_ALCHEMICAL_POOL)
        return is_equipment(item.type) || is_wand(item.type) || is_ring(item.type) || is_amulet(item.type);
    return false;
}
static void fountain() {
    uint8_t outcome = roll(100);
    if(outcome < 25) blessing();
    else if(outcome < 40) game.hunger = game.hunger > 215 ? 255 : game.hunger + 40;
    else if(outcome < 55) cure();
    else if(outcome < 70) confuse();
    else if(outcome < 80) weaken();
    else if(outcome < 85) {
        uint8_t& stat = roll(2) ? game.strength : game.dexterity;
        if(stat < 12) ++stat; else blessing();
    } else if(outcome < 90) { if(!summon(game.player, false)) blessing(); }
    else if(outcome < 95) random_identify();
    else teleport_player();
}
static void sacrifice(uint8_t slot) {
    Item item = game.inventory[slot];
    uint8_t value = is_artifact(item.type) ? 3 : is_equipment(item.type) || is_wand(item.type) ? 2 :
        is_stackable(item.type) && item_value(item) >= 3 ? 2 : 1;
    destroy_inventory_item(slot);
    switch(roll(4)) {
    case 0: heal_player(player_max_hp() * static_cast<uint16_t>(value) / 4); break;
    case 1: gain_xp(5 * value); break;
    case 2: random_uncurse(); break;
    case 3: improve_equipped(); break;
    }
}
static void pool(Item& item) {
    uint8_t outcome = roll(5);
    if(outcome < 2 && !is_equipment(item.type)) outcome = 4;
    if(outcome == 0) set_equipment_enchant(item, equipment_enchant(item) + 1);
    else if(outcome == 1) set_equipment_enchant(item, equipment_enchant(item) - 1);
    else if(outcome == 2) set_curse(item, false);
    else if(outcome == 3) set_curse(item, true);
    else if(is_wand(item.type)) {
        uint8_t charges = wand_charges(item);
        set_wand_charges(item, charges > 12 ? 15 : charges + 3);
    } else if(is_equipment(item.type)) set_equipment_enchant(item, equipment_enchant(item) + 1);
    else if(item_value(item) < ITEM_VALUE_MASK) set_item_value(item, item_value(item) + 1);
}
__attribute__((noinline)) bool resolve_feature(uint8_t index, FeatureChoice choice) {
    if(index >= MAP_FEATURES || session.ended || game.paralyzed ||
       game.features[index].pos != game.player) return false;
    uint8_t type = feature_type(game.features[index].id);
    if(type < FEATURE_FOUNTAIN || type > FEATURE_ALCHEMICAL_POOL) return false;
    if(!choice.accept) {
        SIM_EVENT(sim::EventKind::FeatureDeclined, index, type); return false;
    }
    if((type == FEATURE_SACRIFICIAL_ALTAR || type == FEATURE_ENCHANTED_FORGE ||
        type == FEATURE_ALCHEMICAL_POOL) && !feature_accepts_item(type, choice.slot)) return false;
    // Consume before rewards and damage; no outcome can duplicate this feature.
    remove_feature(index);
    SIM_EVENT(sim::EventKind::FeatureActivated, index, type);
#if defined(ARDUROGUE2_SIM)
    sim::DamageScope source(sim::Cause::Feature, type);
#endif
    switch(type) {
    case FEATURE_FOUNTAIN: fountain(); break;
    case FEATURE_HEALING_SPRING: blessing(2); cure(); break;
    case FEATURE_ALTAR: {
        uint8_t outcome = roll(8);
        if(outcome < 3) blessing();
        else if(outcome < 5) random_uncurse();
        else if(outcome == 5) random_identify();
        else if(outcome == 6) { if(roll(2)) weaken(); else confuse(); }
        else if(!summon(game.player, false)) blessing();
        break;
    }
    case FEATURE_SACRIFICIAL_ALTAR: sacrifice(choice.slot); break;
    case FEATURE_ENCHANTED_FORGE: {
        Item& item = game.inventory[choice.slot]; uint8_t outcome = roll(8);
        if(outcome < 7) set_equipment_enchant(item, equipment_enchant(item) + (outcome < 6 ? 1 : -1));
        else set_curse(item, true); break;
    }
    case FEATURE_STATUE: {
        uint8_t outcome = roll(4);
        if(outcome < 2) { if(!summon(game.player, false)) blessing(); }
        else if(outcome == 2) gift_item();
        else { blessing(); cure(); } break;
    }
    case FEATURE_SARCOPHAGUS: {
        uint8_t outcome = roll(4);
        if(outcome < 2) { if(!summon(game.player, false, true)) blessing(); }
        else if(outcome == 2) gift_item();
        else game.score += 25; break;
    }
    case FEATURE_RUNESTONE:
        switch(roll(4)) {
        case 0: blessing(); break;
        case 1: cure(); break;
        case 2: if(game.magic_resistance < 127) ++game.magic_resistance; else blessing(); break;
        case 3: player_take_magic_damage(3 + roll(4), 6); break;
        } break;
    case FEATURE_ALCHEMICAL_POOL: pool(game.inventory[choice.slot]); break;
    default: break;
    }
    switch(type) {
    case FEATURE_FOUNTAIN: status(F("The fountain dries up.")); break;
    case FEATURE_HEALING_SPRING: status(F("The spring dries up.")); break;
    case FEATURE_STATUE: status(F("The statue crumbles.")); break;
    case FEATURE_SARCOPHAGUS: status(F("The sarcophagus crumbles.")); break;
    case FEATURE_ENCHANTED_FORGE: status(F("The forge grows cold.")); break;
    case FEATURE_ALCHEMICAL_POOL: status(F("The pool evaporates.")); break;
    case FEATURE_RUNESTONE: status(F("The runestone fades away.")); break;
    default: status(F("The altar crumbles.")); break;
    }
    return true;
}
// Pickup transactions unwind before feature messages and enemy rendering.
__attribute__((noinline)) void finish_pickup_turn() {
    if(session.pending_pickups) ++session.pickup_turns;
    else end_turn();
}
__attribute__((noinline)) void complete_ground_pickups() {
    if(!session.pending_pickups) return;
    session.pending_pickups = false;
    activate_entry_feature(feature_at(game.player));
}
__attribute__((noinline)) void complete_player_move(FeatureChoice choice) {
    if(!session.pending_move) return;
    complete_ground_pickups();
    uint8_t index = session.pending_feature;
    session.pending_feature = NONE;
    session.pending_move = false;
    resolve_feature(index, choice);
    if(!session.ended) end_turn();
    while(session.pickup_turns && !session.ended) {
        --session.pickup_turns;
        end_turn();
    }
    session.pickup_turns = 0;
}
} // namespace rogue
