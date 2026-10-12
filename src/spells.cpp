#include "spells.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "features.hpp"
#include "status.hpp"
#include "sim_hooks.hpp"

namespace rogue {
uint8_t player_max_mana() {
    uint8_t maximum = 6 + game.level / 3;
    return maximum > 16 ? 16 : maximum;
}
void regenerate_mana() {
    if(!(game.turns & 15) && game.hunger && !session.ended && game.mana < player_max_mana()) {
        ++game.mana;
        SIM_EVENT(sim::EventKind::ManaRegenerated);
    }
}
static bool can_read() {
    return !session.ended && game.hp && !game.paralyzed && !game.blindness && !game.confused;
}
bool begin_study(uint8_t slot) {
    if(studying() || slot >= INVENTORY || game.inventory[slot].type != SPELLBOOK ||
       game.inventory[slot].info >= SPELL_COUNT) return false;
    uint8_t spell = game.inventory[slot].info;
    if(!can_read()) { status(F("You cannot study now.")); return false; }
    if(game.known_spells & spell_bit(spell)) { status(F("You already know this spell.")); return false; }
    if(game.hunger < study_food_required(spell)) { status(F("You need to eat before studying.")); return false; }
    status(F("You begin studying."));
    session.study_slot = slot; session.study_spell = spell;
    session.study_progress = 0; session.study_floor = game.floor;
    session.study_origin = game.player; session.study_message = false;
    SIM_EVENT(sim::EventKind::StudyStarted, slot, spell);
    return true;
}
void cancel_study(bool announce) {
    if(!studying()) return;
    SIM_EVENT(sim::EventKind::StudyInterrupted, session.study_slot, session.study_spell, 1,
              session.study_message ? 1 : session.ended ? 2 : game.hunger < STUDY_HUNGRY ? 3 : 0);
    session.study_slot = session.study_spell = NONE;
    session.study_progress = 0;
    if(announce) status(F("You lose your concentration."));
}
__attribute__((noinline)) bool advance_study() {
    if(!studying()) return false;
    SIM_EVENT(sim::EventKind::StudyTurn, session.study_slot, session.study_spell);
    end_turn();
    if(!studying()) return true; // Death/floor transition canceled the activity.
    ++session.study_progress;
    if(session.study_message || !can_read() || game.hunger < STUDY_HUNGRY ||
       game.floor != session.study_floor || game.player != session.study_origin ||
       game.inventory[session.study_slot].type != SPELLBOOK ||
       game.inventory[session.study_slot].info != session.study_spell) {
        cancel_study(); return true;
    }
    if(session.study_progress == spell_study_turns(session.study_spell)) {
        uint8_t spell = session.study_spell, slot = session.study_slot;
        session.study_slot = session.study_spell = NONE;
        session.study_progress = 0;
        game.known_spells |= spell_bit(spell);
        game.inventory[slot] = {};
        if(session.repeat_slot == slot) session.repeat_slot = NONE;
        SIM_EVENT(sim::EventKind::SpellLearned, slot, spell);
        SIM_EVENT(sim::EventKind::Consumed, slot, SPELLBOOK);
        status(F("You learn")); status(spell_name(spell), '!');
    }
    return true;
}

bool exchange_target(uint8_t index) {
    return index < MONSTERS && player_can_see_monster(index) &&
        in_light_radius(int16_t(game.monsters[index].pos.x) - game.player.x,
                        int16_t(game.monsters[index].pos.y) - game.player.y, 3) &&
        can_see(game.monsters[index].pos);
}
// Shared transient movement scratch: no renderer or trap callback reenters knockback.
static Position knockback_landing;
__attribute__((noinline)) static bool knockback_step(uint8_t index,int8_t dx,int8_t dy) {
    Position from=game.monsters[index].pos;
    int16_t x=int16_t(from.x)+dx,y=int16_t(from.y)+dy;
    Position next{static_cast<uint8_t>(x),static_cast<uint8_t>(y)};
    if(blocked(x,y) || (dx && dy && (blocked(x,from.y) || blocked(from.x,y))) || next==game.player || monster_at(next)!=NONE) {
        if(game.monsters[index].stun<2) game.monsters[index].stun=2;
        return false;
    }
    knockback_landing=next; game.monsters[index].pos=next;
    return true;
}
__attribute__((noinline)) uint8_t knockback_monster(uint8_t index,int8_t dx,int8_t dy,uint8_t distance) {
    if(index>=MONSTERS || !game.monsters[index].type) return 0;
    uint8_t moved=0;
    while(moved<distance && !session.ended && game.monsters[index].type) {
        if(!knockback_step(index,dx,dy)) break;
        ++moved; activate_monster_trap(index);
        if(!game.monsters[index].type || game.monsters[index].pos!=knockback_landing) break;
    }
    SIM_EVENT(sim::EventKind::SpellDisplacement,index,0,moved);
    return moved;
}
__attribute__((noinline)) static void blink() {
    Position destination = game.player;
    uint8_t count = 0;
    for(int8_t dy = -3; dy <= 3; ++dy) for(int8_t dx = -3; dx <= 3; ++dx) {
        if((!dx && !dy) || !in_light_radius(dx, dy, 3)) continue;
        int16_t x = int16_t(game.player.x) + dx, y = int16_t(game.player.y) + dy;
        Position pos{static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
        if(blocked(x, y) || monster_at(pos) != NONE) continue;
        if(!roll(++count)) destination = pos;
    }
    if(!count) { status(F("There is nowhere to blink.")); return; }
    game.player = destination; session.forced_entry = true;
    status(F("You blink!"));
}
__attribute__((noinline)) static void shockwave() {
    uint16_t adjacent = 0;
    for(uint8_t i = 0; i < MONSTERS; ++i)
        if(game.monsters[i].type && square_contains(game.player, game.monsters[i].pos, 1)) adjacent |= uint16_t(1u << i);
    status(adjacent ? F("A shockwave erupts!") : F("Nothing is nearby."));
    // Stable slot order; capture membership before movement or trap summons.
    for(uint8_t i = 0; i < MONSTERS && !session.ended; ++i) if(adjacent & (1u << i)) {
        Position pos = game.monsters[i].pos;
        int8_t dx = pos.x > game.player.x ? 1 : pos.x < game.player.x ? -1 : 0;
        int8_t dy = pos.y > game.player.y ? 1 : pos.y < game.player.y ? -1 : 0;
        game.monsters[i].state |= MON_AGGRO;
        SIM_EVENT(sim::EventKind::PlayerAttack, i, game.monsters[i].type);
        knockback_monster(i, dx, dy, 2 + roll(2));
    }
}
const LightningProfile PROGMEM LIGHTNING_SPELL = {5,2,{3,3,0},{3,2,0}};
const LightningProfile PROGMEM LIGHTNING_WAND = {6,2,{6,4,0},{5,4,0}};
const LightningProfile PROGMEM LIGHTNING_POWERFUL = {6,3,{10,7,4},{5,4,4}};

__attribute__((noinline)) uint8_t lightning_target(Position origin, uint16_t hit_mask) {
    static_assert(MONSTERS<=16,"lightning hit mask is too small");
    uint8_t best=NONE, nearest=3;
    for(uint8_t i=0;i<MONSTERS;++i) {
        const Monster& m=game.monsters[i];
        if(!m.type || (hit_mask & (1u<<i)) || !square_contains(origin,m.pos,2)) continue;
        uint8_t dx=m.pos.x>origin.x ? m.pos.x-origin.x : origin.x-m.pos.x;
        uint8_t dy=m.pos.y>origin.y ? m.pos.y-origin.y : origin.y-m.pos.y;
        uint8_t distance=dx>dy ? dx : dy;
        if(distance>=nearest || !player_can_see_monster(i) || !can_see(m.pos) || !line_of_effect(origin,m.pos)) continue;
        best=i; nearest=distance; // Ascending slots provide deterministic ties.
    }
    return best;
}
// Scratch is transient, shared by sequential activations; callbacks do not cast.
static struct { Position origin; uint8_t target; } lightning_state;
__attribute__((noinline)) static void lightning_start(int8_t dx,int8_t dy,uint8_t range) {
    RayResult ray=scan_ray(game.player,dx,dy,range);
    animate_lightning(game.player,ray.end);
    lightning_state={ray.end,ray.monster};
}
__attribute__((noinline)) static void lightning_hit(uint8_t minimum,uint8_t spread,uint8_t jump) {
    uint8_t hit=lightning_state.target;
    SIM_EVENT(sim::EventKind::PlayerAttack,hit,game.monsters[hit].type);
    (void)jump;
    uint8_t damage=minimum+roll(spread);
    SIM_EVENT(sim::EventKind::LightningDamage,hit,game.monsters[hit].type,damage<game.monsters[hit].hp ? damage : game.monsters[hit].hp,jump);
    damage_monster(hit,damage,true);
    if(game.monsters[hit].type) monster_status(game.monsters[hit],F("is struck!"));
}
__attribute__((noinline)) void lightning_ray(int8_t dx,int8_t dy,const LightningProfile PROGMEM* profile,uint16_t& hit_mask) {
    lightning_start(dx,dy,profile->range);
    uint8_t hit=lightning_state.target;
    if(hit==NONE || (hit_mask & (1u<<hit))) return;
    for(uint8_t jump=0;jump<profile->targets && !session.ended;++jump) {
        hit_mask |= uint16_t(1u<<lightning_state.target);
        // Origin survives death of the target; never retain Monster references.
        lightning_state.origin=game.monsters[lightning_state.target].pos;
        lightning_hit(profile->minimum[jump],profile->spread[jump],jump);
        if(jump+1>=profile->targets) break;
        hit=lightning_target(lightning_state.origin,hit_mask);
        if(hit==NONE) break;
        animate_lightning(lightning_state.origin,game.monsters[hit].pos);
        lightning_state.target=hit;
    }
}
__attribute__((noinline)) static void fireburst(int8_t dx,int8_t dy) {
    RayResult ray=scan_ray(game.player,dx,dy,5);
    animate_ray(game.player,dx,dy,ray.steps);
    animate_fire_burst(ray.end);
    fire_burst_damage(ray.end,true,1,3,4);
}
__attribute__((noinline)) static void phase_step(int8_t dx,int8_t dy) {
    Position destination=game.player;
    uint8_t chosen=0;
    for(uint8_t step=1;step<=3;++step) {
        int16_t x=int16_t(game.player.x)+step*dx, y=int16_t(game.player.y)+step*dy;
        if(blocked(x,y)) break;
        Position pos{static_cast<uint8_t>(x),static_cast<uint8_t>(y)};
        if(step>=2 && monster_at(pos)==NONE) { destination=pos; chosen=step; }
    }
    SIM_EVENT(sim::EventKind::SpellDisplacement,NONE,PHASE_STEP,chosen);
    if(!chosen) { status(F("There is nowhere to phase.")); return; }
    game.player=destination; session.forced_entry=true;
    status(F("You phase forward!"));
}

static struct { Position origin; uint8_t remaining,hits; } ray_state;
__attribute__((noinline)) static uint8_t spell_ray_target(int8_t dx,int8_t dy) {
    RayResult ray=scan_ray(ray_state.origin,dx,dy,ray_state.remaining);
    animate_ray(ray_state.origin,dx,dy,ray.steps);
    ray_state.origin=ray.end; ray_state.remaining-=ray.steps;
    return ray.monster;
}
__attribute__((noinline)) static void spell_ray(uint8_t spell, int8_t dx, int8_t dy) {
    ray_state = {game.player, uint8_t(spell == SLUMBER ? 4 : 5), 0};
    do {
        uint8_t hit = spell_ray_target(dx, dy);
        if(hit == NONE) { if(!ray_state.hits) status(F("Nothing happens.")); break; }
        game.monsters[hit].state |= MON_AGGRO;
        SIM_EVENT(sim::EventKind::PlayerAttack, hit, game.monsters[hit].type);
        ++ray_state.hits;
        switch(spell) {
        case MAGIC_DART: case PIERCING_BOLT: case BLOOD_BOLT:
            // The ordinary wand magic path has no monster resistance stat.
            damage_monster(hit, (spell == MAGIC_DART ? 2 : spell == BLOOD_BOLT ? 6 : 3) + roll(spell == BLOOD_BOLT ? 4 : 3), true);
            if(game.monsters[hit].type) monster_status(game.monsters[hit], F("is struck!"));
            break;
        case SLUMBER: {
            uint8_t duration=3+roll(2);
            if(game.monsters[hit].stun<duration) game.monsters[hit].stun=duration;
            SIM_EVENT(sim::EventKind::SpellDisabled,hit,SLUMBER,duration);
            monster_status(game.monsters[hit],F("falls asleep!")); break;
        }
        case FROSTBIND: case CONFOUND:
            set_monster_effect(game.monsters[hit], spell == FROSTBIND ? MON_SLOWED : MON_CONFUSED, 4 + roll(3));
            monster_status(game.monsters[hit], spell == FROSTBIND ? F("slows down!") : F("looks confused!")); break;
        case REPULSION:
            monster_status(game.monsters[hit], F("is pushed back!")); knockback_monster(hit, dx, dy, 1 + roll(2)); break;
        case BANISH:
            monster_status(game.monsters[hit], F("vanishes!"));
            if(!teleport_monster(hit, false)) status(F("Nothing happens.")); break;
        default: break;
        }
    } while(spell == PIERCING_BOLT && ray_state.hits < 2 && ray_state.remaining && !session.ended);
}
__attribute__((noinline)) static void exchange(uint8_t target) {
    Position origin=game.player;
    game.player=game.monsters[target].pos; game.monsters[target].pos=origin;
    game.monsters[target].state|=MON_AGGRO;
    status(F("You exchange places!"));
    activate_monster_trap(target);
    if(!session.ended) activate_entry_feature(feature_at(game.player),true);
}
__attribute__((noinline)) static void sense_traps() {
    bool found=false;
    for(uint8_t i=0;i<MAP_FEATURES;++i) {
        const MapFeature& f=game.features[i];
        if(feature_is_trap(f.id) && feature_is_hidden(f.id) &&
           in_light_radius(int16_t(f.pos.x)-game.player.x,int16_t(f.pos.y)-game.player.y,4)) {
            reveal_feature(i); explore(f.pos); found=true;
        }
    }
    status(found ? F("Nearby traps appear on your map.") : F("No hidden traps nearby."));
}
__attribute__((noinline)) static void cleanse() {
    if(game.blindness) { game.blindness=0; status(F("Your sight clears.")); }
    else if(game.confused) { game.confused=0; status(F("Your thoughts clear.")); }
    else if(game.slowed) { game.slowed=0; status(F("Your movement clears.")); }
    else status(F("Nothing needs cleansing."));
}
__attribute__((noinline)) bool cast_spell(uint8_t spell, int8_t dx, int8_t dy, uint8_t target) {
    if(studying() || session.ended || game.paralyzed || spell >= SPELL_COUNT ||
       !(game.known_spells & spell_bit(spell))) return false;
    if(spell_directional(spell) && !((dx == 0 && (dy == -1 || dy == 1)) || (dy == 0 && (dx == -1 || dx == 1)))) return false;
    if(spell == EXCHANGE && !exchange_target(target)) { status(F("No visible target in range.")); return false; }
    if(game.mana < spell_cost(spell)) { status(F("Not enough mana.")); return false; }
    game.mana -= spell_cost(spell);
    SIM_EVENT(sim::EventKind::SpellCast, target, spell, spell_cost(spell));
    {
#if defined(ARDUROGUE2_SIM)
        sim::DamageScope source(sim::Cause::Spell, spell);
#endif
        if(spell_hp_cost(spell)) {
            // hurt_player is the raw HP/lifecycle primitive. It applies no armor,
            // MR, bonuses or fire mitigation and consumes Phoenix Heart normally.
            SIM_EVENT(sim::EventKind::HealthSacrificed,NONE,spell,game.hp<spell_hp_cost(spell) ? game.hp : spell_hp_cost(spell));
            hurt_player(spell_hp_cost(spell));
            if(session.ended) { SIM_EVENT(sim::EventKind::BloodDeath,NONE,spell); return true; }
            status(F("You sacrifice 3 HP!"));
        }
        if(spell == FIREBURST) fireburst(dx,dy);
        else if(spell == PHASE_STEP) phase_step(dx,dy);
        else if(spell == CHAIN_LIGHTNING) { uint16_t hit_mask=0; lightning_ray(dx,dy,&LIGHTNING_SPELL,hit_mask); }
        else if(spell_directional(spell)) spell_ray(spell,dx,dy);
        else switch(spell) {
        case BLINK: blink(); break; case SHOCKWAVE: shockwave(); break;
        case EXCHANGE: exchange(target); break; case SENSE_TRAPS: sense_traps(); break;
        case CLEANSE: cleanse(); break; default: break;
        }
    }
    if(!session.ended) end_turn();
    return true;
}

// Explicit weighted pools share the metadata; no ordinal category tests.
static uint8_t book_from_pool(uint16_t draw,bool basic,bool expanded) {
    uint8_t total=0;
    for(uint8_t s=0;s<SPELL_COUNT;++s) {
        auto def=spell_definition(s);
        if(!def.weight || (!expanded && expansion_spell(s)) || (def.category==SpellCategory::Basic)!=basic) continue;
        total+=expanded ? def.weight : 1;
    }
    uint8_t pick=draw%total;
    for(uint8_t s=0;s<SPELL_COUNT;++s) {
        auto def=spell_definition(s);
        if(!def.weight || (!expanded && expansion_spell(s)) || (def.category==SpellCategory::Basic)!=basic) continue;
        uint8_t weight=expanded ? def.weight : 1;
        if(pick<weight) return s;
        pick-=weight;
    }
    return FROSTBIND;
}
void generate_spellbooks() {
    if(game.has_amulet || game.floor == 4 || !BALANCE_VALUE("magic", 1)) return;
    uint16_t seed = static_cast<uint16_t>(game.run_seed ^ ((game.floor + 1u) * 0x6badu));
    for(GroundItem& ground : game.ground) {
        uint8_t type = ground.item.type;
        if(!type || is_artifact(type) || type == YENDOR_AMULET || type == FOOD || is_ammo(type) || is_light(type)) continue;
        if(next_random(seed) % 100 >= BALANCE_VALUE("spellbook-percent", 6)) continue;
        bool expanded=BALANCE_VALUE("expanded-spells",1);
        uint8_t simple = book_from_pool(next_random(seed),true,expanded);
        uint8_t advanced = book_from_pool(next_random(seed),false,expanded);
        uint8_t depth = game.floor + BALANCE_VALUE("spellbook-depth-bias", 3);
        uint8_t spell = next_random(seed) % 16 >= depth ? simple : advanced;
        ground.item = {SPELLBOOK, spell};
    }
}
}
