#include "companions.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "features.hpp"
#include "world.hpp"
#include "status.hpp"
#include "spells.hpp"
#include "sim_hooks.hpp"

namespace rogue {
static uint8_t gap(Position a, Position b) {
    uint8_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    uint8_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx + dy;
}
uint8_t free_pet_slot() {
    for(uint8_t i = HOSTILE_MONSTERS; i < MONSTERS; ++i)
        if(!game.monsters[i].type) return i;
    return NONE;
}
uint8_t pet_count() {
    uint8_t count = 0;
    for(uint8_t i = HOSTILE_MONSTERS; i < MONSTERS; ++i) count += game.monsters[i].type != 0;
    return count;
}
static int8_t axis(uint8_t a, uint8_t b) { return a < b ? 1 : a > b ? -1 : 0; }
static bool safe_pet_tile(Position p) {
    if(p == game.player || blocked(p.x, p.y) || monster_at(p) != NONE) return false;
    uint8_t f = feature_at(p);
    return f == NONE || !feature_is_trap(game.features[f].id);
}
// At most three cardinal steps. Unlike a spell ray, following can round a
// corner. Check the three monotonic routes without a map search or path array.
static bool connected_landing(Position p) {
    int8_t dx=axis(game.player.x,p.x), dy=axis(game.player.y,p.y);
    uint8_t length=gap(game.player,p);
    if(length<=1) return true;
    bool xopen=dx && !blocked(game.player.x+dx,game.player.y);
    bool yopen=dy && !blocked(game.player.x,game.player.y+dy);
    if(dx && dy) {
        if(length==2) return xopen || yopen;
        if((xopen || yopen) && !blocked(game.player.x+dx,game.player.y+dy)) return true;
        if(p.y==game.player.y+dy) return xopen && !blocked(game.player.x+2*dx,game.player.y);
        return yopen && !blocked(game.player.x,game.player.y+2*dy);
    }
    return (xopen || yopen) && (length==2 || !blocked(game.player.x+2*dx,game.player.y+2*dy));
}
// Twenty-four Manhattan-ring candidates: bounded flash data, no path array.
// No RNG: failed casts and automatic recall cannot disturb gameplay rolls.
static const int8_t PROGMEM landing_offsets[] = {
    0,-1, 1,0, 0,1, -1,0,
    0,-2, 1,-1, 2,0, 1,1, 0,2, -1,1, -2,0, -1,-1,
    0,-3, 1,-2, 2,-1, 3,0, 2,1, 1,2, 0,3, -1,2, -2,1, -3,0, -2,-1, -1,-2
};
__attribute__((noinline)) static Position pet_landing(bool offscreen) {
    uint8_t light = offscreen ? player_light_radius() : 0;
    if(offscreen && light >= 3) {
        // Eight radius-three corner cells. The direct sight ray crosses the
        // diagonal midpoint, while a three-step cardinal route can round it.
        // This prefers actually hidden cells even under a bright lantern.
        for(uint8_t n=26;n<sizeof landing_offsets;n+=2) {
            int8_t x=landing_offsets[n],y=landing_offsets[n+1];
            if(!x || !y || !blocked(game.player.x+(x<0 ? -1 : 1),game.player.y+(y<0 ? -1 : 1))) continue;
            Position p{static_cast<uint8_t>(game.player.x+x),static_cast<uint8_t>(game.player.y+y)};
            if(safe_pet_tile(p) && connected_landing(p)) return p;
        }
        return {NONE,NONE};
    }
    int8_t bx = session.pet_trail.x < MAP_W ? axis(session.pet_trail.x, game.player.x) : 0;
    int8_t by = session.pet_trail.y < MAP_H ? axis(session.pet_trail.y, game.player.y) : 0;
    bx = -bx; by = -by;
    Position behind{static_cast<uint8_t>(game.player.x+bx),static_cast<uint8_t>(game.player.y+by)};
    if((bx || by) && (!offscreen || !in_light_radius(bx,by,light)) && safe_pet_tile(behind)) return behind;
    for(uint8_t n=0;n<sizeof landing_offsets;n+=2) {
        int8_t x=landing_offsets[n],y=landing_offsets[n+1];
        if((x==-bx && y==-by && (bx || by)) || (offscreen && in_light_radius(x,y,light))) continue;
        Position p{static_cast<uint8_t>(game.player.x+x),static_cast<uint8_t>(game.player.y+y)};
        if(safe_pet_tile(p) && connected_landing(p)) return p;
    }
    Position ahead{static_cast<uint8_t>(game.player.x-bx),static_cast<uint8_t>(game.player.y-by)};
    if((bx || by) && (!offscreen || !in_light_radius(bx,by,light)) && safe_pet_tile(ahead)) return ahead;
    return {NONE, NONE};
}
static Position creation_landing;

bool can_create_pet(uint8_t type, bool announce) {
    if(!BALANCE_VALUE("pets", 1) || type < HOUND || type > GOLEM) return false;
    const char PROGMEM* failure = nullptr;
    if(free_pet_slot() == NONE) failure = F("You already have four companions.");
    for(uint8_t i = HOSTILE_MONSTERS; i < MONSTERS; ++i)
        if(game.monsters[i].type == type) failure = F("That companion already follows you.");
    if(!failure) {
        creation_landing=pet_landing(false);
        if(creation_landing.x==NONE) failure=F("There is no safe place to summon.");
    }
    if(failure && announce) status(failure);
    return !failure;
}
bool create_pet(uint8_t type, PetSource source) {
    if(!can_create_pet(type)) return false;
    create_prepared_pet(type,source);
    return true;
}
__attribute__((noinline)) void create_prepared_pet(uint8_t type, PetSource source) {
    uint8_t slot = free_pet_slot();
    game.monsters[slot] = {creation_landing, type, monster_health(type), 0, {0, 0}, MON_SUMMONED};
    session.pet_leash[slot - HOSTILE_MONSTERS] = 0x40;
    SIM_EVENT(sim::EventKind::PetAcquired, slot, type, 1, static_cast<uint8_t>(source));
    (void)source;
    monster_status(game.monsters[slot], F("joins you!"));
}
bool taming_target(uint8_t index) {
    if(index >= HOSTILE_MONSTERS || !game.monsters[index].type || !player_can_see_monster(index) ||
       (game.monsters[index].type==MIMIC && !(game.monsters[index].state & MON_AGGRO))) return false;
    Position p = game.monsters[index].pos;
    int16_t dx = int16_t(p.x) - game.player.x, dy = int16_t(p.y) - game.player.y;
    return dx * dx + dy * dy <= 16 && can_see(p) && line_of_effect(game.player, p);
}
uint8_t taming_chance(const Monster& target, TamingMethod method) {
    uint8_t resistance = tame_resistance(target.type);
    if(!target.type || resistance == 100 || (target.state & MON_SUMMONED)) return 0;
    uint8_t chance = method == TamingMethod::Scroll ? 100 : method == TamingMethod::Wand ? 75 : 50;
    if(method != TamingMethod::Scroll) {
        uint8_t maximum = monster_health(target.type);
        if(uint16_t(target.hp) * 2 <= maximum) chance += 15;
        if(uint16_t(target.hp) * 4 <= maximum || monster_effect(target, MON_WEAKENED)) chance += 10;
    }
    uint8_t result = static_cast<uint8_t>((chance > 100 ? 100 : chance) - resistance);
    return result < 5 ? 5 : result;
}
__attribute__((noinline)) static uint8_t convert_tame_target(uint8_t index, TamingMethod method) {
    if(!BALANCE_VALUE("pets", 1) || index >= MONSTERS || !game.monsters[index].type) return NONE;
    if(free_pet_slot() == NONE) return 254;
    Monster& target = game.monsters[index];
    uint8_t chance = friendly_slot(index) ? 0 : taming_chance(target, method);
    bool success = chance && (chance == 100 || roll(100) < chance);
    SIM_EVENT(sim::EventKind::TamingAttempt, index, target.type, success, static_cast<uint8_t>(method));
    if(!success) return index;
    uint8_t slot = free_pet_slot();
    game.monsters[slot] = target;
    target = {};
    session.pet_leash[slot - HOSTILE_MONSTERS] = 0x40;
    SIM_EVENT(sim::EventKind::PetAcquired, slot, game.monsters[slot].type, 1, 1 + static_cast<uint8_t>(method));
    return slot; // No reference to the moved hostile is used after conversion.
}
__attribute__((noinline)) bool tame_monster(uint8_t index, TamingMethod method) {
    uint8_t result = convert_tame_target(index, method);
    if(result == NONE) return false;
    if(result == 254) { status(F("You already have four companions.")); return false; }
    bool success = result != index && friendly_slot(result);
    monster_status(game.monsters[result], success ? F("joins you!") : F("resists your friendship."));
    return success;
}
__attribute__((noinline)) bool dismiss_pet(uint8_t index) {
    if(studying() || session.ended || game.paralyzed || !friendly_slot(index) || !game.monsters[index].type) return false;
    SIM_EVENT(sim::EventKind::PetDismissed, index, game.monsters[index].type);
    monster_status(game.monsters[index], F("leaves you."));
    game.monsters[index] = {};
    session.pet_leash[index - HOSTILE_MONSTERS] = 0;
    end_turn();
    return true;
}
void place_floor_pets() {
    for(uint8_t i = HOSTILE_MONSTERS; i < MONSTERS; ++i) if(game.monsters[i].type) {
        Position p = pet_landing(false);
        // Custom test maps may lack four entrance squares. Keep the creature
        // unplaced; a later recall retries safely, never deletes or overlaps it.
        game.monsters[i].pos = p;
    }
}
void delay_pet_recall(uint8_t index) {
    if(friendly_slot(index)) {
        session.pet_leash[index - HOSTILE_MONSTERS] = 0x60;
        game.monsters[index].state |= MON_RECALL_GRACE;
    }
}
// Low four bits record combat this turn; bit four throttles battle messages.
static uint8_t pet_turn_flags=0;
void age_pet_leashes() {
    // A cast/acquisition already supplies meaningful feedback for this turn.
    // Deaths and traps still announce themselves independently.
    pet_turn_flags=status_has_message() ? 16 : 0;
    for(uint8_t p=0;p<PETS;++p) {
        uint8_t& leash=session.pet_leash[p];
        Monster& pet=game.monsters[HOSTILE_MONSTERS+p];
        // Resume conservatively restarts grace instead of undoing displacement.
        if(pet.type && (pet.state & MON_RECALL_GRACE) && !(leash & 0xf0)) leash=0x60;
        if(leash & 0xf0) leash-=0x10;
        if(!(leash & 0xf0)) pet.state &= static_cast<uint8_t>(~MON_RECALL_GRACE);
    }
}
static bool pet_sees(const Monster& pet, const Monster& foe) {
    if(pet.state & MON_BLINDED) return false;
    if((naturally_invisible(foe.type) || monster_effect(foe, MON_INVISIBLE)) &&
       !(monster_flags(pet.type) & MON_SEE_INVIS)) return false;
    return gap(pet.pos, foe.pos) == 1 || line_of_effect(pet.pos, foe.pos);
}
__attribute__((noinline)) static uint8_t pet_target(uint8_t index) {
    const Monster& pet = game.monsters[index];
    if(gap(pet.pos, game.player) > 3 || (pet.state & MON_AFRAID) || monster_effect(pet, MON_CONFUSED)) return NONE;
    uint8_t best = NONE, priority = 255;
    if(pet.type != WISP) {
        uint16_t key;
        __builtin_memcpy(&key,&pet.pos,sizeof key);
        for(uint8_t i=0;i<HOSTILE_MONSTERS;++i) {
            const Monster& foe=game.monsters[i];
            uint16_t cell;
            __builtin_memcpy(&cell,&foe.pos,sizeof cell);
            uint16_t delta=cell-key;
            if((delta!=1 && delta!=0xffff && delta!=0x100 && delta!=0xff00) || !foe.type || !pet_sees(pet,foe)) continue;
            uint8_t score=(foe.state & MON_AGGRO) ? 0 : 2;
            if(score<priority) { best=i; priority=score; }
        }
        if(best!=NONE) return best;
        // When no adjacent opponent exists, take one defensive approach step.
        // Pick a nearby threat first, then do one visibility query; no map path.
        for(uint8_t i=0;i<HOSTILE_MONSTERS;++i) {
            const Monster& foe=game.monsters[i];
            if(!foe.type || static_cast<uint8_t>(foe.pos.x-pet.pos.x+3)>6 ||
               static_cast<uint8_t>(foe.pos.y-pet.pos.y+3)>6) continue;
            uint8_t d=gap(foe.pos,pet.pos);
            if(d>3 || gap(foe.pos,game.player)>3) continue;
            uint8_t score=d*4+((foe.state & MON_AGGRO) ? 0 : 2);
            if(score<priority) { best=i; priority=score; }
        }
        return best!=NONE && pet_sees(pet,game.monsters[best]) ? static_cast<uint8_t>(64+best) : NONE;
    }
    for(uint8_t i = 0; i < HOSTILE_MONSTERS; ++i) {
        const Monster& foe = game.monsters[i];
        if(!foe.type) continue;
        if(pet.type == WISP) {
            if(static_cast<uint8_t>(foe.pos.x-pet.pos.x+3)>6 || static_cast<uint8_t>(foe.pos.y-pet.pos.y+3)>6) continue;
        } else if(!((foe.pos.x == pet.pos.x && static_cast<uint8_t>(foe.pos.y-pet.pos.y+1)<=2) ||
                    (foe.pos.y == pet.pos.y && static_cast<uint8_t>(foe.pos.x-pet.pos.x+1)<=2))) continue;
        uint8_t d = gap(foe.pos, pet.pos);
        if((pet.type == WISP ? d > 3 : d != 1) || gap(foe.pos, game.player) > 4 || !pet_sees(pet, foe)) continue;
        uint8_t score = d * 4 + ((foe.state & MON_AGGRO) ? 0 : 2);
        if(score < priority) { best = i; priority = score; }
    }
    return best;
}
// A fast actor can repeat an attack against its surviving target. A movement
// decision never buys an extra attack in the same ordinary player turn.
static uint8_t pet_query_actor=NONE, pet_query_target=NONE;
void begin_pet_turn(uint8_t index) {
    // Query lazily: a slow actor may receive no action, and stun can consume
    // its entire budget. Neither case needs targeting or terrain visibility.
    pet_query_actor=index; pet_query_target=254;
}
void end_pet_turn() { pet_query_actor=NONE; }

__attribute__((noinline)) static bool follow_pet(uint8_t index, Position goal, bool combat) {
    Monster& pet = game.monsters[index];
    if(!combat && session.pet_trail.x < MAP_W && gap(pet.pos, session.pet_trail) < gap(pet.pos, goal)) goal = session.pet_trail;
    uint8_t range = gap(pet.pos, game.player);
    if(!combat && range <= 1) return false;
    // Cardinal greedy motion with deterministic side steps. Failed progress
    // falls back to recall; no map search or large automatic path storage.
    uint8_t start = static_cast<uint8_t>((index + game.turns / 4) & 3);
    int8_t dx = axis(pet.pos.x, goal.x), dy = axis(pet.pos.y, goal.y);
    for(uint8_t n = 0; n < 6; ++n) {
        uint8_t d = (start + n - 2) & 3;
        int8_t x = n == 0 ? dx : n == 1 ? 0 : d == 0 ? 1 : d == 1 ? -1 : 0;
        int8_t y = n == 0 ? 0 : n == 1 ? dy : d == 2 ? 1 : d == 3 ? -1 : 0;
        if(!x && !y) continue;
        Position p{static_cast<uint8_t>(pet.pos.x + x), static_cast<uint8_t>(pet.pos.y + y)};
        if(combat ? gap(p,game.player)>3 : n >= 2 && gap(p, game.player) > range) continue;
        if(p == game.player || wall_at(p.x, p.y)) continue;
        uint8_t door = door_at(p);
        if(door != NONE && !door_open(door)) {
            if(monster_flags(pet.type) & MON_OPENER) { open_door(door); return true; }
            continue;
        }
        uint8_t f = feature_at(p);
        if(f != NONE && feature_is_trap(game.features[f].id) && !feature_is_hidden(game.features[f].id)) continue;
        uint8_t other = monster_at(p);
        if(other != NONE) {
            // Move the leading stationary pet behind the follower, only when
            // it strictly improves the follower's distance. Never attack pets.
            if(!friendly_slot(other) || gap(p, game.player) >= range ||
               gap(game.monsters[other].pos, game.player) > 1 ||
               feature_at(pet.pos) != NONE) continue;
            game.monsters[other].pos = pet.pos;
        }
        pet.pos = p;
        return true;
    }
    return false;
}
__attribute__((noinline)) uint8_t pet_action(uint8_t index) {
    Monster& pet = game.monsters[index];
    if((pet.state & MON_AFRAID) && !roll(32)) pet.state &= static_cast<uint8_t>(~MON_AFRAID);
    if(pet.stun) { --pet.stun; return 0; }
    if(pet_query_actor==index && pet_query_target==254) {
        pet_query_target=pet_target(index);
        if(pet_query_target!=NONE) pet_turn_flags |= static_cast<uint8_t>(1u<<(index-HOSTILE_MONSTERS));
    }
    if(pet_query_actor==index && pet_query_target==128) return 0;
    uint8_t target = pet_query_actor==index ? pet_query_target : pet_target(index);
    bool approaching=target!=NONE && (target & 64);
    if(target!=NONE) target &= 15;
    if(target!=NONE && (!game.monsters[target].type || monster_effect(pet,MON_CONFUSED) || (pet.state & MON_AFRAID))) target=NONE;
    if(target != NONE) {
        if(approaching) {
            Position before=pet.pos;
            follow_pet(index,game.monsters[target].pos,true);
            // An approach never grants another attack in this player turn.
            if(pet_query_actor==index) pet_query_target=128;
            return pet.pos!=before ? 3 : 0;
        }
        return static_cast<uint8_t>((pet.type == WISP ? 32 : 16) + target);
    }
    if(monster_effect(pet, MON_CONFUSED) && roll(2)) return 0;
    uint8_t& leash = session.pet_leash[index - HOSTILE_MONSTERS];
    uint8_t distance = gap(pet.pos, game.player);
    if(distance <= 1) { leash &= 0xf0; return 0; }
    Position before = pet.pos;
    bool moved = pet.pos.x < MAP_W && follow_pet(index,game.player,false);
    bool progress = moved && gap(pet.pos, game.player) < distance;
    if(!progress && distance<=3 && line_of_effect(pet.pos,game.player)) {
        leash &= 0xf0;
        return pet.pos!=before ? 3 : 0;
    }
    uint8_t stuck = progress && distance <= 3 ? 0 : static_cast<uint8_t>((leash & 15) < 15 ? (leash & 15) + 1 : 15);
    leash = static_cast<uint8_t>((leash & 0xf0) | stuck);
    if(!(leash & 0xf0) && ((distance >= 8 && stuck >= 2) || stuck >= 6 || pet.pos.x == NONE)) {
        Position p = pet_landing(true);
        if(p.x == NONE && (distance >= 12 || stuck >= 10 || pet.pos.x == NONE)) p = pet_landing(false);
        if(p.x != NONE) {
            pet.pos = p; leash = 0x80;
            SIM_EVENT(sim::EventKind::PetRecall, index, pet.type, distance, can_see(p));
            return 0;
        }
    }
    return pet.pos != before ? 3 : 0;
}
uint8_t blocking_pet(uint8_t index) {
    const Monster& foe = game.monsters[index];
    int8_t dx = axis(foe.pos.x, game.player.x), dy = axis(foe.pos.y, game.player.y);
    for(uint8_t axis_index = 0; axis_index < 2; ++axis_index) {
        if(axis_index ? !dy : !dx) continue;
        Position p{static_cast<uint8_t>(foe.pos.x + (axis_index ? 0 : dx)),
                   static_cast<uint8_t>(foe.pos.y + (axis_index ? dy : 0))};
        if(blocked(p.x, p.y)) continue;
        uint8_t target = monster_at(p);
        if(friendly_slot(target) && pet_sees(foe, game.monsters[target])) return target;
        if(target == NONE && p != game.player) return NONE; // Ordinary move can advance.
    }
    return NONE;
}
uint8_t pet_fire_target(uint8_t attacker) {
    const Monster& source = game.monsters[attacker];
    int8_t dx = axis(source.pos.x, game.player.x), dy = axis(source.pos.y, game.player.y);
    if(dx && dy) return NONE;
    uint8_t range = gap(source.pos, game.player);
    if(!range || range > 5) return NONE;
    RayResult ray = scan_ray(source.pos, dx, dy, range);
    return friendly_slot(ray.monster) && pet_sees(source, game.monsters[ray.monster]) ? ray.monster : NONE;
}
__attribute__((noinline)) void creature_fire(uint8_t attacker, uint8_t target) {
    if(attacker >= HOSTILE_MONSTERS || !friendly_slot(target) || !game.monsters[target].type) return;
#if defined(ARDUROGUE2_SIM)
    sim::DamageScope source_scope(sim::Cause::Fire, game.monsters[attacker].type);
#endif
    Position center = game.monsters[target].pos;
    monster_status(game.monsters[attacker], F("breathes fire!"));
    animate_lightning(game.monsters[attacker].pos, center);
    animate_fire_burst(center);
    SIM_EVENT(sim::EventKind::MonsterAttack, attacker, game.monsters[attacker].type);
    fire_burst_damage(center, false);
    if(square_contains(center, game.player, 1)) player_take_fire_damage(6 + roll(8), 12);
}
__attribute__((noinline)) void creature_attack(uint8_t attacker, uint8_t target, bool magical) {
    if(attacker >= MONSTERS || target >= MONSTERS || friendly_slot(attacker) == friendly_slot(target) ||
       !game.monsters[attacker].type || !game.monsters[target].type) return;
    Monster& source = game.monsters[attacker];
    Monster& victim = game.monsters[target];
    bool pet = friendly_slot(attacker);
    if(!pet) pet_turn_flags |= static_cast<uint8_t>(1u<<(target-HOSTILE_MONSTERS));
    #if defined(ARDUROGUE2_SIM)
    sim::DamageScope source_scope(pet ? sim::Cause::Pet : sim::Cause::Monster, source.type);
#endif
    bool visible = !(pet_turn_flags & 16) && can_see(source.pos) && player_can_see_monster(attacker);
    SIM_EVENT(sim::EventKind::PetAttack, attacker, source.type, 1, pet);
    if(!magical && !physical_attack_hits(monster_dexterity(source.type), monster_dexterity(victim.type))) {
        if(visible && !(pet_turn_flags & 16)) { pet_turn_flags |= 16; SIM_EVENT(sim::EventKind::PetMessage, attacker, source.type); status_capitalize(); status_visible_monster(source); status(F("misses!")); }
        return;
    }
    uint8_t raw = magical ? static_cast<uint8_t>(2 + roll(3)) : static_cast<uint8_t>(monster_strength(source.type) + roll(3));
    if(monster_effect(source, MON_WEAKENED)) raw = static_cast<uint8_t>((raw + 1) / 2);
    uint8_t damage = magical ? raw : physical_damage_after_armor(raw, armor_absorption(monster_armor(victim.type), 0));
    uint8_t dealt = damage < victim.hp ? damage : victim.hp;
    SIM_EVENT(sim::EventKind::PetHit, attacker, source.type, 1, pet);
    SIM_EVENT(sim::EventKind::PetDealtDamage, attacker, source.type, dealt, pet);
    if(visible && !(pet_turn_flags & 16)) { pet_turn_flags |= 16; SIM_EVENT(sim::EventKind::PetMessage, attacker, source.type); status_capitalize(); status_visible_monster(source); status(F("hits!")); }
    bool lethal = damage >= victim.hp;
    if(pet && lethal) {
        SIM_EVENT(sim::EventKind::PetKilled, attacker, source.type,
                  (victim.state & MON_SUMMONED) ? 0 : monster_xp(victim.type), victim.type);
    }
    damage_monster(target, damage, pet);
    if(!victim.type) return;
    uint16_t flags = monster_flags(source.type);
    if((flags & MON_POISON) && !monster_effect(victim, MON_WEAKENED) && !roll(4)) {
        set_monster_effect(victim, MON_WEAKENED, 15);
        SIM_EVENT(sim::EventKind::PetControl, attacker, source.type, 15, MON_WEAKENED);
    }
    if(source.type == SPIDER && !monster_effect(victim, MON_SLOWED) && !roll(4)) {
        set_monster_effect(victim, MON_SLOWED, 4);
        SIM_EVENT(sim::EventKind::PetControl, attacker, source.type, 4, MON_SLOWED);
    }
    if((flags & MON_CONFUSE_HIT) && !monster_effect(victim, MON_CONFUSED) && !roll(4)) set_monster_effect(victim, MON_CONFUSED, 4);
    if(((flags & MON_PARALYZE_HIT) || source.type == GOLEM) && !victim.stun && !roll(4)) {
        victim.stun = source.type == GOLEM ? 2 : 3;
        SIM_EVENT(sim::EventKind::PetControl, attacker, source.type, victim.stun, 4);
    }
    if(flags & MON_VAMPIRE) {
        uint8_t maximum = monster_health(source.type);
        uint16_t hp = static_cast<uint16_t>(source.hp) + dealt;
        source.hp = hp > maximum ? maximum : static_cast<uint8_t>(hp);
    }
}
void recover_pets() {
    if(game.turns & 31) return;
    for(uint8_t i = HOSTILE_MONSTERS; i < MONSTERS; ++i) {
        Monster& pet = game.monsters[i];
        if(!pet.type || (pet_turn_flags & (1u<<(i-HOSTILE_MONSTERS))) || pet.hp >= monster_health(pet.type)) continue;
        bool fighting = false;
        for(uint8_t h = 0; h < HOSTILE_MONSTERS; ++h) {
            const Monster& foe=game.monsters[h];
            if(foe.type && static_cast<uint8_t>(foe.pos.x-pet.pos.x+3)<=6 &&
               static_cast<uint8_t>(foe.pos.y-pet.pos.y+3)<=6 && gap(pet.pos,foe.pos)<=3 && pet_sees(pet,foe)) {
                fighting=true; break;
            }
        }
        if(!fighting) { ++pet.hp; SIM_EVENT(sim::EventKind::PetRegenerated, i, pet.type); }
    }
}
}
