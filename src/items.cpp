#include "game.hpp"
#include "game_internal.hpp"
#include "status.hpp"
#include "world.hpp"
#include <string.h>

namespace rogue {

uint8_t item_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type && game.ground[i].x == x &&
           game.ground[i].y == y)
            return i;
    return NONE;
}

uint8_t ground_item_before(uint8_t x, uint8_t y, uint8_t before)
{
    if(before > GROUND_ITEMS) before = GROUND_ITEMS;
    while(before) {
        --before;
        if(game.ground[before].item.type && game.ground[before].x == x &&
           game.ground[before].y == y)
            return before;
    }
    return NONE;
}

Item ground_item_info(uint8_t index)
{
    return game.ground[index].item;
}

static bool add_inventory(Item incoming)
{
    if(incoming.type == FOOD || is_potion(incoming.type) ||
       is_scroll(incoming.type)) {
        for(Item& item : game.inventory)
            if(item.type == incoming.type &&
               item_value(item) <= ITEM_VALUE_MASK - item_value(incoming)) {
                set_item_value(item, static_cast<uint8_t>(
                    item_value(item) + item_value(incoming)));
                item.info |= incoming.info & ITEM_IDENTIFIED;
                return true;
            }
    }
    for(Item& item : game.inventory)
        if(item.type == NO_ITEM) {
            item = incoming;
            return true;
        }
    return false;
}

void take_item(uint8_t index)
{
    GroundItem& ground = game.ground[index];
    Item item = ground_item_info(index);
    if(item.type == YENDOR_AMULET) {
        game.has_amulet = 1;
        game.score += 100;
        status(F("You found the amulet!"));
    } else if(add_inventory(item)) {
        status(F("You picked up"));
        status(item, '.');
    } else {
        status(F("Your pack is full."));
        return;
    }
    mark(game.marks[game.floor], TAKEN_ITEMS, index);
    ground.item.type = NO_ITEM;
    end_turn();
}

static bool item_is_equipped(uint8_t slot)
{
    return game.weapon_slot == slot || game.armor_slot == slot ||
        game.amulet_slot == slot || game.ring_slots[0] == slot ||
        game.ring_slots[1] == slot;
}

static void clear_equipment_slot(uint8_t slot)
{
    if(game.weapon_slot == slot)
        game.weapon_slot = NONE;
    if(game.armor_slot == slot) {
        game.armor_slot = NONE;
        game.defense = 0;
    }
    if(game.amulet_slot == slot)
        game.amulet_slot = NONE;
    if(game.ring_slots[0] == slot)
        game.ring_slots[0] = NONE;
    if(game.ring_slots[1] == slot)
        game.ring_slots[1] = NONE;
    uint8_t maximum = player_max_hp();
    if(game.hp > maximum)
        game.hp = maximum;
}

static bool toggle_accessory(uint8_t slot)
{
    Item& item = game.inventory[slot];
    uint8_t* target;
    if(is_amulet(item.type)) {
        target = &game.amulet_slot;
    } else {
        if(game.ring_slots[0] >= INVENTORY)
            target = &game.ring_slots[0];
        else if(game.ring_slots[1] >= INVENTORY)
            target = &game.ring_slots[1];
        else if(item_is_cursed(game.inventory[game.ring_slots[1]]))
            target = &game.ring_slots[0];
        else
            target = &game.ring_slots[1];
    }

    if(item_is_equipped(slot)) {
        if(item_is_cursed(item)) {
            status(F("The cursed item cannot be removed."));
            return false;
        }
        bool was_invisible = player_is_invisible();
        clear_equipment_slot(slot);
        status(F("You take off"));
        status(item, '.');
        if(was_invisible && !player_is_invisible())
            status(F("You become visible again."));
        return true;
    }

    if(*target < INVENTORY) {
        Item& replaced = game.inventory[*target];
        if(item_is_cursed(replaced)) {
            status(F("The cursed item cannot be removed."));
            return false;
        }
        clear_equipment_slot(*target);
    }
    bool was_invisible = player_is_invisible();
    *target = slot;
    identify_item(slot);
    if(item.type == AMULET_CLARITY &&
       !item_is_cursed(item) && game.confused) {
        game.confused = 0;
        status(F("Your mind clears."));
    }
    if(item.type == RING_INVISIBILITY &&
       item_is_cursed(item) && game.invisible) {
        game.invisible = 0;
        status(F("The cursed ring makes you visible."));
    }
    uint8_t maximum = player_max_hp();
    if(game.hp > maximum)
        game.hp = maximum;
    status(F("You put on"));
    status(item, '.');
    if(item_is_cursed(item)) {
        item.info |= ITEM_IDENTIFIED;
        status(F("It is cursed and cannot be removed."));
    }
    if(!was_invisible && player_is_invisible())
        status(F("You fade from sight."));
    return true;
}

