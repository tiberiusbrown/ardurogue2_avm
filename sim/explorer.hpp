#pragma once
// Deliberately independent of all production headers, Game and global state.
#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include "explorer_parameters.hpp"

namespace sim {
inline constexpr unsigned EXPLORER_OBSERVATION_VERSION = 1;
inline constexpr int EXPLORER_W = 64, EXPLORER_H = 32, EXPLORER_CELLS = 2048;
struct SeenPosition {
    uint8_t x=255, y=255;
    bool operator==(SeenPosition b) const { return x==b.x && y==b.y; }
    bool operator!=(SeenPosition b) const { return !(*this==b); }
};
enum class Terrain : uint8_t { Unknown, Floor, Wall, ClosedDoor, OpenDoor, Up, Down };
enum class ItemCategory : uint8_t { None, Ammo, Food, Potion, Weapon, Armor, Amulet, Ring, Scroll, Wand, Light, Shield };
struct SeenItem {
    ItemCategory category=ItemCategory::None;
    uint8_t type=0, appearance=255, quantity=0, magnitude=0, modifier=0;
    int8_t enchant=0;
    bool individual=false, cursed=false;
};
struct SeenGround { SeenPosition pos; SeenItem item; uint8_t handle=255; };
struct SeenMonster {
    SeenPosition pos;
    uint8_t type=0, strength=0, speed=0, armor=0;
    uint16_t traits=0; // Public species definitions, never live HP/status/AI.
};
struct ExplorerObservation {
    SeenPosition player;
    uint8_t floor=0, hp=0, max_hp=0, level=0, xp=0, strength=0, dexterity=0, speed=0, hunger=0;
    uint8_t weakened=0, confused=0, paralyzed=0, slowed=0, invisible=0, blindness=0; // Known flags, not hidden timers.
    uint8_t radius=0;
    bool yendor=false, pending_pickups=false;
    uint64_t actions=0, turns=0;
    std::array<uint8_t,6> equipped{{255,255,255,255,255,255}}; // weapon, armor, offhand, amulet, rings
    std::array<SeenItem,16> inventory{};
    std::array<Terrain,EXPLORER_CELLS> terrain{}; // Only current sight or explicit mechanical reveals.
    std::bitset<EXPLORER_CELLS> visible;
    std::array<uint8_t,EXPLORER_CELLS> features{};
    std::array<SeenGround,28> ground{}; // Includes indistinguishable mimic sprites.
    std::array<SeenMonster,12> monsters{};
    uint8_t ground_count=0, monster_count=0;
    std::array<std::array<uint8_t,16>,6> identities{}; // Only identified appearance mappings.
    bool attacked=false, last_rejected=false; // Public feedback, never raw attacker coordinates.
    uint8_t reported_attacker=0, contact_type=0;
    SeenPosition contact; // Our own attempted melee square, only after hit/miss feedback.
    bool contact_reported=false;
};
enum class ExplorerActionKind { Move, Wait, Take, Stairs, Use, Throw, Wand, Drop };
struct ExplorerAction {
    ExplorerActionKind kind=ExplorerActionKind::Wait;
    int8_t dx=0, dy=0;
    uint8_t slot=255, target=255;
    bool discard=false, no_path=false;
    std::string goal;
    SeenPosition destination;
};
struct ExplorerStats {
    uint64_t revealed=0, exploring_actions=0, repeats=0, frontier_changes=0, unreachable=0, missing_items=0;
};
struct SeenFeatureChoice { bool accept=false; uint8_t slot=255; };
struct RememberedTile {
    Terrain terrain=Terrain::Unknown, historical=Terrain::Unknown;
    uint8_t feature=0, visits=0, failures=0;
    SeenItem item;
    bool has_item=false,surveyed=false;
};
struct MonsterMemory { SeenPosition pos; uint8_t type=0; uint64_t seen=0; };
struct ExplorerFloor {
    std::array<RememberedTile,EXPLORER_CELLS> tiles{};
    std::array<MonsterMemory,12> enemies{};
    SeenPosition up, down;
    bool visited=false, ascent_layout=false;
};
class ExplorerAgent final {
    std::array<ExplorerFloor,16> floors{};
    std::array<std::array<uint8_t,16>,6> identities{};
    uint8_t active_floor=255;
    uint64_t last_actions=UINT64_MAX, last_turns=UINT64_MAX, last_experiment=0,last_mass_scroll=0;
    SeenPosition last_position, frontier, last_retreat;
    SeenPosition shadow;
    uint64_t shadow_turn=0;
    SeenPosition blocked_contact;
    uint64_t blocked_contact_until=0;
    unsigned probe_direction=0;
    unsigned retreat_budget=0, quiet_turns=0, stalled=0, bow_cooldown=0;
    uint8_t pending_ring=255;
    ExplorerAction previous;
    std::array<int16_t,EXPLORER_CELLS> costs{}, steps{};
    std::array<int8_t,EXPLORER_CELLS> first{};
    void remember(const ExplorerObservation&);
    void paths(const ExplorerObservation&);
    ExplorerAction navigate(SeenPosition,const char*);
    ExplorerAction pickup(const ExplorerObservation&);
public:
    ExplorerParameters parameters;
    explicit ExplorerAgent(ExplorerParameters p={}) : parameters(p) {}
    static const char* name() { return "explorer-v1"; }
    ExplorerStats stats;
    void reset();
    ExplorerAction choose_action(const ExplorerObservation&);
    SeenFeatureChoice choose_feature(const ExplorerObservation&) const;
    const ExplorerFloor& memory(unsigned f) const { return floors.at(f); }
};
}
