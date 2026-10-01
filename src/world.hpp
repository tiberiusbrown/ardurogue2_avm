#pragma once

#include "model.hpp"

namespace rogue {

bool marked(const FloorMarks& marks, FloorMark group, uint8_t index);
void mark(FloorMarks& marks, FloorMark group, uint8_t index);
bool wall_at(int16_t x, int16_t y);
bool wall_exposed(uint8_t x, uint8_t y);
void explore(uint8_t x, uint8_t y);
bool explored(uint8_t x, uint8_t y);
uint8_t door_at(uint8_t x, uint8_t y);
bool door_open(uint8_t index);
bool blocked(int16_t x, int16_t y);
void visit_room();
void make_floor();
bool can_see(uint8_t x, uint8_t y);
bool ray_visible(uint8_t sx, uint8_t sy, const uint16_t opaque[13]);

} // namespace rogue
