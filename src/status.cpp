#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "render.hpp"
#include "status.hpp"
#include <stdio.h>
#include <string.h>

using namespace rogue;

namespace {

static uint8_t status_x = 67, status_y = 28;

int16_t text_width(const char* words)
{
    return avm_draw_text(128, 0, words).x - 128;
}

static char pending_suffix = 0;
static bool pending_capitalize = false;

void status_next_line(uint8_t& x, uint8_t& y)
{
    x = 67;
    y = static_cast<uint8_t>(y + 7);
    if(y <= 56)
        return;
    restore_play_render();
    int16_t more_width = avm_draw_text_P(128, 0, F("[more]")).x - 128;
    avm_draw_text_P(static_cast<int16_t>(128 - more_width), 63, F("[more]"));
    avm_display(false);
    // A that initiated this turn must be released before it can advance a page.
    while(avm_buttons() & AVM_BUTTON_A)
        avm_idle();
    while(!(avm_buttons() & AVM_BUTTON_A))
        avm_idle();
    ui.previous_buttons = avm_buttons();
    ui.held_direction = 0;
    status_clear();
    x = 67;
    y = 28;
}

char capitalized(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

// Pointer retains its address space: RAM loads for AS0, program loads for AS1.
// Only this frame owns text scratch, including while waiting for [more].
template<typename Pointer>
Pointer stream_status_word(Pointer word)
{
    uint8_t x = status_x, y = status_y;
    bool capitalize = pending_capitalize;
    char suffix = pending_suffix;
    while(*word == ' ') ++word;
    if(!*word) return nullptr;

    char buf[5];
    buf[4] = 0;
    int16_t width = 0;
    Pointer scan = word;
    while(*scan && *scan != ' ') {
        uint8_t count = 0;
        while(count < 4 && scan[count] && scan[count] != ' ') {
            buf[count] = scan[count];
            ++count;
        }
        buf[count] = 0;
        if(capitalize && scan == word) buf[0] = capitalized(buf[0]);
        width += text_width(buf);
        scan += count;
    }
    buf[0] = suffix;
    buf[1] = 0;
    if(suffix) width += text_width(buf);
    if(x != 67) {
        buf[0] = ' ';
        width += text_width(buf);
        if(x + width > 128) status_next_line(x, y);
        else x = static_cast<uint8_t>(x + text_width(buf));
    }

    ui.repeat_suppressed = true;
    scan = word;
    while(*scan && *scan != ' ') {
        uint8_t count = 0;
        while(count < 4 && scan[count] && scan[count] != ' ') {
            buf[count] = scan[count];
            ++count;
        }
        buf[count] = 0;
        if(capitalize && scan == word) buf[0] = capitalized(buf[0]);
        x = static_cast<uint8_t>(avm_draw_text(x, y, buf).x);
        scan += count;
    }
    if(suffix) {
        buf[0] = suffix;
        buf[1] = 0;
        x = static_cast<uint8_t>(avm_draw_text(x, y, buf).x);
    }
    status_x = x;
    status_y = y;
    pending_capitalize = false;
    pending_suffix = 0;
    while(*scan == ' ') ++scan;
    return *scan ? scan : nullptr;
}

// A phrase suffix belongs to its last lexical word, including multiword names.
void status_final_words(const char AVM_PROGMEM* words, char suffix)
{
    while(*words == ' ') ++words;
    while(*words) {
        const char AVM_PROGMEM* next = words;
        while(*next && *next != ' ') ++next;
        while(*next == ' ') ++next;
        if(!*next) status_suffix(suffix);
        words = status_word(words);
        if(!words) return;
    }
}

void status_formatted_number(uint8_t value, char sign)
{
    // At most '+255' or '-255', followed by NUL.
    char buf[5];
    snprintf(buf, sizeof buf, sign ? F("%+d") : F("%d"),
             sign == '-' ? -static_cast<int>(value) : static_cast<int>(value));
    status_word(buf);
}

const char AVM_PROGMEM* monster_name(uint8_t type)
{
    switch(type) {
    case BAT: return F("bat");
    case SNAKE: return F("snake");
    case RATTLESNAKE: return F("rattlesnake");
    case ZOMBIE: return F("zombie");
    case GOBLIN: return F("goblin");
    case PHANTOM: return F("phantom");
    case ORC: return F("orc");
    case TARANTULA: return F("tarantula");
    case HOBGOBLIN: return F("hobgoblin");
    case MIMIC: return F("mimic");
    case INCUBUS: return F("incubus");
    case TROLL: return F("troll");
    case GRIFFIN: return F("griffin");
    case DRAGON: return F("dragon");
    case ANGEL: return F("fallen angel");
    case LORD: return F("Lord of Darkness");
    default: return F("foe");
    }
}

void status_entity(uint8_t type, char punctuation = 0)
{
    status_final_words(monster_name(type), punctuation);
}

static const char PROGMEM* const PROGMEM potion_effect_names[] = {
    F("healing"), F("confusion"), F("poison"), F("harming"),
    F("strength"), F("dexterity"), F("paralysis"), F("slowing"),
    F("experience"), F("invisibility")
};
static const char PROGMEM* const PROGMEM potion_color_names[] = {
    F("red"), F("clear"), F("orange"), F("green"), F("blue"),
    F("white"), F("yellow"), F("violet"), F("black"), F("pink")
};
static const char PROGMEM* const PROGMEM ring_names[] = {
    F("see invisible"), F("strength"), F("dexterity"), F("protection"),
    F("fire immunity"), F("attack"), F("sustenance"), F("invisibility")
};
static const char PROGMEM* const PROGMEM amulet_names[] = {
    F("speed"), F("clarity"), F("conservation"), F("regeneration"),
    F("the vampire"), F("ironblood"), F("vitality"), F("wisdom")
};
static const char PROGMEM* const PROGMEM scroll_names[] = {
    F("identify"), F("enchanting"), F("remove curse"),
    F("teleportation"), F("magic mapping"), F("fear"),
    F("torment"), F("mass confusion"), F("mass poison")
};
static const char PROGMEM* const PROGMEM scroll_descriptors[] = {
    F("faded"), F("yellowed"), F("tattered"), F("glowing"),
    F("shimmering"), F("humming"), F("dark"), F("bright"),
    F("brilliant")
};
static const char PROGMEM* const PROGMEM jewel_descriptors[] = {
    F("diamond"), F("ruby"), F("emerald"), F("topaz"),
    F("gold"), F("silver"), F("platinum"), F("iron")
};
static const char PROGMEM* const PROGMEM wand_names[] = {
    F("force"), F("teleportation"), F("digging"), F("fire"),
    F("striking"), F("ice"), F("polymorph")
};
static const char PROGMEM* const PROGMEM wand_modifier_names[] = {
    F(""), F("cursed"), F("unreliable"), F("spreading"),
    F("powerful"), F("overpowered")
};
static const char PROGMEM* const PROGMEM wand_descriptors[] = {
    F("long"), F("short"), F("slender"), F("thick"),
    F("twisted"), F("curved"), F("glossy")
};

const char PROGMEM* ring_name(uint8_t type)
{
    return is_ring(type) ? ring_names[type - RING_FIRST] : F("unknown");
}

const char PROGMEM* amulet_name(uint8_t type)
{
    return is_amulet(type) ? amulet_names[type - AMULET_FIRST] : F("unknown");
}

struct BufferedItemText {
    char* out;
    uint8_t length = 0;

    void append(const char AVM_PROGMEM* words)
    {
        while(*words && length < ITEM_TEXT_CAPACITY - 1)
            out[length++] = *words++;
        out[length] = 0;
    }

    void word(const char AVM_PROGMEM* words)
    {
        if(length) append(F(" "));
        append(words);
    }

    void number(uint8_t value)
    {
        if(length) append(F(" "));
        snprintf_P(out + length, ITEM_TEXT_CAPACITY - length, F("%u"), value);
        length = static_cast<uint8_t>(strlen(out));
    }

    void bonus(int8_t value)
    {
        if(length) append(F(" "));
        snprintf_P(out + length, ITEM_TEXT_CAPACITY - length,
                   F("%+d"), static_cast<int>(value));
        length = static_cast<uint8_t>(strlen(out));
    }

    void words(const char AVM_PROGMEM* words) { word(words); }
    void final_word(const char AVM_PROGMEM* words, char) { word(words); }
    void final_bonus(int8_t value, char) { bonus(value); }
    void final_number(uint8_t value, char) { number(value); }
};

struct DrawItemText {
    int16_t x;
    int16_t y;
    bool has_text = false;

    void space()
    {
        if(has_text)
            x = avm_draw_text_P(x, y, F(" ")).x;
        has_text = true;
    }

    void word(const char AVM_PROGMEM* words)
    {
        space();
        x = avm_draw_text_P(x, y, words).x;
    }

#if defined(__AVM__)
    void word(const char* words)
    {
        space();
        x = avm_draw_text(x, y, words).x;
    }
#endif

    void number(uint8_t value)
    {
        space();
        x = avm_draw_textf_P(x, y, F("%u"), value).x;
    }

    void bonus(int8_t value)
    {
        space();
        x = avm_draw_textf_P(x, y, F("%+d"), static_cast<int>(value)).x;
    }

    void words(const char AVM_PROGMEM* words) { word(words); }
    void final_word(const char AVM_PROGMEM* words, char) { word(words); }
    void final_bonus(int8_t value, char) { bonus(value); }
    void final_number(uint8_t value, char) { number(value); }
};

enum ItemTextStyle : uint8_t { STATUS_ITEM, INVENTORY_ITEM, PROMPT_ITEM };

const char AVM_PROGMEM* article_for(const char AVM_PROGMEM* word)
{
    switch(*word) {
    case 'a': case 'e': case 'i': case 'o': case 'u':
    case 'A': case 'E': case 'I': case 'O': case 'U':
        return F("an");
    default:
        return F("a");
    }
}

struct StatusItemText {
    void word(const char AVM_PROGMEM* word) { status_word(word); }
    void words(const char AVM_PROGMEM* words) { status_words(words); }
    void final_word(const char AVM_PROGMEM* words, char suffix)
    {
        status_final_words(words, suffix);
    }
    void number(uint8_t value) { status_formatted_number(value, false); }
    void bonus(int8_t value) {
        status_formatted_number(static_cast<uint8_t>(value < 0 ? -value : value),
                                value < 0 ? '-' : '+');
    }
    void final_bonus(int8_t value, char suffix)
    {
        status_suffix(suffix);
        bonus(value);
    }
    void final_number(uint8_t value, char suffix)
    {
        status_suffix(suffix);
        number(value);
    }
};

const char AVM_PROGMEM* equipment_name(uint8_t type)
{
    switch(type) {
    case DAGGER: return F("dagger");
    case SPEAR: return F("spear");
    case LONG_SWORD: return F("long sword");
    case MACE: return F("mace");
    case TWO_HANDED_SWORD: return F("two-handed sword");
    case LEATHER_ARMOR: return F("leather armor");
    case RING_MAIL: return F("ring mail");
    case SCALE_MAIL: return F("scale mail");
    case CHAIN_MAIL: return F("chain mail");
    case SPLINT_MAIL: return F("splint mail");
    case PLATE_MAIL: return F("plate mail");
    default: return F("equipment");
    }
}

template<typename Output>
void emit_item(Item item, ItemTextStyle style, Output& text, char suffix = 0)
{
    bool known = item_type_identified(item.type);
    bool cursed = !is_wand(item.type) && item_is_identified(item) &&
        item_is_cursed(item);
    if(is_potion(item.type) || is_scroll(item.type)) {
        const char AVM_PROGMEM* first_word = known
            ? (is_scroll(item.type) ? F("scroll") : F("potion"))
            : (is_scroll(item.type)
                ? scroll_descriptors[item_appearance(item.type)]
                : potion_color_names[item_appearance(item.type)]);
        uint8_t quantity = item_value(item);
        bool plural = quantity > 1 && style != PROMPT_ITEM;
        if(plural) text.number(quantity);
        else if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM) text.word(article_for(first_word));
        if(known) {
            text.word(is_scroll(item.type)
                ? (plural ? F("scrolls") : F("scroll"))
                : (plural ? F("potions") : F("potion")));
            text.word(F("of"));
            text.final_word(is_scroll(item.type)
                ? scroll_names[item.type - SCROLL_FIRST]
                : potion_effect_names[item.type - POTION_FIRST], suffix);
        } else {
            text.word(first_word);
            text.final_word(is_scroll(item.type)
                ? (plural ? F("scrolls") : F("scroll"))
                : (plural ? F("potions") : F("potion")), suffix);
        }
        return;
    }
    if(is_ring(item.type)) {
        const char AVM_PROGMEM* first_word = known
            ? F("ring") : jewel_descriptors[item_appearance(item.type)];
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM)
            text.word(cursed ? F("a") : article_for(first_word));
        if(cursed) text.word(F("cursed"));
        if(known) {
            text.word(first_word);
            text.word(F("of"));
            text.final_word(ring_name(item.type), suffix);
        } else {
            text.word(first_word);
            text.final_word(F("ring"), suffix);
        }
        return;
    }
    if(is_amulet(item.type)) {
        const char AVM_PROGMEM* first_word = known
            ? F("amulet") : jewel_descriptors[item_appearance(item.type)];
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM)
            text.word(cursed ? F("a") : article_for(first_word));
        if(cursed) text.word(F("cursed"));
        if(known) {
            text.word(first_word);
            text.word(F("of"));
            text.final_word(amulet_name(item.type), suffix);
        } else {
            text.word(first_word);
            text.final_word(F("amulet"), suffix);
        }
        return;
    }
    if(is_wand(item.type)) {
        bool individual = known && item_is_identified(item);
        WandModifier modifier = individual ? wand_modifier(item) : WAND_NORMAL;
        const char AVM_PROGMEM* first_word = !known
            ? wand_descriptors[item_appearance(item.type)]
            : modifier == WAND_NORMAL ? F("wand") :
              wand_modifier_names[modifier];
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM) text.word(article_for(first_word));
        if(known) {
            if(modifier != WAND_NORMAL) text.word(first_word);
            text.word(F("wand"));
            text.word(F("of"));
            if(individual && style == INVENTORY_ITEM) {
                text.words(wand_names[item.type - WAND_FIRST]);
                text.final_number(wand_charges(item), suffix);
            } else text.final_word(wand_names[item.type - WAND_FIRST], suffix);
        } else {
            text.word(first_word);
            text.final_word(F("wand"), suffix);
        }
        return;
    }
    if(is_equipment(item.type)) {
        const char AVM_PROGMEM* name = equipment_name(item.type);
        bool has_bonus = item_is_identified(item) && equipment_enchant(item);
        if(style == PROMPT_ITEM) text.word(F("the"));
        else if(style == STATUS_ITEM && is_weapon(item.type))
            text.word(article_for(cursed ? F("cursed") : name));
        if(cursed) text.word(F("cursed"));
        text.final_word(name, has_bonus ? 0 : suffix);
        if(has_bonus) text.final_bonus(equipment_enchant(item), suffix);
        return;
    }
    switch(item.type) {
    case FOOD:
        if(item_value(item) > 1) {
            if(style == PROMPT_ITEM) text.word(F("the"));
            text.number(item_value(item));
            text.final_word(F("food rations"), suffix);
        } else {
            if(style == PROMPT_ITEM) text.word(F("the"));
            else if(style == STATUS_ITEM) text.word(F("some"));
            text.final_word(F("food"), suffix);
        }
        break;
    case YENDOR_AMULET:
        if(style != INVENTORY_ITEM) text.word(F("the"));
        text.final_word(F("amulet"), suffix);
        break;
    default:
        if(style == PROMPT_ITEM) text.word(F("the"));
        text.final_word(F("item"), suffix);
        break;
    }
}

} // namespace

