#include "app_state.hpp"
#include "game.hpp"
#include "game_internal.hpp"
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

static uint8_t arrow_frames[7][1024];
static unsigned arrow_frame_count;
static void capture_arrow_frame()
{
    if(arrow_frame_count < 7)
        std::memcpy(arrow_frames[arrow_frame_count], __avm_framebuffer, 1024);
    ++arrow_frame_count;
}

static bool check_arrow_animation()
{
    using namespace rogue;
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.ground, 0, sizeof game.ground);
    game.player = {20, 15}; game.door_count = 0;
    render_play();
    Game before = game;
    uint8_t base[1024], expected[1024];
    std::memcpy(base, __avm_framebuffer, 1024);
    const int8_t dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    const uint16_t sprites[] = {0x2f20, 0x44e4, 0x4f40, 0x4e44};
    for(unsigned direction = 0; direction < 4; ++direction) {
        arrow_frame_count = 0; avm_test_millis = 65500;
        avm_test_display_hook = capture_arrow_frame;
        animate_arrow(game.player, dx[direction], dy[direction], 6);
        avm_test_display_hook = nullptr;
        if(arrow_frame_count != 7 || avm_test_millis != uint16_t(65500 + 360) ||
           std::memcmp(&before, &game, sizeof game) ||
           std::memcmp(base, __avm_framebuffer, 1024)) return false;
        for(unsigned step = 1; step <= 6; ++step) {
            std::memcpy(expected, base, 1024);
            int x = 30 + dx[direction] * int(step) * 5;
            int y = 30 + dy[direction] * int(step) * 5;
            for(int col = 0; col < 4; ++col) for(int row = 0; row < 4; ++row) {
                auto& pixel = expected[((y + row) >> 3) * 128 + x + col];
                uint8_t mask = uint8_t(1u << ((y + row) & 7));
                bool lit = (sprites[direction] >> (12 - col * 4)) & (1u << row);
                pixel = lit ? pixel | mask : pixel & ~mask;
            }
            if(std::memcmp(expected, arrow_frames[step - 1], 1024)) return false;
        }
        if(std::memcmp(base, arrow_frames[6], 1024)) return false;
    }
    return item_icon(ARROWS) != item_icon(SHORT_BOW) && item_icon(ARROWS) != 0;
}

static bool check_effect_sprites()
{
    using namespace rogue;
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.ground, 0, sizeof game.ground);
    game.player = {20, 15}; game.door_count = 0;
    for(int x : {0, 30, 60}) for(int y : {0, 30, 60}) {
        render_play();
        uint8_t base[1024], expected[1024];
        std::memcpy(base, __avm_framebuffer, sizeof base);
        std::memcpy(expected, base, sizeof expected);
        for(int pass = 0; pass < 2; ++pass)
            for(int oy = -1; oy <= 1; ++oy)
                for(int ox = -1; ox <= 1; ++ox) {
                    if((pass == 0) == (!ox && !oy)) continue;
                    for(int col = 0; col < 4; ++col)
                        for(int row = 0; row < 4; ++row) {
                            int px = x + ox + col, py = y + oy + row;
                            if(px < 0 || px >= 64 || py < 0 || py >= 64 ||
                               !((0x0eae >> (12 - col * 4)) & (1u << row))) continue;
                            uint8_t& byte = expected[(py >> 3) * 128 + px];
                            uint8_t mask = uint8_t(1u << (py & 7));
                            byte = pass ? byte | mask : byte & ~mask;
                        }
                }
        arrow_frame_count = 0;
        avm_test_display_hook = capture_arrow_frame;
        animate_ray({uint8_t(game.player.x + x / 5 - 7),
                     uint8_t(game.player.y + y / 5 - 6)}, 1, 0, 1);
        avm_test_display_hook = nullptr;
        if(arrow_frame_count != 2 || std::memcmp(expected, arrow_frames[0], sizeof expected) ||
           std::memcmp(base, arrow_frames[1], sizeof base)) return false;
    }
    return true;
}

