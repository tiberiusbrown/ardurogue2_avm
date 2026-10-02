#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "world.hpp"

#include <cstdio>
#include <cstring>

uint8_t __avm_framebuffer[1024] = {};
void (*avm_test_text_hook)(int16_t, int16_t, const char*) = nullptr;
uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0, avm_test_button_index = 0;
namespace rogue {
Game game = {};
Ui ui = {};
}

static uint32_t hash_bytes(uint32_t hash, const uint8_t* bytes, size_t size)
{
    for(size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

int main()
{
    using namespace rogue;
    // Baseline snapshots cover viewport clipping, rooms, corridors and doors.
    // Captured with the original renderer; hashes include exploration changes.
    const Position positions[] = {
        {0, 0}, {63, 0}, {0, 31}, {63, 31}, {10, 10}, {32, 16}
    };
    const uint32_t expected[] = {
        0xc0b79398u, 0xc7113a38u, 0x473d2b98u, 0x8bbf3a38u,
        0x863529d4u, 0xfbdd3b6bu, 0x4d57b998u, 0x7b1ab8b4u,
        0x2db7c517u, 0x7af2648du, 0x3a284971u
    };
    for(unsigned i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        start_new(0x4312);
        if(i < 6) game.player = positions[i];
        else if(i == 6) game.player = game.up;
        else if(i == 7) game.player = game.down;
        else {
            // A closed door stops sight down a corridor; opening it must
            // immediately reveal the items and monsters beyond it.
            std::memset(game.walls, 0xff, sizeof game.walls);
            std::memset(game.explored, i == 10 ? 0xff : 0, sizeof game.explored);
            std::memset(game.rooms, 0, sizeof game.rooms);
            std::memset(game.monsters, 0, sizeof game.monsters);
            std::memset(game.ground, 0, sizeof game.ground);
            for(uint8_t x = 4; x < 17; ++x) carve(x, 10);
            game.player = {10, 10};
            game.door_count = 1;
            game.doors[0] = {{12, 10}};
            if(i > 8) open_door(0);
            game.ground[0] = {{13, 10}, {FOOD, 1}};
            game.monsters[0] = {{14, 10}, GOBLIN, 8, 0, {0, 0}, MON_AGGRO};
        }
        std::memset(__avm_framebuffer, 0xa5, sizeof __avm_framebuffer);
        render_play();
        uint32_t hash = hash_bytes(2166136261u, __avm_framebuffer,
                                  sizeof __avm_framebuffer);
        hash = hash_bytes(hash, game.explored, sizeof game.explored);
        if(hash != expected[i]) {
            std::fprintf(stderr, "Rendering case %u: expected %08x, got %08x\n",
                         i, static_cast<unsigned>(expected[i]),
                         static_cast<unsigned>(hash));
            return 1;
        }
    }
    return 0;
}
