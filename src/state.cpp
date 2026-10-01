#include "game.hpp"
#include "game_internal.hpp"
#include "status.hpp"
#include "world.hpp"
#include <string.h>

namespace rogue {

#if defined(__AVM__)
Game game __attribute__((section(".saved"))) = {};
#endif
Session session = {NONE, DEATH, false};

uint16_t next_random(uint16_t& state)
{
    uint16_t x = state ? state : 0xace1;
    x ^= static_cast<uint16_t>(x << 7);
    x ^= x >> 9;
    x ^= static_cast<uint16_t>(x << 8);
    state = x;
    return x;
}

int8_t ring_bonus(uint8_t type)
{
    int16_t bonus = 0;
    for(uint8_t i = 0; i < 2; ++i) {
        uint8_t slot = game.ring_slots[i];
        if(slot < INVENTORY) {
            const Item& item = game.inventory[slot];
            if(item.type == type)
                bonus += item_is_cursed(item)
                    ? -item_value(item) : item_value(item);
        }
    }
    if(bonus > 127) bonus = 127;
    if(bonus < -127) bonus = -127;
    return static_cast<int8_t>(bonus);
}

int8_t amulet_bonus(uint8_t type)
{
    uint8_t slot = game.amulet_slot;
    if(slot >= INVENTORY)
        return 0;
    const Item& item = game.inventory[slot];
    if(item.type != type)
        return 0;
    return static_cast<int8_t>(item_is_cursed(item)
        ? -item_value(item) : item_value(item));
}

uint8_t player_max_hp()
{
    int16_t maximum = static_cast<int16_t>(game.max_hp) +
        static_cast<int16_t>(amulet_bonus(AMULET_VITALITY)) * 5 -
        game.vamp_drain;
    if(maximum < 1) maximum = 1;
    if(maximum > 255) maximum = 255;
    return static_cast<uint8_t>(maximum);
}

bool player_is_invisible()
{
    int8_t bonus = ring_bonus(RING_INVISIBILITY);
    return bonus > 0 || (game.invisible && bonus >= 0);
}

bool player_can_see_monster(uint8_t index)
{
    if(index >= MONSTERS || !game.monsters[index].type)
        return false;
    if(ring_bonus(RING_SEE_INVISIBLE) < 0 &&
       ((game.turns + index) & 1))
        return false;
    return (!(monster_info(game.monsters[index].type).flags & MON_NATURAL_INVIS) &&
            !monster_effect(game.monsters[index], MON_INVISIBLE)) ||
           ring_bonus(RING_SEE_INVISIBLE) > 0;
}

void heal_player(uint8_t amount)
{
    if(amulet_bonus(AMULET_REGENERATION) < 0)
        amount = static_cast<uint8_t>((amount + 1) / 2);
    uint8_t maximum = player_max_hp();
    uint16_t hp = static_cast<uint16_t>(game.hp) + amount;
    game.hp = hp > maximum ? maximum : static_cast<uint8_t>(hp);
}

uint8_t roll(uint8_t limit)
{
    return static_cast<uint8_t>(next_random(game.random_state) % limit);
}

bool potion_identified(uint8_t type)
{
    return is_potion(type) && item_type_identified(type);
}

static uint8_t knowledge_index(uint8_t type)
{
    if(is_potion(type)) return static_cast<uint8_t>(type - HEALING);
    if(is_scroll(type)) return static_cast<uint8_t>(POTION_COUNT + type - SCROLL_IDENTIFY);
    if(is_ring(type)) return static_cast<uint8_t>(POTION_COUNT + SCROLL_COUNT + type - RING_SEE_INVISIBLE);
    if(is_amulet(type)) return static_cast<uint8_t>(POTION_COUNT + SCROLL_COUNT + RING_COUNT + type - AMULET_SPEED);
    return NONE;
}

bool item_type_identified(uint8_t type)
{
    uint8_t index = knowledge_index(type);
    return index != NONE && (game.identified_items[index >> 3] &
        (1u << (index & 7))) != 0;
}

uint8_t item_appearance(uint8_t type)
{
    if(is_potion(type)) return game.potion_appearance[type - HEALING];
    if(is_scroll(type)) return game.scroll_appearance[type - SCROLL_IDENTIFY];
    if(is_ring(type)) return game.ring_appearance[type - RING_SEE_INVISIBLE];
    if(is_amulet(type)) return game.amulet_appearance[type - AMULET_SPEED];
    return NONE;
}

uint8_t potion_color(uint8_t type)
{
    return is_potion(type) ? item_appearance(type) : NONE;
}

void identify_type(uint8_t type)
{
    uint8_t index = knowledge_index(type);
    if(index == NONE) return;
    game.identified_items[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
    if(!is_potion(type) && !is_scroll(type)) return;
    for(Item& item : game.inventory)
        if(item.type == type)
            item.info |= ITEM_IDENTIFIED;
    for(GroundItem& ground : game.ground)
        if(ground.item.type == type)
            ground.item.info |= ITEM_IDENTIFIED;
}

void identify_item(uint8_t slot)
{
    if(slot >= INVENTORY || !game.inventory[slot].type) return;
    Item& item = game.inventory[slot];
    item.info |= ITEM_IDENTIFIED;
    identify_type(item.type);
}

static void shuffle_appearances(uint8_t* values, uint8_t count)
{
    for(uint8_t i = 0; i < count; ++i) values[i] = i;
    for(uint8_t i = static_cast<uint8_t>(count - 1); i > 0; --i) {
        uint8_t j = static_cast<uint8_t>(next_random(game.random_state) % (i + 1));
        uint8_t old = values[i];
        values[i] = values[j];
        values[j] = old;
    }
}

void gain_xp(uint8_t amount)
{
    int8_t wisdom = amulet_bonus(AMULET_WISDOM);
    if(wisdom > 0)
        amount = static_cast<uint8_t>(amount + (amount + 1) / 2);
    else if(wisdom < 0)
        amount = static_cast<uint8_t>((amount + 1) / 2);
    uint16_t total = static_cast<uint16_t>(game.xp) + amount;
    while(game.level < 50) {
        uint16_t threshold = static_cast<uint16_t>(4 + game.level * 3);
        if(total < threshold)
            break;
        total -= threshold;
        ++game.level;
        game.max_hp = static_cast<uint8_t>(game.max_hp + 3);
        game.hp = player_max_hp();
        if(game.level % 2 == 0)
            ++game.attack;
        status(F("You gained a level!"));
    }
    game.xp = static_cast<uint8_t>(total);
}

void start_new(uint16_t seed)
{
    uint16_t best = game.best_score;
    memset(&game, 0, sizeof(game));
    game.magic = SAVE_MAGIC;
    game.version = SAVE_VERSION;
    game.valid = 1;
    game.best_score = best;
    game.run_seed = seed ? seed : 0xace1;
    game.random_state = game.run_seed;
    game.hp = game.max_hp = 18;
    game.level = 1;
    game.attack = 2;
    game.dexterity = 4;
    game.speed = 4;
    game.defense = 0;
    game.hunger = 220;
    game.weapon_slot = game.armor_slot = NONE;
    game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    // Independent Fisher-Yates permutations are saved with the run.
    shuffle_appearances(game.potion_appearance, POTION_COUNT);
    shuffle_appearances(game.scroll_appearance, SCROLL_COUNT);
    shuffle_appearances(game.ring_appearance, RING_COUNT);
    shuffle_appearances(game.amulet_appearance, AMULET_COUNT);
    make_floor();
    game.px = game.up_x;
    game.py = game.up_y;
    visit_room();
    session = {NONE, DEATH, false};
}

void finish(RunResult result)
{
    if(game.score > game.best_score)
        game.best_score = game.score;
    game.valid = 0;
    session.ended = true;
    session.result = result;
}

void change_floor(int8_t delta)
{
    game.floor = static_cast<uint8_t>(game.floor + delta);
    make_floor();
    game.px = delta > 0 ? game.up_x : game.down_x;
    game.py = delta > 0 ? game.up_y : game.down_y;
    visit_room();
    status(F("You take the stairs."));
}

bool take_stairs()
{
    if(game.paralyzed) return false;
    if(game.px == game.up_x && game.py == game.up_y) {
        if(game.floor)
            change_floor(-1);
        else
            finish(game.has_amulet ? ESCAPED : RETURNED_EMPTY);
        return true;
    }
    if(game.floor < FLOORS - 1 &&
       game.px == game.down_x && game.py == game.down_y) {
        change_floor(1);
        return true;
    }
    return false;
}

void action()
{
    if(game.paralyzed) {
        status(F("You cannot act!"));
        end_turn();
        return;
    }
    if(session.repeat_slot != NONE && game.inventory[session.repeat_slot].type) {
        use_inventory(session.repeat_slot);
    } else {
        status(F("There is nothing here."));
        end_turn();
    }
}
} // namespace rogue
