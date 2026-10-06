#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "status.hpp"
#include "world.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>

uint8_t __avm_framebuffer[1024] = {};
void (*avm_test_text_hook)(int16_t, int16_t, const char*) = nullptr;
uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0, avm_test_button_index = 0;
unsigned avm_test_displays = 0;
uint16_t avm_test_millis = 0;
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

static void reference_pixel(uint8_t frame[1024], int x, int y)
{
    if(x >= 0 && x < 64 && y >= 0 && y < 64)
        frame[(y >> 3) * 128 + x] |= static_cast<uint8_t>(1u << (y & 7));
}

// Independent per-tile oracle: Bresenham visibility and the original wall
// drawing rules, rather than packed rows or batched framebuffer writes.
static void reference_terrain(uint8_t frame[1024])
{
    using namespace rogue;
    bool visible[13][13] = {};
    int left = int(game.player.x) - 6, top = int(game.player.y) - 6;
    for(int sy = 0; sy < 13; ++sy)
        for(int sx = 0; sx < 13; ++sx) {
            int x = left + sx, y = top + sy;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            visible[sy][sx] = can_see({uint8_t(x), uint8_t(y)});
        }
    for(int sy = 0; sy < 13; ++sy)
        for(int sx = 0; sx < 13; ++sx) {
            int x = left + sx, y = top + sy;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if(wall_at(x, y) && in_light_radius(sx - 6, sy - 6, 6)) {
                const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
                for(unsigned n = 0; n < 4; ++n) {
                    int nx = sx + dx[n], ny = sy + dy[n];
                    if(nx >= 0 && nx < 13 && ny >= 0 && ny < 13 &&
                       visible[ny][nx] && !wall_at(left + nx, top + ny))
                        visible[sy][sx] = true;
                }
            }
            if(visible[sy][sx]) explore({uint8_t(x), uint8_t(y)});
        }
    for(int sy = 0; sy < 13; ++sy)
        for(int sx = 0; sx < 13; ++sx) {
            int x = left + sx, y = top + sy;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if(!visible[sy][sx] && !explored({uint8_t(x), uint8_t(y)})) continue;
            if(wall_at(x, y)) {
                if(!wall_exposed(uint8_t(x), uint8_t(y))) continue;
                for(int row = 0; row < 4; ++row)
                    for(int col = 0; col < 4; ++col)
                        reference_pixel(frame, sx * 5 + col, sy * 5 + row);
                if(sx < 12 && x + 1 < MAP_W && wall_at(x + 1, y) &&
                   wall_exposed(uint8_t(x + 1), uint8_t(y)) &&
                   explored({uint8_t(x + 1), uint8_t(y)}))
                    for(int row = 0; row < 4; ++row)
                        reference_pixel(frame, sx * 5 + 4, sy * 5 + row);
                if(sy < 12 && y + 1 < MAP_H && wall_at(x, y + 1) &&
                   wall_exposed(uint8_t(x), uint8_t(y + 1)) &&
                   explored({uint8_t(x), uint8_t(y + 1)}))
                    for(int col = 0; col < 4; ++col)
                        reference_pixel(frame, sx * 5 + col, sy * 5 + 4);
            } else if(visible[sy][sx])
                reference_pixel(frame, sx * 5 + 2, sy * 5 + 2);
        }
}

