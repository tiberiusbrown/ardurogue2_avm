#pragma once
#include "model.hpp"
#include "sim_hooks.hpp"
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
struct FloorMetrics {
    int floor=0, visit=0;
    bool ascent=false, exited=false;
    int entry_hp=0, exit_hp=0, entry_level=0, exit_level=0;
    uint64_t turns=0, actions=0, kills=0, damage_taken=0, damage_dealt=0,
        pickups=0, consumables=0;
};
struct RunMetrics {
    uint16_t seed=0, effective_seed=0;
    std::string agent, result, reason, death_cause;
    uint64_t actions=0, turns=0, action_hash=14695981039346656037ull;
    int score=0, deepest=0, final_floor=0, level=0, hp=0, max_hp=0,
        floors_entered=0, floors_exited=0;
    bool has_yendor=false, stuck=false;
    std::array<ItemMetrics, rogue::WAND_POLYMORPH+1> items{};
    std::array<MonsterMetrics, rogue::LORD+1> monsters{};
    std::vector<FloorMetrics> floors;
};
struct Collector {
    RunMetrics data;
    bool enabled=true;
    std::ostream* trace=nullptr;
    std::array<bool, rogue::GROUND_ITEMS> reached{};
    std::array<bool, rogue::MONSTERS> encountered{}, engaged{};
    // Host-only object identity survives ground swaps/drop/repick. This counts
    // distinct wands, separately from pickup transactions and charge uses.
    std::array<size_t, rogue::INVENTORY> inventory_wands{};
    std::array<size_t, rogue::GROUND_ITEMS> ground_wands{};
    std::vector<uint8_t> wand_flags{0};
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
// The production hooks have one non-owning, scoped sink. Execution is serial.
class CollectScope {
    Collector* previous;
public:
    explicit CollectScope(Collector&);
    ~CollectScope();
};
}
