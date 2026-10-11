#pragma once
#include <cstdint>
namespace sim {
struct TuningFitness {
    uint64_t escaped=0,failures=0,actions=0;
    int64_t score=0;
    uint64_t yendor=0,lord=0,progression=0;
};
inline uint64_t progression_credit(unsigned depth,bool lord,bool yendor,unsigned exited) {
    // Each milestone is awarded once per seed. Farming score or turns cannot
    // increase it. A deeper surviving trajectory dominates early fighting.
    return depth*16 + (depth>=1 ? 64 : 0) + (depth>=4 ? 256 : 0) +
        (depth>=9 ? 1024 : 0) + (depth>=14 ? 4096 : 0) +
        (lord ? 8192 : 0) + (yendor ? 16384 : 0) +
        (yendor && exited>16 ? (exited-16)*1024 : 0);
}
inline bool fitness_better(const TuningFitness& a,const TuningFitness& b) {
    if(bool(a.failures)!=bool(b.failures)) return !a.failures;
    if(a.escaped!=b.escaped) return a.escaped>b.escaped;
    if(a.yendor!=b.yendor) return a.yendor>b.yendor;
    if(a.lord!=b.lord) return a.lord>b.lord;
    if(a.progression!=b.progression) return a.progression>b.progression;
    if(a.actions!=b.actions) return a.actions<b.actions;
    return a.score>b.score;
}
}
