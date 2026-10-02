#include "game.hpp"
#include "game_internal.hpp"
#include "status.hpp"
#include "world.hpp"
#include <string.h>

namespace rogue {

uint8_t item_at(Position pos)
{
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type && game.ground[i].pos == pos)
            return i;
    return NONE;
}

uint8_t ground_item_before(Position pos, uint8_t before)
{
    if(before > GROUND_ITEMS) before = GROUND_ITEMS;
    while(before) {
        --before;
        if(game.ground[before].item.type && game.ground[before].pos == pos)
            return before;
    }
    return NONE;
}

Item ground_item_info(uint8_t index)
{
    return game.ground[index].item;
}

static bool stackable(uint8_t type)
{
    return type == FOOD || is_potion(type) || is_scroll(type);
}

static bool compatible(Item a, Item b)
{
    return stackable(a.type) && a.type == b.type;
}

static uint8_t stack_space(Item item)
{
    return static_cast<uint8_t>(ITEM_VALUE_MASK - item_value(item));
}

static Item clean_stack(Item item)
{
    item.info &= static_cast<uint8_t>(ITEM_VALUE_MASK | ITEM_IDENTIFIED);
    return item;
}

static void merge_stack(Item& target, Item incoming, uint8_t amount)
{
    set_item_value(target, static_cast<uint8_t>(item_value(target) + amount));
    target.info = static_cast<uint8_t>(target.info & ~ITEM_CURSED);
    target.info |= incoming.info & ITEM_IDENTIFIED;
}

static uint8_t reusable_ground_slot()
{
    // Unmarked empty entries still belong to deterministic floor generation.
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type == NO_ITEM &&
           marked(game.marks[game.floor], TAKEN_ITEMS, i))
            return i;
    return NONE;
}

static uint16_t ground_capacity(Item item)
{
    uint16_t capacity = 0;
    if(!stackable(item.type)) return 0;
    for(const GroundItem& ground : game.ground)
        if(ground.pos == game.player &&
           compatible(ground.item, item))
            capacity += stack_space(ground.item);
    return capacity;
}

static uint8_t merge_ground(Item item, uint8_t amount)
{
    for(GroundItem& ground : game.ground) {
        if(!amount) break;
        if(ground.pos != game.player ||
           !compatible(ground.item, item)) continue;
        uint8_t moved = amount < stack_space(ground.item)
            ? amount : stack_space(ground.item);
        if(moved) merge_stack(ground.item, item, moved);
        amount = static_cast<uint8_t>(amount - moved);
    }
    return amount;
}

static bool add_inventory(Item incoming)
{
    // Decide whether the complete pickup fits before touching any stack.
    uint16_t capacity = 0;
    uint8_t empty = NONE;
    for(uint8_t i = 0; i < INVENTORY; ++i) {
        const Item& item = game.inventory[i];
        if(item.type == NO_ITEM && empty == NONE) empty = i;
        if(compatible(item, incoming)) capacity += stack_space(item);
    }
    if(stackable(incoming.type)) {
        if(capacity + (empty == NONE ? 0 : ITEM_VALUE_MASK) <
           item_value(incoming)) return false;
        uint8_t remaining = item_value(incoming);
        for(Item& item : game.inventory) {
            if(!remaining) break;
            if(!compatible(item, incoming)) continue;
            uint8_t moved = remaining < stack_space(item)
                ? remaining : stack_space(item);
            if(moved) merge_stack(item, incoming, moved);
            remaining = static_cast<uint8_t>(remaining - moved);
        }
        if(remaining) {
            Item& target = game.inventory[empty];
            target = clean_stack(incoming);
            set_item_value(target, remaining);
        }
        return true;
    }
    if(empty == NONE) return false;
    game.inventory[empty] = incoming;
    return true;
}

