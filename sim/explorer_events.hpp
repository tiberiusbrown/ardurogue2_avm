#pragma once
#include "explorer.hpp"
namespace sim {
// Native frontend message adapter, entirely separate from raw telemetry.
struct ExplorerMessages {
    SeenPosition attempted;
    bool melee_attempt=false, contact_reported=false, attacked=false;
    uint8_t contact_type=0, attacker_type=0, last_named=0, pending=0;
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
