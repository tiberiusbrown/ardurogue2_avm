#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "status.hpp"

#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0;
uint8_t avm_test_button_index = 0;
void (*avm_test_text_hook)(int16_t, int16_t, const char*) = nullptr;

namespace rogue {
Ui ui = {};
bool can_see(Position) { return true; }
bool player_can_see_monster(const Monster&) { return true; }
void status_clear() { reset_status_position(); }
void restore_play_render() {}
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

struct Glyph { int16_t x, y; char c; };
static std::vector<Glyph> glyphs;
static unsigned more_count;
static void capture_glyphs(int16_t x, int16_t y, const char* text)
{
    if(x == 128) return; // Width measurement is off-screen.
    if(!std::strcmp(text, "[more]")) { ++more_count; return; }
    if(std::strlen(text) > 4) {
        std::fprintf(stderr, "status draw exceeded four characters: %s\n", text);
        std::exit(1);
    }
    while(*text) {
        glyphs.push_back({x, y, *text++});
        x += 4;
    }
}

static void begin_capture()
{
    rogue::reset_status_position();
    rogue::status_suffix(0);
    glyphs.clear();
    more_count = 0;
    avm_test_text_hook = capture_glyphs;
}

// Independent lexical layout oracle for the fixed-width native test font.
static void expect_status(const char* expected)
{
    std::vector<Glyph> reference;
    int16_t x = 67, y = 28;
    unsigned pages = 0;
    while(*expected) {
        while(*expected == ' ') ++expected;
        const char* end = expected;
        while(*end && *end != ' ') ++end;
        if(end == expected) break;
        int16_t width = static_cast<int16_t>((end - expected) * 4);
        if(x != 67) {
            if(x + 4 + width > 128) {
                x = 67;
                y += 7;
                if(y > 56) { y = 28; ++pages; }
            } else x += 4;
        }
        while(expected != end) {
            reference.push_back({x, y, *expected++});
            x += 4;
        }
    }
    avm_test_text_hook = nullptr;
    bool ok = glyphs.size() == reference.size() && more_count == pages &&
        rogue::status_baseline() == y;
    for(size_t i = 0; ok && i < reference.size(); ++i)
        ok = glyphs[i].x == reference[i].x && glyphs[i].y == reference[i].y &&
             glyphs[i].c == reference[i].c;
    if(!ok) {
        std::fprintf(stderr, "streaming status layout mismatch (%zu/%zu glyphs, %u/%u pages)\n",
                     glyphs.size(), reference.size(), more_count, pages);
        std::exit(1);
    }
}

static void check_streaming_status()
{
    using namespace rogue;
    const char* cases[] = {"a", "abcd", "abcde", "abcdefgh", "abcdefghi",
        "extraordinarilylongword", "alpha beta gamma", "  alpha   beta  ",
        "", "   ", "alpha beta gamma delta epsilon zeta"};
    for(const char* text : cases) {
        begin_capture();
        status_words(text);
        expect_status(text);
    }
    const char* source = "  abcd   abcde  abcdefgh abcdefghi  ";
    begin_capture();
    const char* next = status_word(source);
    if(next != source + 9) std::exit(1);
    next = status_word(next);
    if(next != source + 16) std::exit(1);
    next = status_word(next);
    if(next != source + 25) std::exit(1);
    if(status_word(next)) std::exit(1);
    expect_status(source);

    begin_capture();
    status_suffix('?');
    status_capitalize();
    if(status_word("") || status_word("    ")) std::exit(1);
    status_words("   food    lowerCase");
    expect_status("Food? lowerCase");

    begin_capture();
    status_capitalize();
    status_words("");
    status_words("  abcdefghi beta");
    expect_status("Abcdefghi beta");

    begin_capture();
    status_suffix('!');
    status_suffix('?');
    status_word("food");
    status_word("x");
    expect_status("food? x");

    begin_capture();
    status_word("abcdefghij");
    status_suffix('?');
    status_word("food"); // Suffix forces the entire word onto the next line.
    expect_status("abcdefghij food?");

    begin_capture();
    status_capitalize();
    status_suffix('!');
    status_word("abcdefghijk");
    expect_status("Abcdefghijk!");

    // Pending modifiers also survive pagination; the initiating A is released
    // before a fresh press advances [more].
    for(uint8_t i = 0; i < 16; ++i)
        avm_test_buttons[i] = i & 1 ? AVM_BUTTON_A : 0;
    avm_test_button_count = 16;
    avm_test_button_index = 0;
    avm_test_buttons[2] = AVM_BUTTON_A; // A remains down for previous_buttons.
    ui.held_direction = AVM_BUTTON_R;
    begin_capture();
    for(uint8_t i = 0; i < 10; ++i) status_word("alpha");
    status_suffix('?');
    status_capitalize();
    status_word("  food");
    expect_status("alpha alpha alpha alpha alpha alpha alpha alpha alpha alpha Food?");
    if(more_count != 1 || ui.previous_buttons != AVM_BUTTON_A ||
       ui.held_direction != 0 || !ui.repeat_suppressed) std::exit(1);
    avm_test_button_count = 0;

    // Exercise all uint8_t digit boundaries, and suffix/capitalization consumption.
    for(unsigned n : {0u, 1u, 9u, 10u, 99u, 100u, 255u}) {
        begin_capture();
        status_capitalize();
        status_number(static_cast<uint8_t>(n), '.');
        status_word("food");
        std::string expected = std::to_string(n) + ". food";
        expect_status(expected.c_str());
    }
}

static void check_status_item(rogue::Item item, char suffix, const char* expected,
                              bool capitalize = false)
{
    begin_capture();
    if(capitalize) rogue::status_capitalize();
    rogue::status(item, suffix);
    expect_status(expected);
}

static void check_item_status()
{
    using namespace rogue;
    check_status_item({FOOD, 1}, '?', "the food?");
    check_status_item({FOOD, 1}, '.', "Some food.", true);
    check_status_item({FOOD, 3}, '?', "the 3 food rations?");
    check_status_item({FOOD, 3}, '.', "3 food rations.");
    other_known[POTION_HEALING] = false;
    check_status_item({POTION_HEALING, 1}, '?', "the red potion?");
    check_status_item({POTION_HEALING, 3}, '.', "3 red potions.");
    other_known[POTION_HEALING] = true;
    check_status_item({POTION_HEALING, 1}, '?', "the potion of healing?");
    check_status_item({POTION_HEALING, 1}, '.', "a potion of healing.");
    other_known[SCROLL_REMOVE_CURSE] = false;
    check_status_item({SCROLL_REMOVE_CURSE, 1}, '?', "the faded scroll?");
    other_known[SCROLL_REMOVE_CURSE] = true;
    check_status_item({SCROLL_REMOVE_CURSE, 1}, '?', "the scroll of remove curse?");
    Item sword = make_equipment(LONG_SWORD, 2);
    check_status_item(sword, '?', "the long sword?");
    sword.info |= ITEM_IDENTIFIED;
    check_status_item(sword, '?', "the long sword +2?");
    set_equipment_enchant(sword, -1);
    check_status_item(sword, '.', "a long sword -1.");
    Item armor = make_equipment(CHAIN_MAIL, 2);
    armor.info |= ITEM_IDENTIFIED;
    check_status_item(armor, '?', "the chain mail +2?");
    check_status_item({YENDOR_AMULET, 1}, '?', "the amulet?");
    other_known[RING_SEE_INVISIBLE] = false;
    check_status_item({RING_SEE_INVISIBLE, 1}, '?', "the diamond ring?");
    other_known[RING_SEE_INVISIBLE] = true;
    check_status_item({RING_SEE_INVISIBLE, 1}, '?', "the ring of see invisible?");
    other_known[AMULET_VAMPIRE] = true;
    check_status_item({AMULET_VAMPIRE, 1}, '?', "the amulet of the vampire?");
    known[WAND_TELEPORT - WAND_FORCE] = true;
    Item longest{WAND_TELEPORT, ITEM_IDENTIFIED | 15};
    set_wand_modifier(longest, WAND_OVERPOWERED);
    check_status_item(longest, '?', "the overpowered wand of teleportation?");
    check_status_item(longest, '.', "an overpowered wand of teleportation.");
    known[WAND_FIRE - WAND_FORCE] = false;
    check_status_item({WAND_FIRE, 1}, '?', "the thick wand?");
    begin_capture();
    status_words("Pick up");
    status(Item{FOOD, 1}, '?');
    expect_status("Pick up the food?");
    begin_capture();
    Monster lord{{1, 1}, LORD, 112, 0, {0, 0}, 0};
    status(lord, '!');
    expect_status("the Lord of Darkness!");
    begin_capture();
    status("   alpha beta   ", '?');
    expect_status("alpha beta?");
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

static void check_equipment_roster_text()
{
    using namespace rogue;
    const uint8_t types[] = {DAGGER, SPEAR, LONG_SWORD, MACE, TWO_HANDED_SWORD,
        LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, CHAIN_MAIL, SPLINT_MAIL, PLATE_MAIL,
        STORMBRINGER, GLASS_SWORD, HAMMER_OF_RUIN, DRAGONHIDE, TITAN_PLATE};
    const char* names[] = {"dagger", "spear", "long sword", "mace", "two-handed sword",
        "leather armor", "ring mail", "scale mail", "chain mail", "splint mail", "plate mail",
        "Stormbringer", "Glass Sword", "Hammer of Ruin", "Dragonhide", "Titan Plate"};
    for(unsigned i = 0; i < sizeof types; ++i) {
        for(int8_t enchant = -5; enchant <= 5; ++enchant) {
            for(unsigned cursed = 0; cursed < 2; ++cursed) {
                Item item = make_equipment(types[i], enchant);
                if(cursed) item.info |= ITEM_CURSED;
                char label[ITEM_TEXT_CAPACITY], expected[ITEM_TEXT_CAPACITY];
                format_item(item, label);
                if(std::strcmp(label, names[i])) std::exit(1);
                check_drawn_item(item);
                item.info |= ITEM_IDENTIFIED;
                std::string name = cursed ? "cursed " : "";
                name += names[i];
                if(enchant) std::snprintf(expected, sizeof expected, "%s %+d", name.c_str(), int(enchant));
                else std::snprintf(expected, sizeof expected, "%s", name.c_str());
                format_item(item, label);
                if(std::strcmp(label, expected)) {
                    std::fprintf(stderr, "equipment label: %s != %s\n", label, expected);
                    std::exit(1);
                }
                check_drawn_item(item);
                std::string message = is_weapon(types[i]) ? "a " : "";
                message += expected;
                message += '.';
                check_status_item(item, '.', message.c_str());
                message = "the ";
                message += expected;
                message += '?';
                check_status_item(item, '?', message.c_str());
            }
        }
    }
}

static void check_cursed_equipment()
{
    using namespace rogue;
    const uint8_t types[] = {LONG_SWORD, CHAIN_MAIL, RING_SEE_INVISIBLE, AMULET_SPEED};
    const char* labels[] = {
        "cursed long sword +3", "cursed chain mail +3",
        "cursed ring of see invisible", "cursed amulet of speed"
    };
    const char* messages[] = {
        "a cursed long sword +3.", "cursed chain mail +3.",
        "a cursed ring of see invisible.", "a cursed amulet of speed."
    };
    const char* prompts[] = {
        "the cursed long sword +3?", "the cursed chain mail +3?",
        "the cursed ring of see invisible?", "the cursed amulet of speed?"
    };
    for(unsigned i = 0; i < 4; ++i) {
        Item item{types[i], ITEM_IDENTIFIED | ITEM_CURSED | 3};
        if(is_equipment(item.type)) {
            item = make_equipment(item.type, 3);
            item.info |= ITEM_IDENTIFIED | ITEM_CURSED;
        }
        other_known[item.type] = true;
        char text[ITEM_TEXT_CAPACITY];
        format_item(item, text);
        if(std::strcmp(text, labels[i])) {
            std::fprintf(stderr, "wrong cursed equipment label: %s\n", text);
            std::exit(1);
        }
        check_drawn_item(item);
        check_status_item(item, '.', messages[i]);
        check_status_item(item, '?', prompts[i]);

        // Learning a jewelry type from another item must not reveal this curse.
        item.info &= static_cast<uint8_t>(~ITEM_IDENTIFIED);
        format_item(item, text);
        if(std::strstr(text, "cursed")) std::exit(1);
        check_drawn_item(item);
    }
    other_known[AMULET_SPEED] = false;
    check_status_item({AMULET_SPEED, ITEM_CURSED | 3}, '.', "a diamond amulet.");
    check_status_item({AMULET_SPEED, ITEM_IDENTIFIED | ITEM_CURSED | 3}, '.',
                      "a cursed diamond amulet.");
    // A curse prefix still belongs to the item when its value is zero.
    Item sword = make_equipment(LONG_SWORD, 0);
    sword.info |= ITEM_IDENTIFIED | ITEM_CURSED;
    check_status_item(sword, '.', "a cursed long sword.");
    Item armor = make_equipment(CHAIN_MAIL, 0);
    armor.info |= ITEM_IDENTIFIED | ITEM_CURSED;
    check_status_item(armor, '?', "the cursed chain mail?");
    for(uint8_t type : {LONG_SWORD, CHAIN_MAIL}) {
        for(int8_t enchant : {-5, -2, 0, 2, 5}) {
            Item item = make_equipment(type, enchant);
            item.info |= ITEM_IDENTIFIED | ITEM_CURSED;
            char label[ITEM_TEXT_CAPACITY];
            char expected[ITEM_TEXT_CAPACITY];
            const char* name = is_weapon(type) ? "cursed long sword" : "cursed chain mail";
            if(enchant) std::snprintf(expected, sizeof expected, "%s %+d", name, int(enchant));
            else std::snprintf(expected, sizeof expected, "%s", name);
            format_item(item, label);
            if(std::strcmp(label, expected)) std::exit(1);
            check_drawn_item(item);
            std::string message = is_weapon(type) ? "a " : "";
            message += expected;
            message += '.';
            check_status_item(item, '.', message.c_str());
        }
    }
}

static void check_artifact_jewelry()
{
    using namespace rogue;
    const uint8_t types[] = {RING_INVISIBILITY, RING_REPRISAL, RING_HUNT, AMULET_PHOENIX_HEART, AMULET_HEART_OF_GIANT};
    const char* names[] = {"Ring of Invisibility", "Ring of Reprisal", "Ring of the Hunt", "Phoenix Heart", "Heart of the Giant"};
    for(unsigned i = 0; i < sizeof types; ++i) {
        for(bool cursed : {false, true}) {
            Item item{types[i], static_cast<uint8_t>(1 | (cursed ? ITEM_CURSED : 0))};
            char text[ITEM_TEXT_CAPACITY]; other_known[item.type] = false;
            format_item(item, text);
            if(std::strcmp(text, is_ring(item.type) ? "diamond ring" : "diamond amulet")) std::exit(1);
            check_drawn_item(item);
            other_known[item.type] = true; item.info |= ITEM_IDENTIFIED;
            std::string expected = cursed ? "cursed " : ""; expected += names[i];
            format_item(item, text);
            if(text != expected) std::exit(1);
            check_drawn_item(item);
            std::string message = "a " + expected + ".";
            check_status_item(item, '.', message.c_str());
            message = "the " + expected + "?"; check_status_item(item, '?', message.c_str());
        }
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
    for(uint8_t n:{1,63,64,127,128,254,255}) {
        Item arrows{ARROWS,n};
        format_item(arrows,text);
        char expected[32]; std::snprintf(expected,sizeof expected,"%u %s",n,n==1 ? "arrow" : "arrows");
        if(std::strcmp(text,expected)) { std::fprintf(stderr,"arrow quantity text: %s\n",text); return 1; }
        check_drawn_item(arrows);
    }
    check_streaming_status();
    check_item_status();
    check_equipment_roster_text();
    check_cursed_equipment();
    check_artifact_jewelry();
    check_status_paging();
    std::puts("streaming status and item text passed");
    return 0;
}