PickupResult take_item(uint8_t index)
{
    if(index >= GROUND_ITEMS || game.ground[index].item.type == NO_ITEM)
        return PICKUP_INVALID;
    GroundItem& ground = game.ground[index];
    Item item = ground.item;
    if(item.type == YENDOR_AMULET) {
        game.has_amulet = 1;
        game.score += 100;
        status(F("You found the amulet!"));
    } else if(add_inventory(item)) {
        status(F("You picked up"));
        status(item, '.');
    } else {
        status(F("Your pack is full."));
        return PICKUP_NEEDS_SWAP;
    }
    mark(game.marks[game.floor], TAKEN_ITEMS, index);
    ground.item.type = NO_ITEM;
    end_turn();
    return PICKUP_TAKEN;
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

static bool remove_equipment_slot(uint8_t slot)
{
    if(!item_is_equipped(slot)) return false;
    bool was_invisible = player_is_invisible();
    clear_equipment_slot(slot);
    return was_invisible && !player_is_invisible();
}

bool inventory_item_removable(uint8_t slot)
{
    if(slot >= INVENTORY || game.inventory[slot].type == NO_ITEM ||
       game.inventory[slot].type == YENDOR_AMULET) return false;
    return !item_is_equipped(slot) || !item_is_cursed(game.inventory[slot]);
}

bool swap_ground_item(uint8_t index, uint8_t slot)
{
    if(index >= GROUND_ITEMS || slot >= INVENTORY ||
       game.ground[index].item.type == NO_ITEM ||
       game.ground[index].item.type == YENDOR_AMULET ||
       !inventory_item_removable(slot)) return false;
    Item incoming = game.ground[index].item;
    Item outgoing = game.inventory[slot];
    bool became_visible = remove_equipment_slot(slot);
    game.inventory[slot] = incoming;
    // Keep the player's item in the exact generated slot being collected.
    game.ground[index].item = outgoing;
    mark(game.marks[game.floor], TAKEN_ITEMS, index);
    session.repeat_slot = NONE;
    status(F("You picked up"));
    status(incoming, '.');
    status(F("You leave"));
    status(outgoing);
    status(F("behind."));
    if(became_visible) status(F("You become visible again."));
    end_turn();
    return true;
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
            Position pos = {
                static_cast<uint8_t>(next_random(game.random_state) % MAP_W),
                static_cast<uint8_t>(next_random(game.random_state) % MAP_H)};
            if(!blocked(pos.x, pos.y) && monster_at(pos) == NONE &&
               item_at(pos) == NONE && pos != game.up && pos != game.down) {
                game.player = pos;
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
        for(uint8_t i = 0; i < ROOMS; ++i)
            mark(game.marks[game.floor], VISITED_ROOMS, i);
        status(F("You become aware of your surroundings."));
        return;
    }
    bool found = false;
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        Monster& target = game.monsters[i];
        if(!target.type || !player_can_see_monster(i) ||
           !can_see(target.pos)) continue;
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
    switch(item.type) {
    case SCROLL_IDENTIFY: case SCROLL_ENCHANT: case SCROLL_REMOVE_CURSE:
    case SCROLL_TELEPORT: case SCROLL_MAPPING: case SCROLL_FEAR:
    case SCROLL_TORMENT: case SCROLL_MASS_CONFUSE: case SCROLL_MASS_POISON: {
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
        session.repeat_slot = slot;
        game.weapon_slot = slot;
        identify_item(slot);
        status(F("You equip"));
        status(item, '.');
        break;
    case ARMOR:
        session.repeat_slot = slot;
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
            session.repeat_slot = slot;
            break;
        }
        return false;
    }
    if(!session.ended)
        end_turn();
    if(item.type == NO_ITEM && session.repeat_slot == slot)
        session.repeat_slot = NONE;
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
    int16_t x = game.player.x, y = game.player.y;
    for(uint8_t step = 0; step < 8; ++step) {
        x += dx;
        y += dy;
        if(wall_at(x, y))
            break;
        Position pos = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
        uint8_t door = door_at(pos);
        if(door != NONE && !door_open(door))
            break;
        hit = monster_at(pos);
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
    if(item.type == NO_ITEM && session.repeat_slot == slot)
        session.repeat_slot = NONE;
    return true;
}

DropDisposition drop_disposition(uint8_t slot)
{
    if(slot >= INVENTORY || game.paralyzed ||
       !inventory_item_removable(slot)) return DROP_INVALID;
    Item item = game.inventory[slot];
    uint16_t capacity = ground_capacity(item);
    if(capacity >= item_value(item) && stackable(item.type))
        return DROP_GROUND;
    if(reusable_ground_slot() != NONE) return DROP_GROUND;
    return capacity ? DROP_DISCARD_REST : DROP_DISCARD_ALL;
}

bool drop_inventory(uint8_t slot, bool discard)
{
    if(slot >= INVENTORY || game.paralyzed)
        return false;
    Item& item = game.inventory[slot];
    if(item.type == YENDOR_AMULET) {
        status(F("You cannot drop the amulet of Yendor."));
        return false;
    }
    if(item.type == NO_ITEM)
        return false;
    if(item_is_equipped(slot) && item_is_cursed(item)) {
        status(F("The cursed item cannot be removed."));
        return false;
    }
    DropDisposition disposition = drop_disposition(slot);
    if(disposition == DROP_INVALID ||
       (disposition != DROP_GROUND && !discard)) return false;
    uint8_t ground_slot = reusable_ground_slot();
    Item dropped = item;
    uint8_t remaining = stackable(item.type)
        ? merge_ground(item, item_value(item)) : item_value(item);
    if(remaining && ground_slot != NONE) {
        Item ground_item = stackable(item.type) ? clean_stack(item) : item;
        if(stackable(item.type)) set_item_value(ground_item, remaining);
        game.ground[ground_slot] = {game.player, ground_item};
    }
    bool became_visible = remove_equipment_slot(slot);
    if(session.repeat_slot == slot)
        session.repeat_slot = NONE;
    item.type = NO_ITEM;
    if(disposition == DROP_GROUND || disposition == DROP_DISCARD_REST) {
        status(F("You dropped"));
        status(dropped, '.');
        if(disposition == DROP_DISCARD_REST)
            status(F("The rest is discarded."));
    } else {
        status(F("You discard"));
        status(dropped, '.');
    }
    if(became_visible) status(F("You become visible again."));
    end_turn();
    return true;
}
} // namespace rogue