void rogue::format_item(Item item, char (&buffer)[ITEM_TEXT_CAPACITY])
{
    buffer[0] = 0;
    BufferedItemText text{buffer};
    emit_item(item, INVENTORY_ITEM, text);
}

void rogue::draw_item_text(int16_t x, int16_t y, Item item)
{
    DrawItemText text{x, y};
    emit_item(item, INVENTORY_ITEM, text);
}

void rogue::reset_status_position()
{
    status_x = 67;
    status_y = 28;
}

uint8_t rogue::status_baseline()
{
    return status_y;
}

void rogue::status_suffix(char c) { pending_suffix = c; }
void rogue::status_capitalize() { pending_capitalize = true; }

const char* rogue::status_word(const char* word)
{
    return stream_status_word(word);
}

void rogue::status_words(const char* words)
{
    while(words) words = status_word(words);
}

#if defined(__AVM__)
const char AVM_PROGMEM* rogue::status_word(const char AVM_PROGMEM* word)
{
    return stream_status_word(word);
}

void rogue::status_words(const char AVM_PROGMEM* words)
{
    while(words) words = status_word(words);
}
#endif

void rogue::status(const char PROGMEM* words)
{
    status_words(words);
}

void rogue::status(const char PROGMEM* words, char punctuation)
{
    status_final_words(words, punctuation);
}

void rogue::status(Item item)
{
    status(item, 0);
}

void rogue::status(Item item, char punctuation)
{
    StatusItemText text;
    emit_item(item, punctuation == '?' ? PROMPT_ITEM : STATUS_ITEM,
              text, punctuation);
}

void rogue::status(MonsterType monster)
{
    status_entity(monster);
}

void rogue::status(MonsterType monster, char punctuation)
{
    status_entity(monster, punctuation);
}

void rogue::status_number(uint8_t value)
{
    status_number(value, 0);
}

void rogue::status_number(uint8_t value, char punctuation)
{
    status_suffix(punctuation);
    status_formatted_number(value, false);
}
