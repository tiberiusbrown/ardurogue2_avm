#pragma once

// No types, calls, argument evaluation or storage survive ordinary builds.
#if defined(ARDUROGUE2_SIM)
#include <stdint.h>
namespace sim {
enum class EventKind {
    ManaRegenerated, StudyStarted, StudyTurn, StudyInterrupted, SpellLearned, SpellCast, SpellDisplacement,
    ItemIdentified, InventoryFull, SeeInvisibleExpired, PlayerEffectExpired, FuelExhausted, FloorEntered, FloorExited, Turn, Finished, PlayerDamage,
    PlayerAttack, MonsterDamage, MonsterKilled, MonsterAttack, MonsterHit,
    Special, GeneratedItem, Pickup, Dropped, Discarded, Equipped,
    ItemUsed, Consumed, PotionThrown, ChargeUsed, MonsterChanged, ArrowFired, ArrowTarget,
    FeatureDiscovered, FeatureActivated, FeatureDisplacement, FeatureDeclined, FeatureSpawn, HolyDamage, MonsterTrapActivated, MonsterTrapTeleport, MonsterTrapStatus, MonsterAware, MonsterSummoned
};
enum class Cause : uint8_t { Other, Monster, Starvation, Item, Fire, Trap, Feature, MonsterTrap, Spell };
enum class Special : uint8_t { Poison, Confusion, Paralysis, Fire };
void event(EventKind, uint8_t index = 255, uint8_t type = 0,
           uint16_t amount = 1, uint8_t detail = 0);
void after_floor_generation();
uint8_t balance_value(const char*, uint8_t);
bool artifacts_enabled();
bool full_light();
bool light_awareness();
bool features_enabled();
bool monster_traps_enabled();
enum class FeatureVariant { Placement, Early, Holy, Density, Damage, Roster };
bool original_features(FeatureVariant);
// Scoped attribution is exclusively host simulator state, never saved state.
class DamageScope {
    Cause previous_cause;
    uint8_t previous_type;
public:
    DamageScope(Cause, uint8_t type = 0);
    ~DamageScope();
};
}
#define SIM_EVENT(...) ::sim::event(__VA_ARGS__)
#define BALANCE_VALUE(name, fallback) ::sim::balance_value(name, fallback)
#else
#define SIM_EVENT(...) ((void)0)
#define BALANCE_VALUE(name, fallback) (fallback)
#endif