static char hidden_target_text[256];
static void capture_hidden_target_text(int16_t x, int16_t y, const char* text)
{
    if(x < 128 && x >= 65 && y >= 23) {
        size_t used = std::strlen(hidden_target_text), n = std::strlen(text);
        if(used + n < sizeof hidden_target_text)
            std::memcpy(hidden_target_text + used, text, n + 1);
    }
}
static bool check_hidden_arrow_status()
{
    using namespace rogue;
    for(bool hit : {false, true}) for(uint8_t hp : {uint8_t(1), uint8_t(100)}) {
        game = {}; session = {NONE, DEATH, false}; ui = {};
        game.player = {20, 15}; game.hp = game.max_hp = 240; game.hunger = 240;
        game.strength = 5; game.dexterity = 4; game.speed = game.level = 1;
        game.weapon_slot = 0;
        game.armor_slot = game.amulet_slot = game.ring_slots[0] = game.ring_slots[1] = NONE;
        game.inventory[0] = make_equipment(LONG_BOW, 0);
        game.inventory[1] = {ARROWS, 4};
        game.monsters[0] = {{23, 15}, PHANTOM, hp, 15, {0, 0}, 0};
        for(unsigned seed = 1; seed <= 65535; ++seed) {
            game.random_state = uint16_t(seed);
            if(physical_attack_hits(player_ranged_accuracy(LONG_BOW), monster_dexterity(PHANTOM)) == hit) {
                game.random_state = uint16_t(seed); break;
            }
        }
        status_clear(); hidden_target_text[0] = 0;
        avm_test_text_hook = capture_hidden_target_text;
        bool accepted = throw_or_shoot(1, 1, 0);
        avm_test_text_hook = nullptr;
        if(!accepted || std::strstr(hidden_target_text, "phantom") || std::strstr(hidden_target_text, "Phantom") ||
           !std::strstr(hidden_target_text, "something")) {
            std::fprintf(stderr, "Hidden target hit=%u hp=%u accepted=%u text=%s\n", hit, hp, accepted, hidden_target_text);
            return false;
        }
    }
    return true;
}

