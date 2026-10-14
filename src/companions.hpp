#pragma once
#include "model.hpp"

namespace rogue {
enum class PetSource : uint8_t { Spell, Scroll, Potion, Wand, Altar };
enum class TamingMethod : uint8_t { Scroll, Potion, Wand };
uint8_t free_pet_slot();
uint8_t pet_count();
bool can_create_pet(uint8_t type, bool announce = true);
bool create_pet(uint8_t type, PetSource source);
// Complete immediately after successful can_create_pet(), before any actor or
// position change. The casting pipeline already validates before charging MP.
void create_prepared_pet(uint8_t type, PetSource source);
bool taming_target(uint8_t index);
uint8_t taming_chance(const Monster&, TamingMethod);
bool tame_monster(uint8_t index, TamingMethod method);
bool dismiss_pet(uint8_t index); // Caller owns confirmation; one ordinary action.
void place_floor_pets();
void delay_pet_recall(uint8_t index);
void age_pet_leashes();
void recover_pets();
// Decisions return ordinary movement (3), melee (16+slot), or magic (32+slot).
// Combat runs after the decision frame unwinds, preserving the AVM stack limit.
uint8_t pet_action(uint8_t index);
void begin_pet_turn(uint8_t index);
void end_pet_turn();
uint8_t blocking_pet(uint8_t index);
void creature_attack(uint8_t attacker, uint8_t target, bool magical = false);
uint8_t pet_fire_target(uint8_t attacker);
void creature_fire(uint8_t attacker, uint8_t target);
}
