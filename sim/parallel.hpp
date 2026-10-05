#pragma once
#include "simulator.hpp"
#include <cstdint>
#include <ostream>
#include <string>

namespace sim {
struct BatchCounts { uint64_t escaped=0, deaths=0, stuck=0; };
// Separate processes are required: production Game/session are globals.
// Contiguous chunks merge in seed order, independently of completion order.
BatchCounts parallel_batch(const std::string& executable, uint64_t start,
    uint64_t count, unsigned jobs, const Options&, std::ostream& runs,
    std::ostream* floors, std::ostream* items, std::ostream* monsters);
}
