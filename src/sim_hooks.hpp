#pragma once

// No types, calls, argument evaluation or storage survive ordinary builds.
#if defined(ARDUROGUE2_SIM)
#include <stdint.h>
namespace sim {
enum class EventKind {
    FloorEntered, FloorExited, Turn, Finished, PlayerDamage,
    PlayerAttack, MonsterDamage, MonsterKilled, MonsterAttack, MonsterHit,
    Special, GeneratedItem, Pickup, Dropped, Discarded, Equipped,
    ItemUsed, Consumed, PotionThrown, ChargeUsed, MonsterChanged, ArrowFired, ArrowTarget
};
enum class Cause : uint8_t { Other, Monster, Starvation, Item, Fire };
enum class Special : uint8_t { Poison, Confusion, Paralysis, Fire };
void event(EventKind, uint8_t index = 255, uint8_t type = 0,
           uint16_t amount = 1, uint8_t detail = 0);
void after_floor_generation();
bool artifacts_enabled();
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
#else
#define SIM_EVENT(...) ((void)0)
#endif