static void consume_potion(Item& item)
{
    int8_t conservation = amulet_bonus(AMULET_CONSERVATION);
    if(conservation > 0 && roll(4) == 0) {
        status(F("The amulet preserves the potion."));
        return;
    }
    uint8_t amount = item_value(item);
    if(amount) {
        --amount;
        set_item_value(item, amount);
        if(!amount)
            item.type = NO_ITEM;
    }
    if(conservation < 0 && item.type != NO_ITEM && item_value(item) &&
       roll(4) == 0) {
        amount = static_cast<uint8_t>(item_value(item) - 1);
        set_item_value(item, amount);
        if(!amount)
            item.type = NO_ITEM;
        status(F("The cursed amulet consumes another potion."));
    }
}

static void scroll_effect(uint8_t type, uint8_t target_slot)
{
    if(type == SCROLL_IDENTIFY || type == SCROLL_ENCHANT ||
       type == SCROLL_REMOVE_CURSE) {
        if(target_slot >= INVENTORY ||
           !game.inventory[target_slot].type) {
            status(F("Nothing happens."));
            return;
        }
        Item& target = game.inventory[target_slot];
        if(type == SCROLL_IDENTIFY) {
            identify_item(target_slot);
            status(F("You identify")); status(target, '.');
        } else if(type == SCROLL_ENCHANT) {
            if(target.type != SWORD && target.type != ARMOR &&
               !is_ring(target.type) && !is_amulet(target.type)) {
                status(F("Nothing happens."));
            } else {
                uint8_t value = item_value(target);
                if(item_is_cursed(target) &&
                   (is_ring(target.type) || is_amulet(target.type))) {
                    if(value) set_item_value(target, value - 1);
                } else if(value < ITEM_VALUE_MASK) {
                    set_item_value(target, value + 1);
                }
                if(target.type == ARMOR && game.armor_slot == target_slot)
                    game.defense = item_value(target);
                status(F("The")); status(target); status(F("glows blue."));
            }
        } else if(item_is_cursed(target)) {
            target.info &= static_cast<uint8_t>(~ITEM_CURSED);
            status(F("The")); status(target); status(F("glows white."));
        } else {
            status(F("Nothing happens."));
        }
        return;
    }
    if(type == SCROLL_TELEPORT) {
        for(uint8_t attempt = 0; attempt < 100; ++attempt) {
            uint8_t x = static_cast<uint8_t>(next_random(game.random_state) % MAP_W);
            uint8_t y = static_cast<uint8_t>(next_random(game.random_state) % MAP_H);
            if(!blocked(x, y) && monster_at(x, y) == NONE) {
                game.px = x; game.py = y;
                visit_room();
                status(F("You teleport!"));
                return;
            }
        }
        status(F("Nothing happens."));
        return;
    }
    if(type == SCROLL_MAPPING) {
        memset(game.explored, 0xff, sizeof(game.explored));
        status(F("You become aware of your surroundings."));
        return;
    }
    bool found = false;
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        Monster& target = game.monsters[i];
        if(!target.type || !player_can_see_monster(i) ||
           !can_see(target.x, target.y)) continue;
        found = true;
        target.state |= MON_AGGRO;
        switch(type) {
        case SCROLL_FEAR:
            target.state |= MON_AFRAID;
            monster_status(target, F("flees!"));
            break;
        case SCROLL_TORMENT:
            target.hp = static_cast<uint8_t>(target.hp / 2);
            if(!target.hp) target.hp = 1;
            monster_status(target, F("is stricken!"));
            break;
        case SCROLL_MASS_CONFUSE:
            set_monster_effect(target, MON_CONFUSED, 15);
            monster_status(target, F("becomes confused."));
            break;
        case SCROLL_MASS_POISON:
            set_monster_effect(target, MON_WEAKENED, 15);
            monster_status(target, F("grows weaker."));
            break;
        default: break;
        }
    }
    if(!found) status(F("Nothing happens."));
}

