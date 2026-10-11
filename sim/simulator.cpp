#include "simulator.hpp"
#include "trace.hpp"
#include "world.hpp"
#include "explorer_observation.hpp"
#include "explorer_events.hpp"
#include <cstring>
#include <ostream>
#include <stdexcept>

namespace sim {
using namespace rogue;
bool dispatch_explorer(const Action&,ExplorerAgent&);
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
Action decide(Agent& agent,const ExplorerObservation&,uint64_t actions,uint64_t turns) {
    return session.pending_pickups ? agent.choose_pickup({game,actions,turns}) : agent.choose_action({game,actions,turns});
}
Action decide(ExplorerAgent& agent,const ExplorerObservation& o,uint64_t,uint64_t) {
    auto e=agent.choose_action(o); Action a;
    switch(e.kind) {
    case ExplorerActionKind::Move:a.kind=ActionKind::Move;break;
    case ExplorerActionKind::Wait:a.kind=ActionKind::Wait;break;
    case ExplorerActionKind::Take:a.kind=ActionKind::Take;break;
    case ExplorerActionKind::Stairs:a.kind=ActionKind::Stairs;break;
    case ExplorerActionKind::Use:a.kind=ActionKind::Use;break;
    case ExplorerActionKind::Throw:a.kind=ActionKind::Throw;break;
    case ExplorerActionKind::Wand:a.kind=ActionKind::Wand;break;
    case ExplorerActionKind::Drop:a.kind=ActionKind::Drop;break;
    }
    a.dx=e.dx; a.dy=e.dy; a.slot=e.slot; a.target=e.target; a.discard=e.discard;
    a.goal=std::move(e.goal); a.destination={e.destination.x,e.destination.y};
    a.diagnostic=e.no_path ? Diagnostic::NoPath : Diagnostic::None; return a;
}
void policy_metrics(Collector& c,Agent&) { c.decision_risk={}; }
void policy_metrics(Collector& c,ExplorerAgent& a) { c.explorer_stats=a.stats; c.decision_risk=a.risk; }
FeatureChoice feature_choice(Agent&,uint64_t,uint64_t) { return choose_feature_choice(feature_type(observed_feature_at(game,game.player))); }
FeatureChoice feature_choice(ExplorerAgent& a,uint64_t actions,uint64_t turns) {
    auto s=a.choose_feature(observe_explorer(actions,turns)); FeatureChoice choice; choice.accept=s.accept; choice.slot=s.slot; return choice;
}
bool dispatch_policy(const Action& a,Agent&) { return dispatch(a); }
bool dispatch_policy(const Action& a,ExplorerAgent& agent) { return dispatch_explorer(a,agent); }
template<class Policy>
RunMetrics execute(Collector& c, Policy& agent, const Options& options) {
    auto& r = c.data;
    unsigned rejected = 0, identical = 0, path_failures = 0, swap_cycles = 0;
    uint8_t last_incoming = NO_ITEM, last_outgoing = NO_ITEM;
    Position last_swap{NONE,NONE};
    ExplorerObserver observer;
    ExplorerMessages messages;
    ExplorerMessageScope message_scope(std::string(agent.name())=="explorer-v1" ? &messages : nullptr);
    bool last_rejected=false;
    auto stuck = [&](const char* why) { r.stuck = true; r.result = "SIM_STUCK"; r.reason = why; };
    while(!session.ended && !r.stuck) {
        if(r.actions >= options.max_actions) { stuck("excessive action count"); break; }
        c.observe();
        Game before = game;
        uint16_t rng = game.random_state;
        Action a;
        try {
            auto observation=observer.observe(r.actions,r.turns);
            observation.last_rejected=last_rejected;
            c.observe_exploration(observation);
            a=decide(agent,observation,r.actions,r.turns);
            policy_metrics(c,agent);
            if(options.trace && r.agent=="explorer-v1") trace_explorer(*options.trace,observation,c.decision_risk);
        }
        catch(const std::exception& error) {
            r.stuck = true; r.result = "SIM_ERROR";
            r.reason = std::string("agent error: ")+error.what(); break;
        }
        if(game.random_state != rng || std::memcmp(&before,&game,sizeof game) != 0) {
            r.stuck = true; r.result = "SIM_ERROR"; r.reason = "agent mutated production state or RNG"; break;
        }
        if(session.pending_pickups && a.kind == ActionKind::Wait) {
            complete_ground_pickups();
            complete_player_move(feature_choice(agent,r.actions,r.turns));
            continue;
        }
        if(options.trace) trace_action(*options.trace,r.actions,r.turns,a);
        ++r.actions;
        if(!r.floors.empty()) ++r.floors.back().actions;
        hash_action(r,a);
        c.prepare_action(a);
        ExplorerAction message_action;
        if(a.kind==ActionKind::Move) message_action.kind=ExplorerActionKind::Move;
        message_action.dx=a.dx; message_action.dy=a.dy;
        messages.prepare(message_action,{before.player.x,before.player.y},before.confused!=0);
        bool accepted = dispatch_policy(a,agent);
        last_rejected=!accepted;
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
        if(r.stuck && std::string(agent.name())=="explorer-v1") r.reason += std::string("; goal=")+a.goal+"; position="+std::to_string(before.player.x)+":"+std::to_string(before.player.y);
    }
    c.observe(); c.close_floor(false);
    if(session.ended) {
        r.result = session.result == ESCAPED ? "escaped" : session.result == DEATH ? "death" : "abandoned";
        if(session.result == ESCAPED) c.close_floor(true);
    }
    r.effective_seed = game.run_seed; r.score = game.score; r.final_floor = game.floor;
    r.xp = game.xp; r.final_hunger = game.hunger;
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
        begin_player_move(a.dx,a.dy);
        if(session.pending_move && !session.pending_pickups) {
            uint8_t id = observed_feature_at(game, game.player);
            complete_player_move(choose_feature_choice(feature_type(id)));
        }
        return std::memcmp(&before,&game,sizeof game) != 0;
    }
    case ActionKind::Wait: wait_search(); return true;
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
bool dispatch_explorer(const Action& a,ExplorerAgent& agent) {
    if(a.kind!=ActionKind::Move) return dispatch(a);
    if(session.ended || !cardinal(a)) return false;
    Game before=game;
    begin_player_move(a.dx,a.dy);
    if(session.pending_move && !session.pending_pickups) complete_player_move(feature_choice(agent,0,0));
    return std::memcmp(&before,&game,sizeof game)!=0;
}
RunMetrics run(uint16_t seed, ExplorerAgent& agent, const Options& options) {
    Collector c; c.data.seed=seed; c.data.agent=agent.name(); c.enabled=options.telemetry; c.trace=options.trace;
    c.experiment=options.experiment; c.data.experiment=options.experiment_id; c.data.variant=options.variant;
    CollectScope sink(c); game={}; session={NONE,DEATH,false}; agent.reset();
    if(options.trace) *options.trace<<"seed="<<seed<<" agent="<<agent.name()<<'\n';
    start_new(seed); return execute(c,agent,options);
}
RunMetrics run_started(uint16_t seed, ExplorerAgent& agent, const Options& options) {
    Collector c; c.data.seed=seed; c.data.agent=agent.name(); c.enabled=options.telemetry; c.trace=options.trace;
    c.experiment=options.experiment; c.data.experiment=options.experiment_id; c.data.variant=options.variant;
    CollectScope sink(c); agent.reset(); c.enter_floor(); return execute(c,agent,options);
}
}