static bool check_terrain_rows()
{
    using namespace rogue;
    const Position positions[] = {{0, 0}, {63, 0}, {0, 31}, {63, 31},
        {6, 6}, {7, 7}, {8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12},
        {13, 13}, {57, 25}, {58, 26}, {1, 1}, {2, 2}, {3, 3}, {4, 4},
        {5, 5}, {59, 27}, {60, 28}, {61, 29}, {62, 30}};
    uint32_t random = 0x4312;
    for(unsigned pattern = 0; pattern < 24; ++pattern) {
        start_new(0x4312);
        std::memset(game.monsters, 0, sizeof game.monsters);
        std::memset(game.ground, 0, sizeof game.ground);
        game.door_count = 0;
        game.up = game.down = {NONE, NONE};
        for(unsigned i = 0; i < sizeof game.walls; ++i) {
            random = random * 1664525u + 1013904223u;
            game.walls[i] = pattern == 0 ? 0 : pattern == 1 ? 0xff :
                pattern == 2 ? 0x55 : pattern == 3 ? 0xaa : uint8_t(random >> 24);
            random = random * 1664525u + 1013904223u;
            game.explored[i] = pattern % 3 == 0 ? 0 :
                pattern % 3 == 1 ? 0xff : uint8_t(random >> 24);
        }
        // Room cases contain open rectangles, as the floor generator creates.
        if(pattern == 20) std::memset(game.walls, 0, sizeof game.walls);
        for(uint8_t y = 0; y < MAP_H; ++y)
            for(uint8_t x = 0; x < MAP_W; ++x)
                if((pattern == 21 && ((x < 10 && y < 10) || (x >= 54 && y >= 22))) ||
                   (pattern == 22 && x >= 5 && x < 15 && y >= 5 && y < 15) ||
                   (pattern == 23 && ((x < 10 && y < 10) ||
                                     (x >= 5 && x < 15 && y >= 5 && y < 15))))
                    carve(x, y);
        Game initial = game;
        for(Position pos : positions) {
            game = initial;
            game.player = pos;
            carve(pos.x, pos.y);
            uint8_t before[sizeof game.explored], expected[sizeof game.explored];
            std::memcpy(before, game.explored, sizeof before);
            uint8_t frame[1024] = {};
            reference_terrain(frame);
            std::memcpy(expected, game.explored, sizeof expected);
            std::memcpy(game.explored, before, sizeof before);
            render_play();
            if(std::memcmp(expected, game.explored, sizeof expected)) {
                std::fprintf(stderr, "Exploration changed in terrain pattern %u at %u,%u\n",
                             pattern, unsigned(pos.x), unsigned(pos.y));
                return false;
            }
            for(int y = 0; y < 64; ++y)
                for(int x = 0; x < 64; ++x) {
                    // The player icon overwrites this terrain tile.
                    if(x >= 30 && x < 34 && y >= 30 && y < 34) continue;
                    unsigned index = (y >> 3) * 128 + x;
                    uint8_t bit = static_cast<uint8_t>(1u << (y & 7));
                    if((frame[index] & bit) != (__avm_framebuffer[index] & bit)) {
                        std::fprintf(stderr, "Terrain pattern %u at %u,%u differs at pixel %d,%d\n",
                                     pattern, unsigned(pos.x), unsigned(pos.y), x, y);
                        return false;
                    }
                }
        }
    }
    return true;
}

static uint8_t page_background[1024];
static unsigned restored_pages;
static bool page_background_ok;

static void capture_restored_page(int16_t x, int16_t, const char* text)
{
    if(x == 128 || std::strcmp(text, "[more]")) return;
    for(unsigned row = 0; row < 8; ++row)
        for(unsigned col = 0; col < 65; ++col)
            if(__avm_framebuffer[row * 128 + col] != page_background[row * 128 + col])
                page_background_ok = false;
    ++restored_pages;
    // A subsequent page must retain this already-rendered dungeon background.
    __avm_framebuffer[0] ^= 0x80;
    page_background[0] ^= 0x80;
}

static bool check_deferred_pages()
{
    using namespace rogue;
    start_new(0x4312);
    ui = {};
    ui.mode = PLAY;
    render_play();
    std::memcpy(page_background, __avm_framebuffer, sizeof page_background);
    std::memset(__avm_framebuffer, 0xa5, sizeof __avm_framebuffer);
    status_clear();
    defer_play_render();
    restored_pages = 0;
    page_background_ok = true;
    avm_test_text_hook = capture_restored_page;
    for(uint8_t i = 0; i < 16; ++i) avm_test_buttons[i] = i & 1 ? AVM_BUTTON_A : 0;
    avm_test_button_count = 16;
    avm_test_button_index = 0;
    for(unsigned i = 0; i < 24; ++i) status_word("alpha");
    avm_test_text_hook = nullptr;
    avm_test_button_count = 0;
    __avm_framebuffer[0] ^= 0x40;
    page_background[0] ^= 0x40;
    restore_play_render();
    if(restored_pages != 2 || !page_background_ok ||
       __avm_framebuffer[0] != page_background[0]) return false;
    // Page clearing must affect only the status rectangle.
    for(int y = 23; y < 64; ++y)
        for(int x = 65; x < 128; ++x)
            if(__avm_framebuffer[(y >> 3) * 128 + x] & (1u << (y & 7))) return false;

    render_play();
    std::memcpy(page_background, __avm_framebuffer, sizeof page_background);
    std::memset(__avm_framebuffer, 0xa5, sizeof __avm_framebuffer);
    status_clear();
    defer_play_render();
    render_yesno_prompt(F("Confirm?"), nullptr);
    for(unsigned row = 0; row < 8; ++row)
        if(std::memcmp(__avm_framebuffer + row * 128,
                       page_background + row * 128, 65)) return false;
    return true;
}

