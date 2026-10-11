#pragma once
#include "explorer.hpp"
namespace sim {
// Native frontend message adapter, entirely separate from raw telemetry.
struct ExplorerMessages {
    SeenPosition attempted, origin;
    bool melee_attempt=false, contact_reported=false, attacked=false;
    uint8_t contact_type=0, attacker_type=0, last_named=0, pending=0;
    MeleeFeedback feedback=MeleeFeedback::None;
    uint8_t incoming_attacks=0, anonymous_attacks=0;
    bool encounter_changed=false, summoning_reported=false;
    void prepare(const ExplorerAction& action,SeenPosition player,bool confused);
    void text(const char*);
    void monster(uint8_t type);
};
class ExplorerMessageScope {
    ExplorerMessages* previous;
public:
    explicit ExplorerMessageScope(ExplorerMessages*);
    ~ExplorerMessageScope();
};
void explorer_status_text(const char*);
void explorer_status_monster(uint8_t type);
void explorer_message_observation(ExplorerObservation&);
}
