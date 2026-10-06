#include "simulator.hpp"
#include "trace.hpp"
#include "world.hpp"
#include <cstring>
#include <ostream>
#include <stdexcept>

namespace sim {
using namespace rogue;
namespace {
bool cardinal(const Action& a) { return (a.dx == 0 && (a.dy == -1 || a.dy == 1)) ||
    (a.dy == 0 && (a.dx == -1 || a.dx == 1)); }
bool ground_here(uint8_t index) { return index < GROUND_ITEMS && game.ground[index].item.type &&
    game.ground[index].pos == game.player; }
void hash_action(RunMetrics& r, const Action& a) {
    for(uint8_t b : {uint8_t(a.kind),uint8_t(a.dx),uint8_t(a.dy),a.slot,a.target,uint8_t(a.discard)}) {
        r.action_hash ^= b; r.action_hash *= 1099511628211ull;
    }
}
RunMetrics execute(Collector& c, Agent& agent, const Options& options) {
    auto& r = c.data;
    unsigned rejected = 0, identical = 0, path_failures = 0, swap_cycles = 0;
    uint8_t last_incoming = NO_ITEM, last_outgoing = NO_ITEM;
    Position last_swap{NONE,NONE};
    auto stuck = [&](const char* why) { r.stuck = true; r.result = "SIM_STUCK"; r.reason = why; };
    while(!session.ended && !r.stuck) {
        if(r.actions >= options.max_actions) { stuck("excessive action count"); break; }
        c.observe();
        Game before = game;
        uint16_t rng = game.random_state;
        Action a;
        try { a = agent.choose_action({game,r.actions,r.turns}); }
        catch(const std::exception& error) {
            r.stuck = true; r.result = "SIM_ERROR";
            r.reason = std::string("agent error: ")+error.what(); break;
        }
        if(game.random_state != rng || std::memcmp(&before,&game,sizeof game) != 0) {
            r.stuck = true; r.result = "SIM_ERROR"; r.reason = "agent mutated production state or RNG"; break;
        }
        if(options.trace) trace_action(*options.trace,r.actions,r.turns,a);
        ++r.actions;
        if(!r.floors.empty()) ++r.floors.back().actions;
        hash_action(r,a);
        c.prepare_action(a);
        bool accepted = dispatch(a);
        if(options.trace && !accepted) *options.trace << "  action rejected\n";
        bool swapped = accepted && (a.kind == ActionKind::Swap || a.kind == ActionKind::Take) &&
            a.slot < INVENTORY && a.target < GROUND_ITEMS &&
            before.ground[a.target].item.type && game.ground[a.target].item.type;
        if(swapped) {
            uint8_t incoming = before.ground[a.target].item.type, outgoing = before.inventory[a.slot].type;
            swap_cycles = before.player == last_swap && incoming == last_outgoing && outgoing == last_incoming ? swap_cycles+1 : 0;
            last_incoming = incoming; last_outgoing = outgoing; last_swap = before.player;
        } else swap_cycles = 0;
        rejected = accepted ? 0 : rejected+1;
        identical = std::memcmp(&before,&game,sizeof game) == 0 ? identical+1 : 0;
        path_failures = a.diagnostic == Diagnostic::NoPath ? path_failures+1 : 0;
        if(!session.ended) {
            if(swap_cycles >= 8) stuck("inventory-policy failure: repeated reverse swaps");
            else if(rejected >= options.max_rejected) stuck("action rejected repeatedly");
            else if(identical >= options.max_identical) stuck("repeated identical state");
            else if(path_failures >= options.max_path_failures) stuck("unable to find path");
        }
    }
    c.observe(); c.close_floor(false);
    if(session.ended) {
        r.result = session.result == ESCAPED ? "escaped" : session.result == DEATH ? "death" : "abandoned";
        if(session.result == ESCAPED) c.close_floor(true);
    }
    r.effective_seed = game.run_seed; r.score = game.score; r.final_floor = game.floor;
    r.hp = game.hp; r.max_hp = player_max_hp(); r.level = game.level; r.has_yendor = game.has_amulet;
    if(r.result != "death") r.death_cause.clear();
    else if(r.death_cause.empty()) r.death_cause = "other";
    if(c.enabled) {
        for(Item i : game.inventory) if(i.type)
            r.items[i.type].carried += is_stackable(i.type) ? item_value(i) : 1;
        r.ranged.carried = r.items[ARROWS].carried;
        if(game.has_amulet) r.items[YENDOR_AMULET].carried = 1;
    }
    if(options.trace) { *options.trace << "result=" << r.result << " actions=" << r.actions << " turns=" << r.turns
        << " score=" << r.score << " cause=" << r.death_cause << " reason=" << r.reason << " action_hash=" << r.action_hash << '\n'; }
    return r;
}
}
bool dispatch(const Action& a) {
    if(session.ended) return false;
    switch(a.kind) {
    case ActionKind::Move: {
        if(!cardinal(a)) return false;
        Game before = game;
        move_player(a.dx,a.dy);
        return std::memcmp(&before,&game,sizeof game) != 0;
    }
    case ActionKind::Wait: end_turn(); return true;
    case ActionKind::Take: {
        if(game.paralyzed || !ground_here(a.target)) return false;
        PickupResult result = take_item(a.target);
        if(result == PICKUP_NEEDS_SWAP && a.slot < INVENTORY) return swap_ground_item(a.target,a.slot);
        return result == PICKUP_TAKEN;
    }
    case ActionKind::Swap: return !game.paralyzed && ground_here(a.target) && swap_ground_item(a.target,a.slot);
    case ActionKind::Stairs: return take_stairs();
    case ActionKind::Use: return use_inventory(a.slot,a.target);
    case ActionKind::Throw: return cardinal(a) && throw_or_shoot(a.slot,a.dx,a.dy);
    case ActionKind::Wand: return use_wand(a.slot,a.dx,a.dy);
    case ActionKind::Drop: return drop_inventory(a.slot,a.discard);
    }
    return false;
}
RunMetrics run(uint16_t seed, Agent& agent, const Options& options) {
    Collector c; c.data.seed = seed; c.data.agent = agent.name(); c.enabled = options.telemetry; c.trace = options.trace;
    c.experiment=options.experiment; c.data.experiment=options.experiment_id; c.data.variant=options.variant;
    CollectScope sink(c);
    // start_new preserves best_score; isolate that cross-run frontend history.
    game = {}; session = {NONE,DEATH,false}; agent.reset();
    if(options.trace) *options.trace << "seed=" << seed << " agent=" << agent.name() << '\n';
    start_new(seed);
    return execute(c,agent,options);
}
RunMetrics run_started(uint16_t seed, Agent& agent, const Options& options) {
    Collector c; c.data.seed = seed; c.data.agent = agent.name(); c.enabled = options.telemetry; c.trace = options.trace;
    c.experiment=options.experiment; c.data.experiment=options.experiment_id; c.data.variant=options.variant;
    CollectScope sink(c); agent.reset(); c.enter_floor();
    return execute(c,agent,options);
}
}
