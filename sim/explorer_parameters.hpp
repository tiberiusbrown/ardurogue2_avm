#pragma once
#include <cstddef>
#include <string>
namespace sim {
// Integer preferences only. These never change production mechanics.
// name, default, minimum, maximum; also the canonical serialization order.
#define EXPLORER_PARAMETERS(X) \
 X(frontier_adjacent_weight,150,0,500) \
 X(frontier_gain_weight,8,0,100) \
 X(frontier_distance_cost,5,1,100) \
 X(frontier_visit_cost,9,0,300) \
 X(frontier_hold_bonus,68,0,1000) \
 X(stair_base_score,1234,0,5000) \
 X(stair_distance_cost,8,0,100) \
 X(stair_low_health_bonus,903,0,5000) \
 X(hunger_distance_cost,2,0,100) \
 X(loot_distance_offset,14,1,100) \
 X(loot_visit_cost,25,0,500) \
 X(loot_max_distance,75,1,128) \
 X(loot_max_visits,11,1,20) \
 X(ascent_food_detour_distance,10,1,24) \
 X(ascent_food_reserve_units,2,0,8) \
 X(ascent_food_seek_hunger,180,0,255) \
 X(trap_cost,270,0,300) \
 X(teleport_cost,167,1,2000) \
 X(summoning_cost,30,1,1000) \
 X(remembered_threat_cost,6,0,100) \
 X(monster_memory_turns,7,1,40) \
 X(healing_percent,41,10,90) \
 X(combat_healing_percent,38,10,95) \
 X(emergency_hp,3,1,40) \
 X(emergency_percent,6,5,80) \
 X(crowd_threshold,2,2,8) \
 X(mass_scroll_distance,3,1,6) \
 X(mass_scroll_cooldown,6,1,30) \
 X(dragon_escape_percent,52,10,90) \
 X(food_threshold,140,5,180) \
 X(food_stock_target,24,1,63) \
 X(potion_min_hp,11,11,60) \
 X(potion_health_percent,78,30,100) \
 X(experiment_quiet_turns,2,2,16) \
 X(experiment_interval,12,3,60) \
 X(known_wand_experiment_range,1,1,6) \
 X(unknown_wand_experiment_range,3,3,6) \
 X(wand_experiment_interval,6,1,30) \
 X(scroll_min_hp,3,1,30) \
 X(scroll_quiet_turns,3,1,12) \
 X(scroll_interval,6,1,40) \
 X(jewelry_quiet_turns,8,1,12) \
 X(jewelry_experiment_max_score,158,0,350) \
 X(jewelry_experiment_health_percent,54,20,100) \
 X(unknown_jewelry_score,16,0,200) \
 X(shield_contact_bonus,34,0,400) \
 X(shield_ambient_bonus,68,0,200) \
 X(shield_quiet_percent,100,0,100) \
 X(ascent_shield_quiet_percent,5,0,100) \
 X(ascent_light_bonus,200,0,800) \
 X(shield_contact_distance,2,1,4) \
 X(shield_threat_memory_turns,3,1,12) \
 X(light_low_bonus,11,0,250) \
 X(light_high_bonus,41,0,100) \
 X(light_radius_weight,11,1,80) \
 X(shield_rating_weight,73,1,80) \
 X(armor_rating_weight,29,1,80) \
 X(weapon_damage_weight,4,1,30) \
 X(weapon_accuracy_weight,4,0,30) \
 X(enchant_weight,5,0,30) \
 X(illumination_ring_score,194,1,300) \
 X(illumination_dark_bonus,80,0,400) \
 X(brilliance_score,226,1,350) \
 X(brilliance_dark_bonus,200,0,500) \
 X(see_invisible_score,40,1,300) \
 X(invisibility_ring_score,180,1,350) \
 X(clarity_amulet_score,79,1,350) \
 X(clarity_lord_bonus,500,0,600) \
 X(fire_immunity_ring_score,55,1,350) \
 X(fire_immunity_late_bonus,120,0,350) \
 X(sustenance_ring_score,100,1,350) \
 X(sustenance_ascent_bonus,100,0,350) \
 X(speed_amulet_score,255,1,350) \
 X(protection_ring_score,137,1,300) \
 X(unknown_potion_value,26,1,250) \
 X(unknown_scroll_value,102,1,250) \
 X(unknown_wand_value,62,1,250) \
 X(unknown_gear_value,255,1,300) \
 X(early_xp_floors,2,0,8) \
 X(early_xp_level,3,1,15) \
 X(early_xp_distance,2,1,12) \
 X(early_xp_health_percent,74,40,100) \
 X(retreat_steps,2,0,8) \
 X(retreat_connections,3,1,4) \
 X(retreat_reversal_cost,44,0,100) \
 X(retreat_separation_weight,7,1,50) \
 X(retreat_connectivity_weight,0,0,30) \
 X(retreat_reset_quiet_turns,12,4,40) \
 X(stationary_contact_turns,128,32,512)
struct ExplorerParameters {
#define DECLARE(name,value,low,high) int name=value;
    EXPLORER_PARAMETERS(DECLARE)
#undef DECLARE
};
struct ParameterDefinition { const char* name; int ExplorerParameters::* member; int minimum,maximum; };
inline constexpr ParameterDefinition parameter_definitions[]={
#define DESCRIBE(name,value,low,high) {#name,&ExplorerParameters::name,low,high},
    EXPLORER_PARAMETERS(DESCRIBE)
#undef DESCRIBE
};
inline constexpr size_t EXPLORER_PARAMETER_COUNT=sizeof(parameter_definitions)/sizeof(parameter_definitions[0]);
void validate_parameters(const ExplorerParameters&);
ExplorerParameters parse_parameters(const std::string& json);
ExplorerParameters load_parameters(const std::string& path);
std::string parameter_json(const ExplorerParameters&);
}
