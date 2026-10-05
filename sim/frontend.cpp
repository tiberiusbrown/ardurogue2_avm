#include "game_internal.hpp"
#include "status.hpp"
namespace rogue {
Game game{};
const char* status_word(const char*) { return nullptr; }
void status_words(const char*) {}
void status_suffix(char) {}
void status_capitalize() {}
void status(const char*) {}
void status(const char*, char) {}
void status(Item) {}
void status(Item, char) {}
void status(MonsterType) {}
void status(MonsterType, char) {}
void status_number(uint8_t) {}
void status_number(uint8_t, char) {}
void animate_ray(Position, int8_t, int8_t, uint8_t) {}
void animate_fire_burst(Position) {}
void animate_spreading_rays(Position, const uint8_t[4]) {}
void animate_fire_bursts(const Position*, uint8_t, bool) {}
}
