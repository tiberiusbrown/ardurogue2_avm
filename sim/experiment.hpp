#pragma once
#include "model.hpp"
#include "sim_hooks.hpp"
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
    virtual bool full_light() const { return false; }
    virtual bool light_awareness() const { return true; }
    virtual bool artifacts_enabled() const { return true; }
    virtual bool monster_traps_enabled() const { return true; }
    virtual bool features_enabled() const { return true; }
    virtual bool original_features(FeatureVariant) const { return false; }
};
struct Rule {
    bool monster=false, remove=false, ordinary_items=false, no_features=false;
    bool feature=false, no_monster_traps=false, full_light=false;
    bool light_awareness=false;
    bool legacy_awareness=false;
    int feature_mode=-1; // 0 teleport, 1 early damage, 2 interactive only, 3 traps only
    int original=-1; // FeatureVariant, host only
    uint8_t from=0, to=0;
    int floor=-1, direction=-1, maximum=65535, info=-1; // -1 default, -2 preserve
};
class RuleExperiment final : public Experiment {
public:
    std::vector<Rule> rules;
    void apply(rogue::Game&, ExperimentContext, std::vector<Intervention>&) const override;
    bool full_light() const override;
    bool light_awareness() const override;
    bool artifacts_enabled() const override;
    bool features_enabled() const override;
    bool monster_traps_enabled() const override;
    bool original_features(FeatureVariant) const override;
};
Rule parse_rule(const std::string&);
void after_floor_generation();
}