static bool check_monster_reference_status()
{
    using namespace rogue;
    game = {}; ui = {}; session = {NONE, DEATH, false};
    game.player = {20, 15}; game.hp = game.max_hp = 100;
    game.weapon_slot = game.armor_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    for(uint8_t ring : {uint8_t(NO_ITEM), uint8_t(RING_SEE_INVISIBLE)})
        for(bool cursed : {false, true}) for(uint16_t turns : {0, 1})
            for(uint8_t type : {uint8_t(ORC), uint8_t(PHANTOM)})
                for(bool temporary : {false, true}) for(bool distant : {false, true}) {
                    game.inventory[0] = {ring, uint8_t(1 | (cursed ? ITEM_CURSED : 0))};
                    game.ring_slots[0] = ring ? 0 : NONE; game.turns = turns;
                    for(uint8_t index = 0; index < MONSTERS; ++index) {
                        auto& monster = game.monsters[index];
                        monster = {{uint8_t(distant ? 40 : 23), 15}, type, 100, 0, {0, 0}, 0};
                        if(temporary) set_monster_effect(monster, MON_INVISIBLE, 5);
                        bool detected = player_can_see_monster(index);
                        if(detected != player_can_see_monster(monster)) return false;
                        bool visible = !distant && (!cursed || !ring || !((turns + index) & 1)) &&
                            ((!temporary && type != PHANTOM) || (ring && !cursed));
                        status_clear(); hidden_target_text[0] = 0;
                        avm_test_text_hook = capture_hidden_target_text;
                        status_capitalize(); status(monster, '!');
                        avm_test_text_hook = nullptr;
                        const char* expected = !visible ? "Something!" : type == PHANTOM ? "Thephantom!" : "Theorc!";
                        if(std::strcmp(hidden_target_text, expected)) return false;
                    }
                }
    return true;
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

// Compare every item/monster frame to the original column-nibble artwork,
// including tiles that cross framebuffer page boundaries.
static bool check_sprite_artwork()
{
    using namespace rogue;
    const uint8_t items[] = {ARROWS, FOOD, POTION_HEALING, LONG_SWORD, CHAIN_MAIL,
                             AMULET_SPEED, RING_STRENGTH, SCROLL_IDENTIFY, WAND_FORCE};
    const uint16_t item_shapes[] = {0x8421, 0x9429, 0x0bb0, 0x04f4, 0x0f90,
                                    0x0606, 0x0aaa, 0x01b3, 0x1248};
    const uint16_t monster_shapes[] = {0, 0x0fa4, 0x0bd0, 0x0f5a, 0x9db9,
        0x0bf0, 0x0f52, 0x0f9f, 0x07a0, 0x0f2c, 0x0f2f, 0x09f9,
        0x01f1, 0x069d, 0xf996, 0x0e5e, 0x0f88};
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.ground, 0, sizeof game.ground);
    game.player = {10, 10}; game.door_count = 0;
    game.up = game.down = {NONE, NONE};
    game.inventory[0] = {RING_SEE_INVISIBLE, 1}; game.ring_slots[0] = 0;
    for(unsigned kind = 0; kind < 2; ++kind)
        for(unsigned index = kind ? 1 : 0;
            index < (kind ? sizeof monster_shapes / sizeof *monster_shapes : sizeof items); ++index)
            for(uint8_t sy : {uint8_t(6), uint8_t(7), uint8_t(8)}) {
                Position pos{11, uint8_t(10 + sy - 6)};
                game.ground[0] = {pos, {kind ? uint8_t(NO_ITEM) : items[index], 1}};
                game.monsters[0] = {pos, kind ? uint8_t(index) : uint8_t(NO_MONSTER),
                                    100, 0, {0, 0}, MON_AGGRO};
                render_play();
                uint16_t shape = kind ? monster_shapes[index] : item_shapes[index];
                for(unsigned col = 0; col < 4; ++col)
                    for(unsigned row = 0; row < 4; ++row) {
                        unsigned y = sy * 5 + row;
                        bool actual = __avm_framebuffer[(y >> 3) * 128 + 35 + col] &
                                      (1u << (y & 7));
                        bool lit = (shape >> (12 - col * 4)) & (1u << row);
                        if(actual != lit) return false;
                    }
            }
    return true;
}

