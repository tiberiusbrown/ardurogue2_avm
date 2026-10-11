#pragma once
#include <cstdint>
namespace sim {
struct TuningFitness { uint64_t escaped=0,failures=0,actions=0; int64_t score=0; };
inline bool fitness_better(const TuningFitness& a,const TuningFitness& b) {
    if(bool(a.failures)!=bool(b.failures)) return !a.failures;
    if(a.escaped!=b.escaped) return a.escaped>b.escaped;
    return a.score>b.score;
}
}
