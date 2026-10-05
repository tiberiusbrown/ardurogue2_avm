#pragma once
#include "game.hpp"
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
struct DecisionContext { const rogue::Game& world; uint64_t actions, turns; };
class Agent {
public:
    virtual ~Agent() = default;
    virtual const char* name() const = 0;
    virtual void reset() {}
    virtual Action choose_action(const DecisionContext&) = 0;
};

// Cardinal BFS: closed doors are traversable estimates; monsters are endpoints
// but cannot be traversed unless allow_monsters is requested as a fallback.
struct Paths {
    std::array<int16_t, rogue::MAP_W * rogue::MAP_H> distance;
    std::array<int8_t, rogue::MAP_W * rogue::MAP_H> first;
    explicit Paths(bool allow_monsters = false);
    int to(rogue::Position p) const;
    Action move_to(rogue::Position p, const std::string& goal) const;
};
class OmniscientAgent final : public Agent {
public:
    const char* name() const override { return "omniscient-v1"; }
    Action choose_action(const DecisionContext&) override;
};
std::string action_text(const Action&);
}