static bool check_shared_icons()
{
    using namespace rogue;
    if(item_icon(LONG_SWORD) != 4 || item_icon(CHAIN_MAIL) != 5 ||
       item_icon(NO_ITEM) != 0 || item_icon(255) != 0) return false;
    for(unsigned type = 0; type <= 255; ++type) {
        if(is_weapon(type) && item_icon(type) != 4) return false;
        if(is_armor(type) && item_icon(type) != 5) return false;
    }
    const uint8_t representatives[MIMIC_APPEARANCE_COUNT] = {
        SCROLL_IDENTIFY, POTION_HEALING, AMULET_SPEED, RING_STRENGTH, WAND_FORCE
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
        if(mimic_icon(category) != item_icon(representatives[appearance])) return false;
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
        for(unsigned col = 0; col < 4; ++col)
            for(unsigned row = 0; row < 4; ++row) {
                bool actual = disguised[((30 + row) >> 3) * 128 + 35 + col] &
                              (1u << ((30 + row) & 7));
                bool lit = (expected[appearance] >> (12 - col * 4)) & (1u << row);
                if(actual != lit) return false;
            }
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

static bool check_full_map_pixels()
{
    using namespace rogue;
    const Position positions[] = {{0, 0}, {63, 0}, {0, 31}, {63, 31}, {32, 16}};
    for(Position pos : positions) for(uint8_t pattern : {uint8_t(0), uint8_t(0x55), uint8_t(0xff)}) {
        start_new(0x4312);
        game.player = pos;
        std::memset(game.explored, pattern, sizeof game.explored);
        uint8_t expected[1024] = {};
        auto set_pixel = [&](unsigned x, unsigned y) {
            expected[(y >> 3) * 128 + x] |= uint8_t(1u << (y & 7));
        };
        for(uint8_t y = 0; y < MAP_H; ++y)
            for(uint8_t x = 0; x < MAP_W; ++x) {
                if(!explored({x, y})) continue;
                if(wall_exposed(x, y)) {
                    for(unsigned dy = 0; dy < 2; ++dy)
                        for(unsigned dx = 0; dx < 2; ++dx)
                            set_pixel(x * 2 + dx, y * 2 + dy);
                } else if(x + 6 >= pos.x && x <= pos.x + 6 &&
                          y + 6 >= pos.y && y <= pos.y + 6 && can_see({x, y}))
                    set_pixel(x * 2, y * 2);
            }
        set_pixel(pos.x * 2, pos.y * 2);
        set_pixel(pos.x * 2 + 1, pos.y * 2 + 1);
        ui.mode = FULL_MAP;
        std::memset(__avm_framebuffer, 0xa5, sizeof __avm_framebuffer);
        render();
        if(std::memcmp(expected, __avm_framebuffer, sizeof expected)) return false;
    }
    return true;
}

static bool check_direction_prompt()
{
    using namespace rogue;
    start_new(0x4312);
    std::memset(game.walls, 0, sizeof game.walls);
    std::memset(game.monsters, 0, sizeof game.monsters);
    std::memset(game.ground, 0, sizeof game.ground);
    game.player = {20, 15};
    game.door_count = 0;
    game.monsters[0] = {{22, 15}, GOBLIN, 8, 0, {0, 0}, MON_AGGRO};
    render_play();
    uint8_t dungeon[1024];
    std::memcpy(dungeon, __avm_framebuffer, sizeof dungeon);
    const uint8_t types[] = {POTION_HARMING, ARROWS, WAND_FIRE};
    for(uint8_t type : types) {
        game.inventory[0] = {type, 3};
        ui.selection = 0;
        ui.mode = is_wand(type) ? WAND_DIRECTION : PROJECTILE_DIRECTION;
        Game before = game;
        unsigned displays = avm_test_displays;
        // The picker leaves a full-screen inventory behind; the prompt must
        // rebuild the dungeon, including the player and visible monsters.
        std::memset(__avm_framebuffer, 0xa5, sizeof __avm_framebuffer);
        render();
        for(unsigned page = 0; page < 8; ++page)
            if(std::memcmp(dungeon + page * 128,
                           __avm_framebuffer + page * 128, 65)) return false;
        if(std::memcmp(&game, &before, sizeof game) || ui.dirty ||
           avm_test_displays != displays + 1) return false;
        // The stats above the prompt remain the ordinary dungeon stats.
        for(unsigned y = 0; y < 23; ++y)
            for(unsigned x = 65; x < 128; ++x) {
                unsigned index = (y / 8) * 128 + x;
                if((dungeon[index] ^ __avm_framebuffer[index]) & (1u << (y % 8)))
                    return false;
            }
    }
    return true;
}

int main()
{
    using namespace rogue;
    if(!check_direction_prompt()) {
        std::fprintf(stderr, "Direction prompt hid the dungeon or changed game state\n");
        return 1;
    }
    if(!check_arrow_animation() || !check_effect_sprites()) {
        std::fprintf(stderr, "Arrow direction, cadence, restoration or state/RNG isolation failed\n");
        return 1;
    }
    if(!check_hidden_arrow_status()) {
        std::fprintf(stderr, "Hidden arrow hit/miss/defeat status revealed a monster\n");
        return 1;
    }
    if(!check_monster_reference_status()) {
        std::fprintf(stderr, "Monster reference visibility, article or punctuation failed\n");
        return 1;
    }
    if(!check_generation_loading()) {
        std::fprintf(stderr, "Generation loading cadence, pane bounds or scratch isolation failed\n");
        return 1;
    }
    if(!check_packed_wall_rows()) {
        std::fprintf(stderr, "Packed wall row extraction failed\n");
        return 1;
    }
    if(!check_shared_icons() || !check_sprite_artwork()) {
        std::fprintf(stderr, "Shared item/mimic icons or independent appearance flags failed\n");
        return 1;
    }
    if(!check_terrain_rows() || !check_deferred_pages() || !check_full_map_pixels()) {
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