static bool loading_text_in_pane, loading_stats_visible;

static void capture_loading_text(int16_t x, int16_t y, const char* text)
{
    if(!std::strcmp(text, "Generating..."))
        loading_text_in_pane = x >= 0 && x + 4 * std::strlen(text) <= 64 && y < 64;
    if(x == 67 && y == 7 && text[0] == 'D') loading_stats_visible = true;
}

static bool check_generation_loading()
{
    using namespace rogue;
    start_new(0x4312);
    // The animation must be safe even while explored contains BFS scratch
    // and door coordinates contain the generator's temporary priority bits.
    std::memset(game.explored, 0xa5, sizeof game.explored);
    game.doors[0].pos.y = 255;
    Game before = game;
    ui = {};
    ui.mode = TITLE;
    avm_test_millis = 65500;
    avm_test_displays = 0;
    loading_text_in_pane = loading_stats_visible = false;
    avm_test_text_hook = capture_loading_text;
    begin_generation_render();
    avm_test_text_hook = nullptr;
    if(avm_test_displays != 1 || !loading_text_in_pane || !loading_stats_visible) return false;
    // Retained pixels elsewhere in the dungeon pane must survive updates too.
    __avm_framebuffer[2] |= 4;
    __avm_framebuffer[7 * 128 + 2] |= 16;
    uint8_t frame[1024];
    std::memcpy(frame, __avm_framebuffer, sizeof frame);
    avm_test_millis = static_cast<uint16_t>(65500 + 149);
    update_generation_render(55);
    if(avm_test_displays != 1 || std::memcmp(frame, __avm_framebuffer, sizeof frame)) return false;
    ++avm_test_millis;
    for(unsigned step = 0; step < 8; ++step) {
        update_generation_render();
        if(avm_test_displays != step + 2) return false;
        bool animated = false;
        for(unsigned row = 0; row < 8; ++row) {
            animated |= std::memcmp(frame + row * 128, __avm_framebuffer + row * 128, 64) != 0;
            if(std::memcmp(frame + row * 128 + 64, __avm_framebuffer + row * 128 + 64, 64)) return false;
        }
        if(!(__avm_framebuffer[2] & 4) || !(__avm_framebuffer[7 * 128 + 2] & 16)) return false;
        if(!animated || std::memcmp(&before, &game, sizeof game)) return false;
        std::memcpy(frame, __avm_framebuffer, sizeof frame);
        avm_test_millis = static_cast<uint16_t>(avm_test_millis + 150);
    }
    end_generation_render();
    return !std::memcmp(&before, &game, sizeof game) && ui.dirty &&
        !ui.held_direction && !ui.selection && ui.repeat_suppressed;
}

int main()
{
    using namespace rogue;
    if(!check_generation_loading()) {
        std::fprintf(stderr, "Generation loading cadence, pane bounds or scratch isolation failed\n");
        return 1;
    }
    if(!check_packed_wall_rows()) {
        std::fprintf(stderr, "Packed wall row extraction failed\n");
        return 1;
    }
    if(!check_shared_icons()) {
        std::fprintf(stderr, "Shared item/mimic icons or independent appearance flags failed\n");
        return 1;
    }
    if(!check_terrain_rows() || !check_deferred_pages()) {
        std::fprintf(stderr, "Terrain rows or deferred page/prompt restoration failed\n");
        return 1;
    }
    // Baseline snapshots cover viewport clipping, rooms, corridors and doors.
    // Hashes include exploration changes and the current generated loot.
    const Position positions[] = {
        {0, 0}, {63, 0}, {0, 31}, {63, 31}, {10, 10}, {32, 16}
    };
    const uint32_t expected[] = {
        0xc0b79398u, 0xc7113a38u, 0x473d2b98u, 0x8bbf3a38u,
        0x226e8606u, 0xc280f35fu, 0x37850c9cu, 0xdc749961u,
        0x4b46c42bu, 0xd975e0abu, 0x0d861207u
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
            std::memset(game.monsters, 0, sizeof game.monsters);
            std::memset(game.ground, 0, sizeof game.ground);
            for(uint8_t x = 4; x < 17; ++x) carve(x, 10);
            game.player = {10, 10};
            // Keep the synthetic corridor fixture independent of generation.
            game.up = {4, 10};
            game.down = {16, 10};
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
