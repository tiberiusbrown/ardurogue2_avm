#include "app_state.hpp"
#include "game.hpp"
#include "status.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        if(std::strcmp(text, identified[i])) {
            std::fprintf(stderr, "wrong identified wand text: %s\n", text);
            return 1;
        }
    }
    std::puts("wand item text passed");
    return 0;
}
