#pragma once
#include "explorer.hpp"
namespace sim {
// Engine-only bridge. Policy includes explorer.hpp, never this header.
ExplorerObservation observe_explorer(uint64_t actions, uint64_t turns);
class ExplorerObserver {
    uint8_t floor=255;
    bool ascent=false;
    std::bitset<EXPLORER_CELLS> explicit_reveals;
    uint8_t previous_hp=0;
public:
    ExplorerObservation observe(uint64_t actions,uint64_t turns);
};
}