bool use_inventory(uint8_t slot, uint8_t target_slot)
{
    if(slot >= INVENTORY || game.paralyzed)
        return false;
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM)
        return false;
    session.repeat_slot = slot;
    switch(item.type) {
    case SCROLL_IDENTIFY: case SCROLL_ENCHANT: case SCROLL_REMOVE_CURSE:
    case SCROLL_TELEPORT: case SCROLL_MAPPING: case SCROLL_FEAR:
    case SCROLL_TORMENT: case SCROLL_MASS_CONFUSE: case SCROLL_MASS_POISON: {
        session.repeat_slot = NONE;
        uint8_t type = item.type;
        bool known = item_type_identified(type);
        status(F("You read"));
        status(Item{type, 1}, '.');
        identify_type(type);
        if(!known) {
            status(F("It was"));
            status(Item{type, 1}, '.');
        }
        uint8_t count = item_value(item);
        if(count > 1) set_item_value(item, count - 1);
        else item.type = NO_ITEM;
        scroll_effect(type, target_slot);
        break;
    }
    case FOOD:
        game.hunger = game.hunger > 145 ? 255 : game.hunger + 110;
        status(F("You eat"));
        status(Item{item.type, 1}, '.');
        set_item_value(item, static_cast<uint8_t>(item_value(item) - 1));
        if(!item_value(item)) item.type = NO_ITEM;
        break;
    case HEALING: case CONFUSION: case POISON: case HARMING:
    case STRENGTH: case DEXTERITY: case PARALYSIS: case SLOWING:
    case EXPERIENCE: case INVISIBILITY: {
        uint8_t type = item.type;
        bool known = potion_identified(type);
        status(F("You drink"));
        status(Item{item.type, static_cast<uint8_t>(1 |
            (item.info & ITEM_IDENTIFIED))}, '.');
        consume_potion(item);
        identify_type(type);
        item.info |= ITEM_IDENTIFIED;
        if(!known) {
            status(F("It was"));
            status(Item{type, 1}, '.');
        }
        switch(type) {
        case HEALING: {
            uint8_t maximum = player_max_hp();
            uint8_t healed = static_cast<uint8_t>(maximum / 4 +
                roll(static_cast<uint8_t>(maximum / 2 + 1)));
            heal_player(healed);
            if(game.weakened)
                status(F("Your strength returns."));
            game.weakened = 0;
            status(F("You feel better."));
            break;
        }
        case STRENGTH:
            if(game.weakened) {
                game.weakened = 0;
                status(F("Your strength returns."));
            } else if(game.attack < 250) {
                ++game.attack;
                status(F("You feel stronger."));
            }
            break;
        case DEXTERITY:
            if(game.dexterity < 12) ++game.dexterity;
            status(F("You feel more agile."));
            break;
        case EXPERIENCE:
            gain_xp(50);
            break;
        case INVISIBILITY:
            if(ring_bonus(RING_INVISIBILITY) < 0) {
                status(F("The cursed ring keeps you visible."));
            } else {
                if(!player_is_invisible())
                    status(F("You turn invisible."));
                game.invisible = static_cast<uint8_t>(12 + roll(16));
            }
            break;
        case HARMING: {
            uint8_t base = static_cast<uint8_t>(player_max_hp() / 8 + 1);
            uint8_t damage = static_cast<uint8_t>(base + roll(base * 2));
            if(damage > 10) damage = 10;
            game.hp = damage >= game.hp ? 0 : game.hp - damage;
            status(F("The potion harms you!"));
            if(!game.hp) finish(DEATH);
            break;
        }
        case POISON:
            if(!game.weakened)
                status(F("You feel weaker."));
            if(game.weakened < 3) ++game.weakened;
            break;
        case CONFUSION:
            if(amulet_bonus(AMULET_CLARITY) > 0) {
                status(F("Your amulet protects you from confusion."));
            } else {
                if(!game.confused)
                    status(F("You feel confused."));
                game.confused = static_cast<uint8_t>(8 + roll(8));
            }
            break;
        case PARALYSIS:
            if(amulet_bonus(AMULET_IRONBLOOD) > 0) {
                status(F("Your amulet protects you from paralysis."));
            } else {
                if(!game.paralyzed)
                    status(F("You are paralyzed!"));
                game.paralyzed = static_cast<uint8_t>(3 + roll(4));
            }
            break;
        case SLOWING:
            if(!game.slowed)
                status(F("You feel sluggish."));
            game.slowed = static_cast<uint8_t>(8 + roll(8));
            break;
        default: break;
        }
        break;
    }
    case SWORD:
        game.weapon_slot = slot;
        identify_item(slot);
        status(F("You equip"));
        status(item, '.');
        break;
    case ARMOR:
        game.armor_slot = slot;
        game.defense = item_value(item);
        identify_item(slot);
        status(F("You equip"));
        status(item, '.');
        break;
    default:
        if(is_ring(item.type) || is_amulet(item.type)) {
            if(!toggle_accessory(slot))
                return false;
            break;
        }
        return false;
    }
    if(!session.ended)
        end_turn();
    return true;
}

