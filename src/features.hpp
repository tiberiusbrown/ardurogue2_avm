#pragma once
#include "model.hpp"

namespace rogue {
uint8_t feature_at(Position pos);
// Filtered observations also erase position, preventing hidden information leaks.
constexpr MapFeature observed_feature(const MapFeature& feature) {
    return feature_is_hidden(feature.id) ? MapFeature{} : feature;
}
uint8_t observed_feature_at(const Game& source, Position pos);
void reveal_feature(uint8_t index);
void remove_feature(uint8_t index);
void activate_entry_feature(uint8_t index, bool forced = false);
void resolve_forced_entry();
void search_traps();
void wait_search();
struct FeatureChoice { bool accept = false; uint8_t slot = NONE; };
bool feature_accepts_item(uint8_t type, uint8_t slot);
bool resolve_feature(uint8_t index, FeatureChoice choice);
// Frontends collect choices only after the combat movement frame has unwound.
void begin_player_move(int8_t dx, int8_t dy);
void complete_player_move(FeatureChoice choice = {});
} // namespace rogue
