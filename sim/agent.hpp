#pragma once
#include "game.hpp"
#include "features.hpp"
#include <array>
#include <string>

namespace sim {
enum class ActionKind { Move, Wait, Take, Swap, Stairs, Use, Throw, Wand, Drop };
enum class Diagnostic { None, NoPath };
struct Action {
    ActionKind kind = ActionKind::Wait;
    int8_t dx = 0, dy = 0;
    uint8_t slot = rogue::NONE, target = rogue::NONE;
    bool discard = false;
    Diagnostic diagnostic = Diagnostic::None;
    std::string goal;
    rogue::Position destination{rogue::NONE, rogue::NONE};
};
// Policy receives a read-only state, never the runner, metrics or trace sink.
// Future frontends may provide a restricted observation to other policies.
struct DecisionContext {
    rogue::Game world;
    uint64_t actions, turns;
    DecisionContext(const rogue::Game& source, uint64_t actions, uint64_t turns)
        : world(source), actions(actions), turns(turns) {
        for(auto& feature : world.features) feature = rogue::observed_feature(feature);
    }
};
class Agent {
public:
    virtual ~Agent() = default;
    virtual const char* name() const = 0;
    virtual void reset() {}
    virtual Action choose_action(const DecisionContext&) = 0;
    // A Wait response declines remaining pickups before the entry feature.
    virtual Action choose_pickup(const DecisionContext&) { return {}; }
};

// Cardinal BFS: closed doors are traversable estimates; monsters are endpoints
// but cannot be traversed unless allow_monsters is requested as a fallback.
struct Paths {
    std::array<int16_t, rogue::MAP_W * rogue::MAP_H> distance;
    std::array<int8_t, rogue::MAP_W * rogue::MAP_H> first;
    explicit Paths(bool allow_monsters = false, bool allow_teleports = false);
    int to(rogue::Position p) const;
    Action move_to(rogue::Position p, const std::string& goal) const;
};
class OmniscientAgent final : public Agent {
    rogue::Position retreat_origin{rogue::NONE, rogue::NONE};
    uint8_t retreat_floor = rogue::NONE;
    unsigned retreat_steps = 0;
    unsigned bow_idle = 0, bow_cooldown = 0;
    unsigned holy_entries = 0, feature_steps = 0;
    std::array<bool, rogue::MAP_FEATURES> feature_attempted{};
    uint8_t feature_floor = rogue::NONE;
    uint16_t retreat_threats = 0;
    rogue::Position navigation_goal{rogue::NONE, rogue::NONE};
    std::array<rogue::Position, 8> navigation_history{};
    unsigned navigation_cursor = 0, navigation_stale = 0;
    int navigation_best = 10000;
    std::array<bool, rogue::GROUND_ITEMS> abandoned_loot{};
    Action navigate(const Paths&, rogue::Position, const char*, bool optional = false);
public:
    // Maintained balance policy. The automatic build hash identifies its exact
    // implementation; establish a fresh reference after extending mechanics.
    const char* name() const override { return "omniscient-v2"; }
    void reset() override { retreat_origin = {rogue::NONE, rogue::NONE}; retreat_floor = rogue::NONE; retreat_steps = 0; retreat_threats = 0; bow_idle = bow_cooldown = holy_entries = 0; feature_floor = rogue::NONE; feature_steps = 0; feature_attempted.fill(false); navigation_goal = {rogue::NONE, rogue::NONE}; navigation_cursor = navigation_stale = 0; navigation_best = 10000; navigation_history.fill({rogue::NONE, rogue::NONE}); abandoned_loot.fill(false); }
    Action choose_action(const DecisionContext&) override;
    Action choose_pickup(const DecisionContext&) override;
};
rogue::FeatureChoice choose_feature_choice(uint8_t type);
inline std::array<rogue::MapFeature, rogue::MAP_FEATURES> observed_features(const rogue::Game& source) {
    std::array<rogue::MapFeature, rogue::MAP_FEATURES> result{};
    for(size_t i = 0; i < result.size(); ++i) result[i] = rogue::observed_feature(source.features[i]);
    return result;
}
std::string action_text(const Action&);
}
