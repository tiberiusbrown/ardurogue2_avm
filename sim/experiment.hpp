#pragma once
#include "model.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sim {
struct Intervention {
    int visit=0, floor=0;
    bool ascent=false;
    std::string operation, from, to;
    unsigned count=0;
};
struct ExperimentContext { uint16_t effective_seed; int visit; };
// Host-only extension point. Custom experiments must preserve gameplay RNG.
class Experiment {
public:
    virtual ~Experiment() = default;
    virtual void apply(rogue::Game&, ExperimentContext, std::vector<Intervention>&) const = 0;
};
struct Rule {
    bool monster=false, remove=false;
    uint8_t from=0, to=0;
    int floor=-1, direction=-1, maximum=65535, info=-1; // -1 default, -2 preserve
};
class RuleExperiment final : public Experiment {
public:
    std::vector<Rule> rules;
    void apply(rogue::Game&, ExperimentContext, std::vector<Intervention>&) const override;
};
Rule parse_rule(const std::string&);
void after_floor_generation();
}
