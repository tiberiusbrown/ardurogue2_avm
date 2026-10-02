#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "status.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0;
uint8_t avm_test_button_index = 0;
void (*avm_test_text_hook)(int16_t, int16_t, const char*) = nullptr;

namespace rogue {
Ui ui = {};
void status_clear() { reset_status_position(); }
bool known[WAND_COUNT] = {};
bool other_known[256] = {};
bool item_type_identified(uint8_t type)
{
    return is_wand(type) ? known[type - WAND_FORCE] : other_known[type];
}
uint8_t item_appearance(uint8_t type)
{
    return is_wand(type) ? type - WAND_FORCE : 0;
}
}

static char drawn[128];
static int16_t draw_next_x;
static bool draw_cursor_ok;
static void capture_text(int16_t x, int16_t y, const char* text)
{
    if(x != draw_next_x || y != 27) draw_cursor_ok = false;
    draw_next_x = static_cast<int16_t>(x + std::strlen(text) * 4);
    std::strncat(drawn, text, sizeof drawn - std::strlen(drawn) - 1);
}

static char status_draws[1024];
static void capture_status(int16_t x, int16_t y, const char* text)
{
    if(x == 128) return;
    char draw[48];
    std::snprintf(draw, sizeof draw, "%d,%d:%s;", x, y, text);
    std::strncat(status_draws, draw,
                 sizeof status_draws - std::strlen(status_draws) - 1);
}

static void check_mutable_status_word(const char* word, char punctuation)
{
    using namespace rogue;
    reset_status_position();
    status_draws[0] = 0;
    avm_test_text_hook = capture_status;
    status_word(word, punctuation);
    char expected[sizeof status_draws];
    std::strcpy(expected, status_draws);
    uint8_t expected_y = status_baseline();
    reset_status_position();
    status_draws[0] = 0;
    status(word, punctuation);
    avm_test_text_hook = nullptr;
    if(std::strcmp(status_draws, expected) ||
       status_baseline() != expected_y) {
        std::fprintf(stderr, "mutable status wrapping changed: %s != %s\n",
                     status_draws, expected);
        std::exit(1);
    }
}

static void check_status_paging()
{
    using namespace rogue;
    const char* message =
        "alpha alpha alpha alpha alpha alpha alpha alpha alpha alpha "
        "alpha alpha alpha alpha alpha alpha alpha alpha alpha alpha";
    for(uint8_t i = 0; i < 16; ++i)
        avm_test_buttons[i] = i & 1 ? AVM_BUTTON_A : 0;
    avm_test_button_count = 16;
    avm_test_button_index = 0;
    reset_status_position();
    status_draws[0] = 0;
    avm_test_text_hook = capture_status;
    for(uint8_t i = 0; i < 20; ++i)
        status_word("alpha");
    char expected[sizeof status_draws];
    std::strcpy(expected, status_draws);
    uint8_t expected_y = status_baseline();
    avm_test_button_index = 0;
    reset_status_position();
    status_draws[0] = 0;
    status(message);
    avm_test_text_hook = nullptr;
    avm_test_button_count = 0;
    if(!std::strstr(expected, "[more]") ||
       std::strcmp(status_draws, expected) ||
       status_baseline() != expected_y) {
        std::fprintf(stderr, "mutable status paging changed: %s != %s\n",
                     status_draws, expected);
        std::exit(1);
    }
}

static void check_drawn_item(rogue::Item item)
{
    using namespace rogue;
    char expected[ITEM_TEXT_CAPACITY];
    format_item(item, expected);
    drawn[0] = 0;
    draw_next_x = 8;
    draw_cursor_ok = true;
    avm_test_text_hook = capture_text;
    draw_item_text(8, 27, item);
    avm_test_text_hook = nullptr;
    if(!draw_cursor_ok || std::strcmp(drawn, expected)) {
        std::fprintf(stderr, "drawn item differs from label: %s != %s\n",
                     drawn, expected);
        std::exit(1);
    }
}

int main()
{
    using namespace rogue;
    const char* unknown[] = {
        "long wand", "short wand", "slender wand", "thick wand",
        "twisted wand", "curved wand", "glossy wand"
    };
    const char* identified[] = {
        "wand of force 5", "wand of teleportation 5",
        "wand of digging 5", "wand of fire 5", "wand of striking 5",
        "wand of ice 5", "wand of polymorph 5"
    };
    for(uint8_t i = 0; i < WAND_COUNT; ++i) {
        char text[ITEM_TEXT_CAPACITY];
        Item wand = {static_cast<uint8_t>(WAND_FORCE + i), 5};
        format_item(wand, text);
        if(std::strcmp(text, unknown[i])) {
            std::fprintf(stderr, "wrong unknown wand text: %s\n", text);
            return 1;
        }
        known[i] = true;
        format_item(wand, text);
        const char* bare[] = {
            "wand of force", "wand of teleportation", "wand of digging",
            "wand of fire", "wand of striking", "wand of ice",
            "wand of polymorph"
        };
        if(std::strcmp(text, bare[i])) {
            std::fprintf(stderr, "wrong type-known wand text: %s\n", text);
            return 1;
        }
        wand.info |= ITEM_IDENTIFIED;
        format_item(wand, text);
        if(std::strcmp(text, identified[i])) {
            std::fprintf(stderr, "wrong identified wand text: %s\n", text);
            return 1;
        }
    }
    Item longest = {WAND_TELEPORT, static_cast<uint8_t>(15 | ITEM_IDENTIFIED)};
    set_wand_modifier(longest, WAND_OVERPOWERED);
    char text[ITEM_TEXT_CAPACITY];
    format_item(longest, text);
    if(std::strcmp(text, "overpowered wand of teleportation 15")) {
        std::fprintf(stderr, "longest wand label was truncated: %s\n", text);
        return 1;
    }
    check_drawn_item(longest);
    const char* modifiers[] = {
        "wand of fire 7", "cursed wand of fire 7",
        "unreliable wand of fire 7", "spreading wand of fire 7",
        "powerful wand of fire 7", "overpowered wand of fire 7"
    };
    for(uint8_t i = WAND_NORMAL; i <= WAND_OVERPOWERED; ++i) {
        Item wand = {WAND_FIRE, static_cast<uint8_t>(ITEM_IDENTIFIED | 7)};
        set_wand_modifier(wand, static_cast<WandModifier>(i));
        format_item(wand, text);
        if(std::strcmp(text, modifiers[i])) {
            std::fprintf(stderr, "wrong modifier label: %s\n", text);
            return 1;
        }
        check_drawn_item(wand);
        wand.info &= static_cast<uint8_t>(~ITEM_IDENTIFIED);
        format_item(wand, text);
        if(std::strcmp(text, "wand of fire")) {
            std::fprintf(stderr, "unidentified wand leaked properties: %s\n",
                         text);
            return 1;
        }
        check_drawn_item(wand);
    }
    for(uint8_t type = FOOD; type <= WAND_POLYMORPH; ++type) {
        Item item = {type, 3};
        other_known[type] = false;
        if(is_wand(type)) known[type - WAND_FORCE] = false;
        check_drawn_item(item);
        other_known[type] = true;
        if(is_wand(type)) known[type - WAND_FORCE] = true;
        check_drawn_item(item);
        item.info |= ITEM_IDENTIFIED;
        check_drawn_item(item);
    }
    check_mutable_status_word("extraordinarilylongword", 0);
    check_mutable_status_word("extraordinarilylongword", '!');
    check_status_paging();
    std::puts("wand item text passed");
    return 0;
}
