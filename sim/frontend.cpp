#include "game_internal.hpp"
#include "game.hpp"
#include "world.hpp"
#include "status.hpp"
#include "explorer_events.hpp"
namespace rogue {
Game game{};
const char* status_word(const char*) { return nullptr; }
void status_words(const char*) {}
void status_suffix(char) {}
void status_capitalize() {}
void status(const char* s) { sim::explorer_status_text(s); }
void status(const char*, char) {}
void status(Item) {}
void status(Item, char) {}
void status(const Monster& m) {
    // Match status_entity: an unseen creature is printed as "something".
    sim::explorer_status_monster(can_see(m.pos) && player_can_see_monster(m) ? m.type : uint8_t(NO_MONSTER));
}
void status(const Monster& m, char) { status(m); }
void status_number(uint8_t) {}
void status_number(uint8_t, char) {}
void animate_arrow(Position, int8_t, int8_t, uint8_t) {}
void animate_ray(Position, int8_t, int8_t, uint8_t) {}
void animate_fire_burst(Position) {}
void animate_spreading_rays(Position, const uint8_t[4]) {}
void animate_fire_bursts(const Position*, uint8_t, bool) {}
}