bool throw_potion(uint8_t slot, int8_t dx, int8_t dy)
{
    if(slot >= INVENTORY || game.paralyzed ||
       !is_potion(game.inventory[slot].type) ||
       (dx == 0 && dy == 0) || (dx != 0 && dy != 0) ||
       dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return false;
    Item& item = game.inventory[slot];
    uint8_t type = item.type;
    status(F("You throw"));
    status(Item{type, 1}, '.');
    set_item_value(item, static_cast<uint8_t>(item_value(item) - 1));
    if(!item_value(item)) item.type = NO_ITEM;

    uint8_t hit = NONE;
    int16_t x = game.px, y = game.py;
    for(uint8_t step = 0; step < 8; ++step) {
        x += dx;
        y += dy;
        if(wall_at(x, y))
            break;
        uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        if(door != NONE && !door_open(door))
            break;
        hit = monster_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        if(hit != NONE)
            break;
    }
    if(hit != NONE) {
        status(F("It hits the"));
        status(static_cast<MonsterType>(game.monsters[hit].type), '.');
        bool known = potion_identified(type);
        identify_type(type);
        if(!known) {
            status(F("It was"));
            status(Item{type, 1}, '.');
        }
        apply_monster_potion(type, hit);
    }
    status(F("The potion shatters."));
    if(!session.ended)
        end_turn();
    return true;
}

bool drop_inventory(uint8_t slot)
{
    if(slot >= INVENTORY || game.paralyzed)
        return false;
    Item& item = game.inventory[slot];
    if(item.type == YENDOR_AMULET) {
        status(F("You cannot drop the amulet of Yendor."));
        return false;
    }
    if(item.type == NO_ITEM || item_at(game.px, game.py) != NONE)
        return false;
    if(item_is_equipped(slot) && item_is_cursed(item)) {
        status(F("The cursed item cannot be removed."));
        return false;
    }
    uint8_t ground_slot = NONE;
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type == NO_ITEM &&
           marked(game.marks[game.floor], TAKEN_ITEMS, i)) {
            ground_slot = i;
            break;
        }
    Item dropped = item;
    if(ground_slot != NONE)
        game.ground[ground_slot] = {game.px, game.py, dropped};
    if(item_is_equipped(slot))
        clear_equipment_slot(slot);
    if(session.repeat_slot == slot)
        session.repeat_slot = NONE;
    item.type = NO_ITEM;
    if(ground_slot != NONE) {
        status(F("You dropped"));
        status(dropped, '.');
    } else {
        status(F("It crumbles to dust."));
    }
    end_turn();
    return true;
}
} // namespace rogue
