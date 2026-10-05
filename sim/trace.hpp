#pragma once
#include "agent.hpp"
#include "sim_hooks.hpp"
#include <iosfwd>
namespace sim {
void trace_action(std::ostream&, uint64_t action, uint64_t turn, const Action&);
void trace_event(std::ostream&, EventKind, uint8_t index, uint8_t type, uint16_t amount);
}
