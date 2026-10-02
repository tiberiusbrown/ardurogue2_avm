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
    // The last slot must remain available for the Lord's Yendor drop.
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].item.type == NO_ITEM &&
           !(i == 15 && game.floor == FLOORS - 1 && !game.has_amulet &&
             game.monsters[MONSTERS - 1].type == LORD))
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
    // Keep the player's item in the slot being collected.
    game.ground[index].item = outgoing;
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

// Keep this position check out of use_inventory's frame so nested status text fits
// within the 256-byte VM stack above the framebuffer.
__attribute__((noinline)) static bool legal_teleport_position(Position pos,
                                                              bool player)
{
    return !blocked(pos.x, pos.y) && monster_at(pos) == NONE &&
        (player || pos != game.player) &&
        pos != game.up && pos != game.down &&
        (!player || item_at(pos) == NONE);
}

static bool teleport_player();

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
               !is_ring(target.type) && !is_amulet(target.type) &&
               !is_wand(target.type)) {
                status(F("Nothing happens."));
            } else {
                uint8_t value = item_value(target);
                if(is_wand(target.type)) {
                    set_item_value(target, value > 11 ? 15 : value + 4);
                } else if(item_is_cursed(target) &&
                   (is_ring(target.type) || is_amulet(target.type))) {
                    if(value) set_item_value(target, value - 1);
                } else if(value < ITEM_VALUE_MASK) {
                    set_item_value(target, value + 1);
                }
                if(target.type == ARMOR && game.armor_slot == target_slot)
                    game.defense = item_value(target);
                status(F("The")); status(target); status(F("glows blue."));
            }
        } else if(is_wand(target.type) && wand_afflicted(target)) {
            set_wand_modifier(target, WAND_NORMAL);
            status(F("The")); status(target); status(F("glows white."));
        } else if(!is_wand(target.type) && item_is_cursed(target)) {
            target.info &= static_cast<uint8_t>(~ITEM_CURSED);
            status(F("The")); status(target); status(F("glows white."));
        } else {
            status(F("Nothing happens."));
        }
        return;
    }
    if(type == SCROLL_TELEPORT) {
        teleport_player();
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

static bool teleport_player()
{
    for(uint8_t attempt = 0; attempt < 100; ++attempt) {
        Position pos = {
            static_cast<uint8_t>(next_random(game.random_state) % MAP_W),
            static_cast<uint8_t>(next_random(game.random_state) % MAP_H)};
        if(legal_teleport_position(pos, true)) {
            game.player = pos;
            status(F("You teleport!"));
            return true;
        }
    }
    status(F("Nothing happens."));
    return false;
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

    uint8_t hit = scan_ray(game.player, dx, dy, 8).monster;
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

static bool teleport_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    for(uint8_t attempt = 0; attempt < 100; ++attempt) {
        Position pos = {
            static_cast<uint8_t>(next_random(game.random_state) % MAP_W),
            static_cast<uint8_t>(next_random(game.random_state) % MAP_H)};
        if(!legal_teleport_position(pos, false))
            continue;
        target.pos = pos;
        set_monster_effect(target, MON_CONFUSED, 8);
        monster_status(target, F("disappears!"));
        return true;
    }
    status(F("Nothing happens."));
    return false;
}

static void force_monster(uint8_t index, int8_t dx, int8_t dy,
                          bool powerful)
{
    Monster& target = game.monsters[index];
    monster_status(target, F("is blasted back!"));
    RayResult path = scan_ray(target.pos, dx, dy, powerful ? 16 : 8);
    target.pos = path.monster != NONE ? path.before : path.end;
    uint8_t stun = powerful ? 8 : 4;
    if(path.monster != NONE) {
        monster_status(target, F("crashes into the"));
        status(static_cast<MonsterType>(game.monsters[path.monster].type), '!');
        target.stun = stun;
        game.monsters[path.monster].stun = stun;
    } else if(path.blocker) {
        monster_status(target, F("hits a wall!"));
        target.stun = stun;
    }
}

static bool in_wand_area(Position pos, Position center)
{
    int16_t dx = static_cast<int16_t>(pos.x) - center.x;
    int16_t dy = static_cast<int16_t>(pos.y) - center.y;
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

static void polymorph_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    if(target.type <= BAT || target.type >= LORD) {
        status(F("Nothing happens."));
        return;
    }
    target.state |= MON_AGGRO;
    monster_status(target, F("changes form!"));
    target.type = static_cast<uint8_t>(target.type +
        (target.type != ANGEL && roll(4) == 0 ? 1 : -1));
    target.hp = monster_info(target.type).health;
    target.stun = 0;
    target.effects[0] = target.effects[1] = 0;
}

static void dig_ray(int8_t dx, int8_t dy, bool powerful)
{
#if defined(AVM_DIG_RAY_COMPILER_REPRO)
    // Retained only to reproduce the AVM backend crash documented in
    // tests/compiler_repros/README.md. Never enabled in the game build.
    for(uint8_t step = 1; step <= 6; ++step) {
        for(int8_t side = powerful ? -1 : 0;
            side <= (powerful ? 1 : 0); ++side) {
            int16_t x = static_cast<int16_t>(game.player.x) + dx * step +
                (dy ? side : 0);
            int16_t y = static_cast<int16_t>(game.player.y) + dy * step +
                (dx ? side : 0);
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            Position pos = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
            carve(pos.x, pos.y);
            explore(pos);
            uint8_t door = door_at(pos);
            if(door != NONE) open_door(door);
        }
    }
#else
    for(uint8_t step = 1; step <= 6; ++step) {
        uint8_t lanes = powerful ? 3 : 1;
        for(uint8_t lane = 0; lane < lanes; ++lane) {
            int16_t side = powerful ? static_cast<int16_t>(lane) - 1 : 0;
            int16_t x = static_cast<int16_t>(game.player.x) + dx * step;
            int16_t y = static_cast<int16_t>(game.player.y) + dy * step;
            if(dx) y += side;
            else x += side;
            if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            Position pos = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
            carve(pos.x, pos.y);
            explore(pos);
            uint8_t door = door_at(pos);
            if(door != NONE) open_door(door);
        }
    }
#endif
}

static void resolve_wand_ray(uint8_t type, Position end, uint8_t hit,
                             int8_t dx, int8_t dy, bool powerful)
{
    if(powerful && (type == WAND_TELEPORT || type == WAND_ICE ||
                    type == WAND_POLYMORPH)) {
        bool found = false;
        for(uint8_t i = 0; i < MONSTERS; ++i) {
            Monster& target = game.monsters[i];
            if(!target.type || !in_wand_area(target.pos, end)) continue;
            found = true;
            target.state |= MON_AGGRO;
            if(type == WAND_TELEPORT) teleport_monster(i);
            else if(type == WAND_POLYMORPH) polymorph_monster(i);
            else {
                set_monster_effect(target, MON_SLOWED, 15);
                damage_monster(i, static_cast<uint8_t>(8 + roll(8)), true);
                if(target.type) monster_status(target, F("slows down!"));
            }
        }
        if(!found) status(F("Nothing happens."));
        return;
    }
    if(hit == NONE) { status(F("Nothing happens.")); return; }
    Monster& target = game.monsters[hit];
    if(!target.type) return;
    target.state |= MON_AGGRO;
    switch(type) {
    case WAND_FORCE:
        force_monster(hit, dx, dy, powerful);
        break;
    case WAND_TELEPORT:
        teleport_monster(hit);
        break;
    case WAND_STRIKING:
        damage_monster(hit, static_cast<uint8_t>(
            powerful ? 24 + roll(24) : 12 + roll(12)), true);
        if(target.type) monster_status(target, F("is struck!"));
        break;
    case WAND_ICE:
        set_monster_effect(target, MON_SLOWED, 15);
        damage_monster(hit, static_cast<uint8_t>(8 + roll(8)), true);
        if(target.type) monster_status(target, F("slows down!"));
        break;
    case WAND_POLYMORPH:
        polymorph_monster(hit);
        break;
    default: break;
    }
}

static const int8_t PROGMEM wand_directions[] = {0, -1, 1, 0, 0, 1, -1, 0};

static void force_player()
{
    uint8_t direction = roll(4);
    int8_t dx = wand_directions[direction * 2];
    int8_t dy = wand_directions[direction * 2 + 1];
    RayResult path = scan_ray(game.player, dx, dy, 8);
    game.player = path.monster != NONE ? path.before : path.end;
    if(path.blocker || path.monster != NONE) {
        if(amulet_bonus(AMULET_IRONBLOOD) <= 0 && game.paralyzed < 4)
            game.paralyzed = 4;
        if(path.monster != NONE && game.monsters[path.monster].stun < 4)
            game.monsters[path.monster].stun = 4;
        status(F("You crash into an obstacle!"));
    } else status(F("You are blasted back!"));
}

static void cursed_wand_effect(uint8_t type)
{
    switch(type) {
    case WAND_FORCE: force_player(); break;
    case WAND_TELEPORT: teleport_player(); break;
    case WAND_DIGGING:
        status(F("The wand digs into you!"));
        hurt_player(static_cast<uint8_t>(12 + roll(12)));
        break;
    case WAND_FIRE:
        animate_fire_burst(game.player);
        fire_burst_damage(game.player, true);
        break;
    case WAND_STRIKING:
        status(F("The wand strikes you!"));
        hurt_player(static_cast<uint8_t>(12 + roll(12)));
        break;
    case WAND_ICE:
        status(F("The wand freezes you!"));
        game.slowed = static_cast<uint8_t>(8 + roll(8));
        hurt_player(static_cast<uint8_t>(8 + roll(8)));
        break;
    case WAND_POLYMORPH:
        status(F("Your form twists!"));
        switch(roll(4)) {
        case 0: if(game.weakened < 3) ++game.weakened; break;
        case 1:
            if(amulet_bonus(AMULET_CLARITY) <= 0)
                game.confused = static_cast<uint8_t>(8 + roll(8));
            break;
        case 2: game.slowed = static_cast<uint8_t>(8 + roll(8)); break;
        case 3:
            if(amulet_bonus(AMULET_IRONBLOOD) <= 0)
                game.paralyzed = static_cast<uint8_t>(3 + roll(4));
            break;
        }
        break;
    default: break;
    }
}

__attribute__((noinline)) static void animate_all_wand_rays()
{
    uint8_t steps[4];
    for(uint8_t i = 0; i < 4; ++i)
        steps[i] = scan_ray(game.player, wand_directions[i * 2],
                            wand_directions[i * 2 + 1], 6).steps;
    animate_spreading_rays(game.player, steps);
}

__attribute__((noinline)) static void resolve_all_fire_rays(bool powerful)
{
    Position ends[4];
    for(uint8_t i = 0; i < 4; ++i)
        ends[i] = scan_ray(game.player, wand_directions[i * 2],
                           wand_directions[i * 2 + 1], 6).end;
    animate_fire_bursts(ends, 4, powerful);
    for(uint8_t i = 0; i < 4 && !session.ended; ++i)
        fire_burst_damage(ends[i], true, powerful ? 2 : 1);
}

__attribute__((noinline)) static void resolve_all_other_rays(
    uint8_t type, bool powerful)
{
    Position ends[4];
    uint8_t hits[4];
    for(uint8_t i = 0; i < 4; ++i) {
        RayResult ray = scan_ray(game.player, wand_directions[i * 2],
                                 wand_directions[i * 2 + 1], 6);
        ends[i] = ray.end;
        hits[i] = ray.monster;
    }
    for(uint8_t i = 0; i < 4 && !session.ended; ++i)
        resolve_wand_ray(type, ends[i], hits[i], wand_directions[i * 2],
                         wand_directions[i * 2 + 1], powerful);
}

__attribute__((noinline)) static void single_wand_ray(
    uint8_t type, int8_t dx, int8_t dy, bool powerful)
{
    RayResult ray = scan_ray(game.player, dx, dy, 6);
    animate_ray(game.player, dx, dy, ray.steps);
    if(type == WAND_FIRE) {
        if(powerful) animate_fire_bursts(&ray.end, 1, true);
        else animate_fire_burst(ray.end);
        fire_burst_damage(ray.end, true, powerful ? 2 : 1);
    } else resolve_wand_ray(type, ray.end, ray.monster, dx, dy, powerful);
}

bool use_wand(uint8_t slot, int8_t dx, int8_t dy)
{
    if(slot >= INVENTORY || game.paralyzed ||
       !is_wand(game.inventory[slot].type))
        return false;
    Item& item = game.inventory[slot];
    if(wand_needs_direction(item) &&
       ((dx == 0 && dy == 0) || (dx != 0 && dy != 0) ||
        dx < -1 || dx > 1 || dy < -1 || dy > 1))
        return false;
    if(!wand_charges(item)) {
        status(F("The wand has no charges."));
        return false;
    }
    uint8_t type = item.type;
    WandModifier modifier = wand_modifier(item);
    bool known = item_type_identified(type);
    bool individual = item_is_identified(item);
    status(F("You use"));
    status(Item{type, item.info}, '.');
    uint8_t remaining = static_cast<uint8_t>(wand_charges(item) - 1);
    set_wand_charges(item, remaining);
    identify_type(type);
    item.info |= ITEM_IDENTIFIED;
    if(!known || !individual) {
        status(F("It is"));
        status(Item{type, item.info}, '.');
    }
    bool spreading = wand_spreads(item);
    bool powerful = wand_powerful(item);
    if(modifier == WAND_CURSED) cursed_wand_effect(type);
    else {
        if(modifier == WAND_UNRELIABLE) {
            uint8_t direction = roll(4);
            dx = wand_directions[direction * 2];
            dy = wand_directions[direction * 2 + 1];
        }
        if(type == WAND_DIGGING) {
            if(spreading) {
                for(uint8_t i = 0; i < 4; ++i)
                    dig_ray(wand_directions[i * 2],
                            wand_directions[i * 2 + 1], powerful);
            } else dig_ray(dx, dy, powerful);
            status(F("The stone gives way."));
        } else if(spreading) {
            animate_all_wand_rays();
            if(type == WAND_FIRE) resolve_all_fire_rays(powerful);
            else resolve_all_other_rays(type, powerful);
        } else single_wand_ray(type, dx, dy, powerful);
    }
    if(!remaining) {
        item.type = NO_ITEM;
        status(F("The wand crumbles to dust."));
        if(session.repeat_slot == slot) session.repeat_slot = NONE;
    }
    if(!session.ended) end_turn();
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
