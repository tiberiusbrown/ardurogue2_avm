#include "app_state.hpp"
#include "game.hpp"
#include "status.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0;
uint8_t avm_test_button_index = 0;

namespace rogue {
Ui ui = {};
void status_clear() {}
bool known[WAND_COUNT] = {};
bool item_type_identified(uint8_t type)
{
    return is_wand(type) && known[type - WAND_FORCE];
}
uint8_t item_appearance(uint8_t type)
{
    return is_wand(type) ? type - WAND_FORCE : 0;
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
        wand.info &= static_cast<uint8_t>(~ITEM_IDENTIFIED);
        format_item(wand, text);
        if(std::strcmp(text, "wand of fire")) {
            std::fprintf(stderr, "unidentified wand leaked properties: %s\n",
                         text);
            return 1;
        }
    }
    std::puts("wand item text passed");
    return 0;
}
