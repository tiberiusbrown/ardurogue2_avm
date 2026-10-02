#pragma once

#include "model.hpp"

namespace rogue {

bool wall_at(int16_t x, int16_t y);
bool wall_exposed(uint8_t x, uint8_t y);
void explore(Position pos);
bool explored(Position pos);
uint8_t door_at(Position pos);
bool door_open(uint8_t index);
void open_door(uint8_t index);
Position door_position(uint8_t index);
bool blocked(int16_t x, int16_t y);
void carve(uint8_t x, uint8_t y);
struct RayResult {
    Position end, before;
    uint8_t monster, steps;
    bool blocker;
};
RayResult scan_ray(Position origin, int8_t dx, int8_t dy, uint8_t range);
void make_floor();
bool can_see(Position pos);
bool ray_visible(uint8_t sx, uint8_t sy, const uint16_t opaque[13]);

} // namespace rogue
