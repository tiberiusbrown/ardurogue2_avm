#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "world.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>

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

static uint16_t reference_wall_row(uint8_t y, int16_t left)
{
    using namespace rogue;
    uint16_t bits = 0;
    for(uint8_t sx = 0; sx < 13; ++sx) {
        int x = static_cast<int>(left) + sx;
        if(y >= MAP_H || x < 0 || x >= MAP_W ||
           (game.walls[y * (MAP_W / 8) + (x >> 3)] & (1u << (x & 7))))
            bits |= static_cast<uint16_t>(1u << sx);
    }
    return bits;
}

static bool check_packed_wall_rows()
{
    using namespace rogue;
    uint32_t random = 0x4312;
    for(unsigned pattern = 0; pattern < 324; ++pattern) {
        for(uint8_t y = 0; y < MAP_H; ++y)
            for(uint8_t byte = 0; byte < MAP_W / 8; ++byte) {
                random = random * 1664525u + 1013904223u;
                uint8_t bits = static_cast<uint8_t>(random >> 24);
                if(pattern < 4)
                    bits = pattern == 0 ? 0 : pattern == 1 ? 0xff :
                        pattern == 2 ? 0x55 : 0xaa;
                else if(pattern < 68)
                    bits = byte == (pattern - 4) / 8
                        ? static_cast<uint8_t>(1u << ((pattern - 4) & 7)) : 0;
                game.walls[y * (MAP_W / 8) + byte] = bits;
            }
        // Every offset 0..7, two/three-byte runs, and clipping at both edges.
        // Include the first/last map rows to catch source-row overreads.
        for(uint8_t y = 0; y < MAP_H; ++y)
            for(int16_t left = -14; left <= MAP_W + 1; ++left) {
                uint16_t expected = reference_wall_row(y, left);
                uint16_t actual = wall_row_bits(y, left);
                if(actual != expected) {
                    std::fprintf(stderr, "Wall row pattern %u y %u left %d: %04x != %04x\n",
                                 pattern, unsigned(y), int(left), unsigned(actual), unsigned(expected));
                    return false;
                }
            }
    }
    for(uint8_t y : {uint8_t(0), uint8_t(MAP_H - 1), MAP_H, uint8_t(255)})
        for(int16_t left : {int16_t(-32768), int16_t(-13), int16_t(-6),
                           int16_t(0), int16_t(57), int16_t(63), int16_t(32767)})
            if(wall_row_bits(y, left) != reference_wall_row(y, left)) return false;
    return true;
}

static bool check_shared_icons()
{
    using namespace rogue;
    if(item_icon(LONG_SWORD) != 0x04f4 || item_icon(CHAIN_MAIL) != 0x0f90 ||
       item_icon(NO_ITEM) != 0 || item_icon(255) != 0) return false;
    for(unsigned type = 0; type <= 255; ++type) {
        if(is_weapon(type) && item_icon(type) != 0x04f4) return false;
        if(is_armor(type) && item_icon(type) != 0x0f90) return false;
    }
    const uint8_t representatives[MIMIC_APPEARANCE_COUNT] = {
        SCROLL_IDENTIFY, HEALING, AMULET_SPEED, RING_STRENGTH, WAND_FORCE
    };
    const uint16_t expected[MIMIC_APPEARANCE_COUNT] = {0x01b3, 0x0bb0, 0x0606, 0x0aaa, 0x1248};
    uint8_t disguised[sizeof __avm_framebuffer], revealed[sizeof __avm_framebuffer];
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.rooms, 0, sizeof game.rooms);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.ground, 0, sizeof game.ground);
    game.door_count = 0;
    game.player = {10, 10};
    game.up = {0, 0};
    game.down = {63, 31};
    for(uint8_t appearance = 0; appearance < MIMIC_APPEARANCE_COUNT; ++appearance) {
        MimicAppearance category = static_cast<MimicAppearance>(appearance);
        if(mimic_icon(category) != expected[appearance] ||
           mimic_icon(category) != item_icon(representatives[appearance])) return false;
        for(uint8_t flags : {uint8_t(0), MON_AGGRO, MON_AFRAID, uint8_t(MON_AGGRO | MON_AFRAID)}) {
            Monster monster{{11, 10}, MIMIC, 20, 0, {0, 0}, flags};
            set_mimic_appearance(monster, category);
            if(mimic_appearance(monster) != category ||
               (monster.state & ~MIMIC_APPEARANCE_MASK) != flags ||
               (monster.state & MIMIC_APPEARANCE_MASK) != (appearance << MIMIC_APPEARANCE_SHIFT))
                return false;
        }
        game.monsters[0] = {{11, 10}, MIMIC, 20, 0, {0, 0}, MON_AFRAID};
        set_mimic_appearance(game.monsters[0], category);
        render_play();
        std::memcpy(disguised, __avm_framebuffer, sizeof disguised);
        game.monsters[0].type = NO_MONSTER;
        game.ground[0] = {{11, 10}, {representatives[appearance], 1}};
        render_play();
        if(std::memcmp(disguised, __avm_framebuffer, sizeof disguised)) return false;
        game.ground[0].item.type = NO_ITEM;
        game.monsters[0].type = MIMIC;
        game.monsters[0].state |= MON_AGGRO;
        render_play();
        if(appearance == 0) std::memcpy(revealed, __avm_framebuffer, sizeof revealed);
        else if(std::memcmp(revealed, __avm_framebuffer, sizeof revealed)) return false;
        game.monsters[0].state &= static_cast<uint8_t>(~MON_AFRAID);
        if(mimic_appearance(game.monsters[0]) != category) return false;
    }
    return true;
}

int main()
{
    using namespace rogue;
    if(!check_packed_wall_rows()) {
        std::fprintf(stderr, "Packed wall row extraction failed\n");
        return 1;
    }
    if(!check_shared_icons()) {
        std::fprintf(stderr, "Shared item/mimic icons or independent appearance flags failed\n");
        return 1;
    }
    // Baseline snapshots cover viewport clipping, rooms, corridors and doors.
    // Hashes include exploration changes and the current generated loot.
    const Position positions[] = {
        {0, 0}, {63, 0}, {0, 31}, {63, 31}, {10, 10}, {32, 16}
    };
    const uint32_t expected[] = {
        0xc0b79398u, 0xc7113a38u, 0x473d2b98u, 0x8bbf3a38u,
        0x863529d4u, 0xfbdd3b6bu, 0x4d57b998u, 0x7b1ab8b4u,
        0x2db7c517u, 0x7af2648du, 0x3a284971u
    };
    bool failed = false;
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
            failed = true;
        }
    }
    return failed ? 1 : 0;
}
