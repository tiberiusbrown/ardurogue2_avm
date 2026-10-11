#pragma once
#include "model.hpp"
#include "sim_hooks.hpp"
#include "experiment.hpp"
#include "explorer.hpp"
#include <array>
#include <string>
#include <vector>
#include <iosfwd>

namespace sim {
struct ItemMetrics {
    uint64_t generated=0, reached=0, picked_up=0, used=0, consumed=0,
        equipped=0, dropped=0, discarded=0, carried=0,
        charges_used=0, drunk=0, thrown=0, scrolls_read=0, turns_equipped=0,
        wands_picked_up=0, wands_activated=0;
};
struct MonsterMetrics {
    uint64_t generated=0, encountered=0, engaged=0, killed=0,
        player_attacks=0, damage_taken=0, attacks=0, hits=0,
        player_damage=0, deaths=0, poison=0, confusion=0, paralysis=0, fire=0;
};
struct ShotMetrics { uint64_t shots=0, hits=0, damage=0, kills=0; };
struct RangedMetrics {
    uint64_t bundles=0, generated=0, picked=0, fired=0, thrown=0, sacrificed=0, carried=0,
        bow_turns=0, switches_to=0, switches_away=0,
        first_generated=255, first_picked=255, generation_gap=0, acquisition_gap=0;
    std::array<ShotMetrics,2> bows{};
    std::array<ShotMetrics,7> distances{}; // 0 records an adjacent blocker.
    std::array<ShotMetrics,rogue::LORD+1> targets{};
};
struct FeatureMetrics {
    uint64_t generated=0, discovered=0, activated=0, declined=0, damage=0, deaths=0, spawned=0, holy_damage=0, repeated=0, max_entries=0, displacement=0;
    uint64_t monster_activated=0, monster_damage=0, monster_killed=0, monster_teleported=0, monster_status=0, monster_spawned=0;
};
struct InvisibleMetrics {
    uint64_t encounters=0, probes=0, probes_to_first=0, located=0, confirmations=0,
        hits=0, misses=0, max_consecutive=0, contact_turns=0, losses=0,
        premature_abandonments=0, damage=0, healing=0, disengagements=0, kills=0,
        over_12=0, over_32=0, over_64=0, over_128=0, summons=0,
        stationary_starvation=0, phantom_deaths=0, hazard_encounters=0,
        ordinary_damage=0, hazard_damage=0, ordinary_kills=0, hazard_kills=0;
};
struct ExplorationMetrics {
    InvisibleMetrics invisible;
    uint64_t terrain_cells=0, floor_cells=0, surveyed_floor_cells=0, exploring_turns=0, travel=0, repeated_visits=0,
        frontier_changes=0, unreachable=0, missing_items=0, unseen_damage=0, items_observed=0, displacements=0;
    int reachable=0, shortest=0;
    int64_t stair_discovery=-1, stair_reached=-1;
    std::array<uint64_t,7> monster_distances{};
    uint64_t combat_turns=0,caution_actions=0,serious_actions=0,lethal_actions=0,
        healing_then_death=0,food_waste=0,identified_potions=0;
    int remaining_food=0,remaining_healing=0,remaining_escape=0,remaining_control=0,unknown_potions=0;
    int last_one=0,last_two=0,recent_damage=0;
    bool safe_retreat=false;
    std::array<uint64_t,9> alert_distances{};
    std::array<uint64_t,7> alert_light{};
    int64_t first_contact=-1;
};
struct FloorMetrics {
    ExplorationMetrics exploration;
    std::array<FeatureMetrics, rogue::FEATURE_TYPES + 1> features{};
    int floor=0, visit=0;
    uint64_t moves=0;
    unsigned unholy_count=0, holy_candidates=0, holy_preferred=0;
    uint64_t arrow_bundles=0, arrow_generated=0, arrow_picked=0;
    bool bow_owned=false;
    bool ascent=false, exited=false;
    int entry_hp=0, exit_hp=0, entry_level=0, exit_level=0, exit_hunger=0;
    int entry_max_hp=0, entry_strength=0, entry_dexterity=0, entry_speed=0,
        entry_hunger=0, entry_armor_rating=0, entry_food_units=0, entry_healing_units=0,
        entry_weapon_type=0, entry_weapon_enchant=0, entry_armor_type=0, entry_armor_enchant=0;
    // Optional host report sidecar has a version independent of the main streams.
    int entry_control_units=0, entry_wand_charges=0, entry_offensive_charges=0,
        entry_emergency_charges=0;
    bool entry_ring_invisibility=false, entry_speed_amulet=false, entry_invisible=false;
    std::string archetype;
    int floor_tiles=0, major_features=0, corridors=0, loops=0, open_connections=0;
    std::array<uint8_t,16> families{};
    std::array<ItemMetrics, rogue::WAND_POLYMORPH+1> items{};
    std::array<MonsterMetrics, rogue::LORD+1> monsters{};
    uint64_t turns=0, actions=0, kills=0, damage_taken=0, damage_dealt=0,
        pickups=0, consumables=0;
};
struct RunMetrics {
    std::array<FeatureMetrics, rogue::FEATURE_TYPES + 1> features{};
    RangedMetrics ranged;
    std::array<uint64_t,7> light_turns{};
    uint64_t fuel_exhaustions=0, blind_turns=0, blind_deaths=0, shield_turns=0, offhand_light_turns=0;
    uint16_t seed=0, effective_seed=0;
    std::string agent, result, reason, death_cause;
    uint64_t actions=0, turns=0, action_hash=14695981039346656037ull;
    int xp=0, final_hunger=0, score=0, deepest=0, final_floor=0, level=0, hp=0, max_hp=0,
        floors_entered=0, floors_exited=0;
    bool has_yendor=false, stuck=false, lord_killed=false;
    std::array<ItemMetrics, rogue::WAND_POLYMORPH+1> items{};
    std::array<MonsterMetrics, rogue::LORD+1> monsters{};
    std::vector<FloorMetrics> floors;
    std::vector<Intervention> interventions;
    std::string experiment="baseline", variant="control";
};
struct Collector {
    bool invisible_engagement=false, invisible_hazard=false, invisible_located=false, invisible_disengaging=false;
    uint64_t invisible_started=0, invisible_evidence=0, invisible_probes=0, contact_streak=0, invisible_duration=0;
    uint64_t invisible_damage=0, invisible_kills=0, contact_last_action=UINT64_MAX;
    std::array<bool,rogue::MONSTERS> initially_unseen{};
    SeenPosition contact_square, contact_player;
    void begin_invisible();
    void close_invisible();
    void observe_contact(const ExplorerObservation&);
    void decide_contact(const ExplorerObservation&,const struct Action&,const SurvivalRisk&);
    ExplorerStats explorer_stats, entry_explorer_stats;
    std::bitset<EXPLORER_CELLS> observed_terrain, observed_floor, surveyed_floor, visited_tiles;
    std::array<bool,rogue::MONSTERS> visibly_observed{}, visible_before{};
    std::array<bool,rogue::MONSTERS> aware{};
    std::array<bool,rogue::GROUND_ITEMS> observed_items{};
    SeenPosition observed_position;
    bool exploring_action=false;
    bool combat_action=false,healing_action=false;
    SurvivalRisk decision_risk;
    std::array<uint64_t,8> damage_turns{},damage_amounts{};
    std::bitset<256> known_potions;
    uint8_t last_attack_monster=255;
    uint64_t exploration_entry_turn=0;
    void observe_exploration(const ExplorerObservation&);
    RunMetrics data;
    uint64_t generation_gap=0, acquisition_gap=0;
    uint8_t last_weapon=rogue::NO_ITEM;
    int closed_ranged_visit=0;
    bool enabled=true;
    std::ostream* trace=nullptr;
    std::array<bool, rogue::GROUND_ITEMS> reached{};
    std::array<unsigned, rogue::MAP_FEATURES> feature_entries{};
    std::array<bool, rogue::MONSTERS> encountered{}, engaged{};
    // Host-only object identity survives ground swaps/drop/repick. This counts
    // distinct wands, separately from pickup transactions and charge uses.
    std::array<size_t, rogue::INVENTORY> inventory_wands{};
    std::array<size_t, rogue::GROUND_ITEMS> ground_wands{};
    std::vector<uint8_t> wand_flags{0};
    std::shared_ptr<const Experiment> experiment;
    std::array<ItemMetrics, rogue::WAND_POLYMORPH+1> entry_items{};
    std::array<MonsterMetrics, rogue::LORD+1> entry_monsters{};
    uint8_t pickup_slot=rogue::NONE, drop_slot=rogue::NONE;
    size_t new_wand();
    void prepare_action(const struct Action&);
    void enter_floor();
    void close_floor(bool exited);
    void observe();
    void handle(EventKind, uint8_t index, uint8_t type, uint16_t amount, uint8_t detail);
};
const char* item_name(uint8_t);
const char* monster_name(uint8_t);
void write_runs_header(std::ostream&);
void write_floors_header(std::ostream&);
void write_items_header(std::ostream&);
void write_monsters_header(std::ostream&);
void write_run(std::ostream&, const RunMetrics&);
void write_floors(std::ostream&, const RunMetrics&);
void write_items(std::ostream&, const RunMetrics&);
void write_monsters(std::ostream&, const RunMetrics&);
void write_visit_items_header(std::ostream&);
void write_visit_monsters_header(std::ostream&);
void write_interventions_header(std::ostream&);
void write_visit_items(std::ostream&, const RunMetrics&);
void write_visit_monsters(std::ostream&, const RunMetrics&);
void write_interventions(std::ostream&, const RunMetrics&);
inline constexpr unsigned ENTRY_STATE_SCHEMA_VERSION=1;
void write_entry_state_header(std::ostream&);
void write_entry_state(std::ostream&, const RunMetrics&);
inline constexpr unsigned EXPLORATION_SCHEMA_VERSION=5;
void write_exploration_header(std::ostream&);
void write_exploration(std::ostream&,const RunMetrics&);
struct CsvStream {
    const char* name;
    void (*header)(std::ostream&);
    void (*rows)(std::ostream&, const RunMetrics&);
};
inline constexpr unsigned TELEMETRY_SCHEMA_VERSION=7;
inline constexpr size_t CSV_STREAM_COUNT=9;
void write_ranged_header(std::ostream&);
void write_ranged(std::ostream&, const RunMetrics&);
extern const std::array<CsvStream,CSV_STREAM_COUNT> csv_streams;
// The production hooks have one non-owning, scoped sink. Execution is serial.
class CollectScope {
    Collector* previous;
public:
    explicit CollectScope(Collector&);
    ~CollectScope();
};
}
