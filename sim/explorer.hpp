#pragma once
// Deliberately independent of all production headers, Game and global state.
#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include "explorer_parameters.hpp"

namespace sim {
inline constexpr unsigned EXPLORER_OBSERVATION_VERSION = 5;
inline constexpr int EXPLORER_APPEARANCES = 18;
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
enum class MeleeFeedback : uint8_t { None, Miss, Hit, Defeat };
struct ExplorerObservation {
    SeenPosition player;
    uint8_t floor=0, hp=0, max_hp=0, level=0, xp=0, strength=0, dexterity=0, speed=0, hunger=0;
    uint8_t weakened=0, confused=0, paralyzed=0, slowed=0, invisible=0, blindness=0; // Known flags, not hidden timers.
    uint8_t radius=0, inventory_capacity=20;
    bool vitality_drained=false;
    bool haste=false, levitation=false, fire_resistance=false;
    std::bitset<EXPLORER_CELLS> detected_objects; // Positions only, one public map snapshot.
    bool see_invisible=false; // Public start/expiration feedback, never a remaining timer.
    bool yendor=false, pending_pickups=false;
    uint64_t actions=0, turns=0;
    std::array<uint8_t,6> equipped{{255,255,255,255,255,255}}; // weapon, armor, offhand, amulet, rings
    std::array<SeenItem,20> inventory{};
    std::array<Terrain,EXPLORER_CELLS> terrain{}; // Only current sight or explicit mechanical reveals.
    std::bitset<EXPLORER_CELLS> visible;
    std::array<uint8_t,EXPLORER_CELLS> features{};
    std::array<SeenGround,36> ground{}; // Includes indistinguishable mimic sprites.
    std::array<SeenMonster,12> monsters{};
    uint8_t ground_count=0, monster_count=0;
    std::array<std::array<uint8_t,EXPLORER_APPEARANCES>,6> identities{}; // Only identified appearance mappings.
    bool attacked=false, last_rejected=false; // Public feedback, never raw attacker coordinates.
    bool incoming_attack=false;
    uint8_t hp_loss=0; // Difference between consecutive displayed HP values, not damage telemetry.
    uint8_t reported_attacker=0, contact_type=0;
    SeenMonster reported_species; // Public definition from printed name; position stays unknown.
    SeenPosition contact; // Our own attempted melee square, only after hit/miss feedback.
    bool contact_reported=false;
    SeenPosition contact_origin; // Player at attack time, not after enemy turns.
    MeleeFeedback melee_feedback=MeleeFeedback::None;
    uint8_t incoming_attacks=0, anonymous_attacks=0; // Printed attacks, no bearings.
    bool encounter_changed=false, summoning_reported=false; // Public status prose.
};
enum class ExplorerActionKind { Move, Wait, Take, Stairs, Use, Throw, Wand, Drop };
struct ExplorerAction {
    ExplorerActionKind kind=ExplorerActionKind::Wait;
    int8_t dx=0, dy=0;
    uint8_t slot=255, target=255;
    bool discard=false, no_path=false;
    std::string goal;
    SeenPosition destination;
    bool essential_hazard=false, navigation=false;
};
struct ExplorerStats {
    uint64_t revealed=0, exploring_actions=0, repeats=0, frontier_changes=0, unreachable=0, missing_items=0;
    uint64_t safe_detours=0, hazard_abandoned=0, essential_crossings=0, no_safe_route=0,
        known_crossings=0, repeated_crossings=0, reversals=0, goal_changes=0, goal_failures=0, cycles=0;
};
struct SeenFeatureChoice { bool accept=false; uint8_t slot=255; };
struct RememberedTile {
    Terrain terrain=Terrain::Unknown, historical=Terrain::Unknown;
    uint8_t feature=0, failures=0;
    uint16_t visits=0; // Host history must not saturate during a long blind patrol.
    SeenItem item;
    bool has_item=false,surveyed=false;
    uint8_t hazard_entries=0, hazard_loss=0, survey_attempts=0;
};
struct MonsterMemory { SeenPosition pos; uint8_t type=0; uint64_t seen=0; SeenMonster species; };
struct ExplorerFloor {
    std::array<RememberedTile,EXPLORER_CELLS> tiles{};
    std::array<MonsterMemory,12> enemies{};
    SeenPosition up, down;
    bool visited=false, ascent_layout=false;
};
enum class Danger : uint8_t { Quiet, Caution, Serious, Lethal };
struct SurvivalRisk {
    int one=0, two=0, attackers=0;
    bool fire=false, unseen=false, status=false, retreat=false;
    Danger danger=Danger::Quiet;
};
enum class ContactState : uint8_t { None, Suspected, Confirmed, Uncertain, Disengaging };
// One local hypothesis, derived only from observations and our own actions.
struct InvisibleContact {
    ContactState state=ContactState::None;
    SeenPosition square, origin, probe_origin;
    uint64_t confirmed=0, threat=0, started=0, progress=0, disengage_until=0;
    uint8_t tested=0, next_direction=0, recent_attacks=0, observed_hp=0;
    unsigned probes=0, confirmations=0, misses=0, healing=0, defeats=0, summons=0;
    unsigned damage=0, recent_damage=0;
    std::array<uint8_t,8> losses{};
    MeleeFeedback feedback=MeleeFeedback::None;
};
// These helpers accept only public observations and legitimate memory.
SurvivalRisk estimate_risk(const ExplorerObservation&,const ExplorerFloor&,const ExplorerParameters&);
int expected_information(const ExplorerFloor&,SeenPosition,int radius,bool obstruction=true);
class ExplorerAgent final {
    std::array<ExplorerFloor,16> floors{};
    std::array<std::array<uint8_t,EXPLORER_APPEARANCES>,6> identities{};
    uint8_t active_floor=255;
    uint64_t last_actions=UINT64_MAX, last_turns=UINT64_MAX, last_experiment=0,last_mass_scroll=0;
    SeenPosition last_position, frontier, last_retreat;
    SeenPosition shadow;
    uint64_t shadow_turn=0;
    SeenPosition blocked_contact;
    SeenPosition blocked_origin;
    uint64_t blocked_contact_until=0;
    unsigned probe_direction=0;
    unsigned retreat_budget=0, quiet_turns=0, stalled=0, bow_cooldown=0;
    uint8_t pending_ring=255;
    uint8_t previous_hp=0;
    unsigned unseen_turns=0, contact_damage=0;
    uint64_t last_offhand_switch=0, last_control=0;
    SeenPosition controlled_position;
    ExplorerAction previous;
    InvisibleContact invisible_contact;
    bool previous_healing=false;
    // Navigation commitment is separate from invisible-contact inference.
    SeenPosition objective, deferred_objective, prior_position, recent_hazard;
    std::array<SeenPosition,8> recent_positions{};
    uint64_t deferred_until=0, hazard_until=0;
    unsigned navigation_moves=0, objective_failures=0, frontier_stale=0;
    void remember_contact(const ExplorerObservation&);
    std::array<int32_t,EXPLORER_CELLS> costs{};
    std::array<int16_t,EXPLORER_CELLS> steps{};
    std::array<int8_t,EXPLORER_CELLS> first{};
    std::array<uint8_t,EXPLORER_CELLS> route_hazards{};
    void remember(const ExplorerObservation&);
    void paths(const ExplorerObservation&,bool hazards=false);
    ExplorerAction navigate(SeenPosition,const char*);
    ExplorerAction hazardous_progress(const ExplorerObservation&,SeenPosition);
    ExplorerAction pickup(const ExplorerObservation&);
public:
    SurvivalRisk risk;
    ExplorerParameters parameters;
    explicit ExplorerAgent(ExplorerParameters p={}) : parameters(p) {}
    static const char* name() { return "explorer-v1"; }
    ExplorerStats stats;
    void reset();
    ExplorerAction choose_action(const ExplorerObservation&);
    SeenFeatureChoice choose_feature(const ExplorerObservation&) const;
    const ExplorerFloor& memory(unsigned f) const { return floors.at(f); }
    const InvisibleContact& contact_memory() const { return invisible_contact; }
};
}
