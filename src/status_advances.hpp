#pragma once
#include "model.hpp"

namespace rogue {
// BR5D printable ASCII advances. Verified against the SDK font in AVM bench_setup.
static constexpr uint8_t PROGMEM STATUS_ADVANCES[95] = {
    2, 2, 4, 6, 6, 4, 5, 2, 3, 3, 4, 4, 3, 4, 2, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 3, 4, 4, 4, 4,
    5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 6, 5, 5,
    4, 5, 4, 4, 4, 5, 4, 6, 4, 4, 4, 3, 4, 3, 4, 4,
    3, 4, 4, 3, 4, 4, 3, 4, 4, 2, 3, 4, 2, 6, 4, 4,
    4, 4, 4, 3, 3, 4, 4, 6, 4, 4, 3, 4, 2, 4, 5,
};
}
