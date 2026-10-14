#pragma once
#include "model.hpp"

namespace rogue {
// Shared production tuning; host experiments may request the original settings.
uint8_t trap_tuning_depth();
constexpr uint8_t OPENING_SPIKE_MIN = 2, OPENING_SPIKE_ROLL = 3;
constexpr uint8_t OPENING_DART_ROLL = 2;
constexpr uint8_t OPENING_CONFUSION_MIN = 3, OPENING_CONFUSION_ROLL = 3;
constexpr uint8_t MIDDLE_FLAME_MIN = 3, MIDDLE_FLAME_ROLL = 3;
constexpr uint8_t MIDDLE_FROST_MIN = 2, MIDDLE_FROST_ROLL = 3;
constexpr uint8_t MIDDLE_SNARE_MIN = 1;
uint8_t feature_at(Position pos);
// Turn-local row rejection; no features are created during monster actions.
void begin_feature_lookup();
void end_feature_lookup();
// Filtered observations also erase position, preventing hidden information leaks.
constexpr MapFeature observed_feature(const MapFeature& feature) {
    return feature_is_hidden(feature.id) ? MapFeature{} : feature;
}
uint8_t observed_feature_at(const Game& source, Position pos);
void reveal_feature(uint8_t index);
void remove_feature(uint8_t index);
void activate_entry_feature(uint8_t index, bool forced = false);
// Tile entry only; spawning and teleporting never call this resolver.
void activate_monster_trap(uint8_t monster_index);
void resolve_forced_entry();
void search_traps();
void wait_search();
struct FeatureChoice { bool accept = false; uint8_t slot = NONE; };
bool feature_accepts_item(uint8_t type, uint8_t slot);
bool resolve_feature(uint8_t index, FeatureChoice choice);
// Frontends collect choices only after the combat movement frame has unwound.
void begin_player_move(int8_t dx, int8_t dy);
void complete_ground_pickups();
void finish_pickup_turn();
void complete_player_move(FeatureChoice choice = {});
} // namespace rogue
