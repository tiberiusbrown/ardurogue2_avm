#pragma once
#include "agent.hpp"
#include "metrics.hpp"
#include <iosfwd>

namespace sim {
struct Options {
    uint64_t max_actions = 20000;
    unsigned max_rejected = 32, max_identical = 32, max_path_failures = 32;
    bool telemetry = true;
    bool entry_state = false;
    std::ostream* trace = nullptr;
    std::shared_ptr<const Experiment> experiment;
    std::vector<std::string> intervention_rules;
    std::string experiment_id="baseline", variant="control";
};
bool dispatch(const Action&);
RunMetrics run(uint16_t seed, Agent&, const Options& = {});
// Also used by controlled-state tests, without changing the normal start path.
RunMetrics run_started(uint16_t seed, Agent&, const Options& = {});
}
