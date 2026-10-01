#include "game.hpp"
#include <string.h>

namespace rogue {

Session session = {NONE, 0, false};

static const uint8_t PROGMEM monster_damage[] = {0, 1, 2, 2, 3, 4, 6};

uint8_t roll(uint8_t limit)
{
    return static_cast<uint8_t>(next_random(game.random_state) % limit);
}

bool potion_identified(uint8_t type)
{
    if(!is_potion(type))
        return false;
    uint8_t index = static_cast<uint8_t>(type - HEALING);
    return (game.identified_potions[index >> 3] & (1u << (index & 7))) != 0;
}

uint8_t potion_color(uint8_t type)
{
    return is_potion(type) ? game.potion_appearance[type - HEALING] : NONE;
}

static void identify_potion(uint8_t type)
{
    uint8_t index = static_cast<uint8_t>(type - HEALING);
    game.identified_potions[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

static void gain_xp(uint8_t amount)
{
    uint16_t total = static_cast<uint16_t>(game.xp) + amount;
    while(game.level < 50) {
        uint16_t threshold = static_cast<uint16_t>(4 + game.level * 3);
        if(total < threshold)
            break;
        total -= threshold;
        ++game.level;
        game.max_hp = static_cast<uint8_t>(game.max_hp + 3);
        game.hp = game.max_hp;
        if(game.level % 2 == 0)
            ++game.attack;
        status(F("You gained a level!"));
    }
    game.xp = static_cast<uint8_t>(total);
}

uint8_t distance(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    uint8_t dx = ax > bx ? ax - bx : bx - ax;
    uint8_t dy = ay > by ? ay - by : by - ay;
    return dx + dy;
}

void start_new(uint16_t seed)
{
    uint16_t best = game.best_score;
    memset(&game, 0, sizeof(game));
    game.magic = 0xa7;
    game.version = SAVE_VERSION;
    game.valid = 1;
    game.best_score = best;
    game.run_seed = seed ? seed : 0xace1;
    game.random_state = game.run_seed;
    game.hp = game.max_hp = 18;
    game.level = 1;
    game.attack = 2;
    game.dexterity = 4;
    game.defense = 0;
    game.hunger = 220;
    game.weapon_slot = game.armor_slot = NONE;
    for(uint8_t i = 0; i < POTION_COUNT; ++i)
        game.potion_appearance[i] = i;
    // One Fisher-Yates shuffle per run. The mapping lives in the save.
    for(uint8_t i = POTION_COUNT - 1; i > 0; --i) {
        uint8_t j = static_cast<uint8_t>(next_random(game.random_state) % (i + 1));
        uint8_t old = game.potion_appearance[i];
        game.potion_appearance[i] = game.potion_appearance[j];
        game.potion_appearance[j] = old;
    }
    make_floor();
    game.px = game.up_x;
    game.py = game.up_y;
    visit_room();
    session = {NONE, 0, false};
}

void finish(uint8_t result)
{
    if(game.score > game.best_score)
        game.best_score = game.score;
    game.valid = 0;
    session.ended = true;
    session.result = result;
}

uint8_t monster_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < MONSTERS; ++i)
        if(game.monsters[i].type && game.monsters[i].x == x &&
           game.monsters[i].y == y)
            return i;
    return NONE;
}

uint8_t item_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < GROUND_ITEMS; ++i)
        if(game.ground[i].type && game.ground[i].x == x &&
           game.ground[i].y == y)
            return i;
    for(uint8_t i = 0; i < DROPPED_ITEMS; ++i)
        if(game.dropped[i].type && game.dropped[i].floor == game.floor &&
           game.dropped[i].x == x && game.dropped[i].y == y)
            return static_cast<uint8_t>(GROUND_ITEMS + i);
    return NONE;
}

bool can_monster_move(uint8_t x, uint8_t y)
{
    return !blocked(x, y) && !(x == game.px && y == game.py) &&
           monster_at(x, y) == NONE;
}

void enemy_turn()
{
    for(Monster& monster : game.monsters) {
        if(!monster.type)
            continue;
        if(monster.stun) {
            --monster.stun;
            continue;
        }
        if(game.invisible)
            continue;
        uint8_t range = distance(monster.x, monster.y, game.px, game.py);
        if(range == 1) {
            if(roll(100) < 70) {
                uint8_t raw = static_cast<uint8_t>(monster_damage[monster.type] +
                    game.floor / 5 + roll(3));
                uint8_t damage = raw > game.defense ? raw - game.defense : 1;
                game.hp = damage >= game.hp ? 0 : game.hp - damage;
                status(F("The"));
                status(static_cast<MonsterType>(monster.type));
                status(F("hits you!"));
                if(game.hp == 0) {
                    finish(0);
                    return;
                }
            }
            continue;
        }
        if(range > 8 && roll(4) != 0)
            continue;
        int8_t dx = 0, dy = 0;
        if(range <= 8) {
            dx = monster.x < game.px ? 1 : monster.x > game.px ? -1 : 0;
            dy = monster.y < game.py ? 1 : monster.y > game.py ? -1 : 0;
        } else {
            uint8_t direction = roll(4);
            dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
            dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
        }
        uint8_t nx = static_cast<uint8_t>(monster.x + dx);
        uint8_t ny = static_cast<uint8_t>(monster.y + dy);
        if(dx && can_monster_move(nx, monster.y))
            monster.x = nx;
        else if(dy && can_monster_move(monster.x, ny))
            monster.y = ny;
    }
}

void end_turn()
{
    ++game.turns;
    if(game.confused) --game.confused;
    if(game.paralyzed) --game.paralyzed;
    if(game.slowed) --game.slowed;
    if(game.invisible) --game.invisible;
    if(game.turns % 3 == 0 && game.hunger)
        --game.hunger;
    if(game.hunger == 0 && game.turns % 4 == 0) {
        --game.hp;
        status(F("You are starving!"));
        if(game.hp == 0) {
            finish(0);
            return;
        }
    }
    enemy_turn();
    if(!session.ended && game.slowed && (game.turns & 1))
        enemy_turn();
}

void attack_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    uint8_t hit_chance = static_cast<uint8_t>(72 + game.dexterity * 2);
    if(hit_chance > 95) hit_chance = 95;
    if(roll(100) >= hit_chance) {
        status(F("You miss the"));
        status(static_cast<MonsterType>(target.type));
        status(F("."));
        end_turn();
        return;
    }
    uint8_t bonus = game.weapon_slot != NONE
        ? game.inventory[game.weapon_slot].amount : 0;
    uint8_t damage = static_cast<uint8_t>(
        (game.attack > game.weakened ? game.attack - game.weakened : 1) +
        bonus + roll(3));
    if(damage >= target.hp) {
        uint8_t killed_type = target.type;
        uint8_t x = target.x, y = target.y;
        mark(game.marks[game.floor], KILLED_MONSTERS, index);
        target.type = 0;
        game.score += static_cast<uint16_t>(5 + killed_type * 3);
        if(killed_type == LORD &&
           !marked(game.marks[game.floor], TAKEN_ITEMS, 15))
            game.ground[15] = {x, y, AMULET, 1};
        status(F("You defeat the"));
        status(static_cast<MonsterType>(killed_type));
        status(F("."));
        gain_xp(1);
    } else {
        target.hp -= damage;
        status(F("You hit the"));
        status(static_cast<MonsterType>(target.type));
        status(F("."));
    }
    end_turn();
}

void move_player(int8_t dx, int8_t dy)
{
    if(game.paralyzed) {
        status(F("You cannot move!"));
        end_turn();
        return;
    }
    if(game.confused && roll(2) == 0) {
        uint8_t direction = roll(4);
        dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
        dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
    }
    int16_t x = static_cast<int16_t>(game.px) + dx;
    int16_t y = static_cast<int16_t>(game.py) + dy;
    if(wall_at(x, y)) {
        status(F("A wall blocks your way."));
        return;
    }
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    if(door != NONE && !door_open(door)) {
        mark(game.marks[game.floor], OPENED_DOORS, door);
        status(F("You open the door."));
        end_turn();
        return;
    }
    uint8_t monster = monster_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    if(monster != NONE) {
        attack_monster(monster);
        return;
    }
    game.px = static_cast<uint8_t>(x);
    game.py = static_cast<uint8_t>(y);
    visit_room();
    uint8_t item = item_at(game.px, game.py);
    if(item != NONE)
    {
        status(F("You see"));
        status(item < GROUND_ITEMS
            ? Item{game.ground[item].type, game.ground[item].amount, {0, 0}}
            : Item{game.dropped[item - GROUND_ITEMS].type,
                   game.dropped[item - GROUND_ITEMS].amount, {0, 0}});
        status(F("here. A: pick up."));
    }
    else if((game.px == game.up_x && game.py == game.up_y) ||
            (game.px == game.down_x && game.py == game.down_y))
        status(F("Stairs here. Press A."));
    end_turn();
}

bool add_inventory(uint8_t type, uint8_t amount)
{
    if(type == FOOD || is_potion(type)) {
        for(Item& item : game.inventory)
            if(item.type == type && item.amount <= 99 - amount) {
                item.amount = static_cast<uint8_t>(item.amount + amount);
                return true;
            }
    }
    for(Item& item : game.inventory)
        if(item.type == NO_ITEM) {
            item = {type, amount, {0, 0}};
            return true;
        }
    return false;
}

void take_item(uint8_t index)
{
    uint8_t type = index < GROUND_ITEMS ? game.ground[index].type :
        game.dropped[index - GROUND_ITEMS].type;
    uint8_t amount = index < GROUND_ITEMS ? game.ground[index].amount :
        game.dropped[index - GROUND_ITEMS].amount;
    if(type == AMULET) {
        game.has_amulet = 1;
        game.score += 100;
        status(F("You found the amulet!"));
    } else if(add_inventory(type, amount)) {
        status(F("You picked up"));
        status(Item{type, amount, {0, 0}});
        status(F("."));
    } else {
        status(F("Your pack is full."));
        return;
    }
    if(index < GROUND_ITEMS) {
        mark(game.marks[game.floor], TAKEN_ITEMS, index);
        game.ground[index].type = NO_ITEM;
    } else {
        game.dropped[index - GROUND_ITEMS].type = NO_ITEM;
    }
    end_turn();
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

bool use_inventory(uint8_t slot);

void action()
{
    if(game.paralyzed) {
        status(F("You cannot act!"));
        end_turn();
        return;
    }
    uint8_t item = item_at(game.px, game.py);
    if(item != NONE) {
        take_item(item);
        return;
    }
    if(game.px == game.up_x && game.py == game.up_y) {
        if(game.floor)
            change_floor(-1);
        else
            finish(game.has_amulet ? 1 : 2);
        return;
    }
    if(game.floor < FLOORS - 1 &&
       game.px == game.down_x && game.py == game.down_y) {
        change_floor(1);
        return;
    }
    if(session.repeat_slot != NONE && game.inventory[session.repeat_slot].type) {
        use_inventory(session.repeat_slot);
    } else {
        status(F("There is nothing here."));
        end_turn();
    }
}

bool use_inventory(uint8_t slot)
{
    if(slot >= INVENTORY || game.paralyzed)
        return false;
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM)
        return false;
    session.repeat_slot = slot;
    switch(item.type) {
    case FOOD:
        game.hunger = game.hunger > 145 ? 255 : game.hunger + 110;
        status(F("You eat"));
        status(Item{item.type, 1, {0, 0}});
        status(F("."));
        if(--item.amount == 0)
            item.type = NO_ITEM;
        break;
    case HEALING: case CONFUSION: case POISON: case HARMING:
    case STRENGTH: case DEXTERITY: case PARALYSIS: case SLOWING:
    case EXPERIENCE: case INVISIBILITY: {
        uint8_t type = item.type;
        bool known = potion_identified(type);
        status(F("You drink"));
        status(Item{item.type, 1, {0, 0}});
        status(F("."));
        if(--item.amount == 0)
            item.type = NO_ITEM;
        identify_potion(type);
        if(!known) {
            status(F("It was"));
            status(Item{type, 1, {0, 0}});
            status(F("."));
        }
        switch(type) {
        case HEALING: {
            uint8_t healed = static_cast<uint8_t>(game.max_hp / 4 +
                roll(static_cast<uint8_t>(game.max_hp / 2 + 1)));
            uint16_t hp = static_cast<uint16_t>(game.hp) + healed;
            game.hp = hp > game.max_hp ? game.max_hp : static_cast<uint8_t>(hp);
            game.weakened = 0;
            status(F("You feel better."));
            break;
        }
        case STRENGTH:
            if(game.weakened) game.weakened = 0;
            else if(game.attack < 250) ++game.attack;
            status(F("You feel stronger."));
            break;
        case DEXTERITY:
            if(game.dexterity < 12) ++game.dexterity;
            status(F("You feel more agile."));
            break;
        case EXPERIENCE:
            gain_xp(50);
            break;
        case INVISIBILITY:
            game.invisible = static_cast<uint8_t>(12 + roll(16));
            status(F("You turn invisible."));
            break;
        case HARMING: {
            uint8_t base = static_cast<uint8_t>(game.max_hp / 8 + 1);
            uint8_t damage = static_cast<uint8_t>(base + roll(base * 2));
            if(damage > 10) damage = 10;
            game.hp = damage >= game.hp ? 0 : game.hp - damage;
            status(F("The potion harms you!"));
            if(!game.hp) finish(0);
            break;
        }
        case POISON:
            if(game.weakened < 3) ++game.weakened;
            status(F("You feel weaker."));
            break;
        case CONFUSION:
            game.confused = static_cast<uint8_t>(8 + roll(8));
            status(F("You feel confused."));
            break;
        case PARALYSIS:
            game.paralyzed = static_cast<uint8_t>(3 + roll(4));
            status(F("You are paralyzed!"));
            break;
        case SLOWING:
            game.slowed = static_cast<uint8_t>(8 + roll(8));
            status(F("You feel sluggish."));
            break;
        default: break;
        }
        break;
    }
    case SWORD:
        game.weapon_slot = slot;
        status(F("You equip"));
        status(item);
        status(F("."));
        break;
    case ARMOR:
        game.armor_slot = slot;
        game.defense = item.amount;
        status(F("You equip"));
        status(item);
        status(F("."));
        break;
    default:
        return false;
    }
    if(!session.ended)
        end_turn();
    return true;
}

bool drop_inventory(uint8_t slot)
{
    if(slot >= INVENTORY || game.paralyzed)
        return false;
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM || item_at(game.px, game.py) != NONE)
        return false;
    for(DroppedItem& dropped : game.dropped)
        if(dropped.type == NO_ITEM) {
            dropped = {game.floor, game.px, game.py, item.type, item.amount};
            if(game.weapon_slot == slot)
                game.weapon_slot = NONE;
            if(game.armor_slot == slot) {
                game.armor_slot = NONE;
                game.defense = 0;
            }
            if(session.repeat_slot == slot)
                session.repeat_slot = NONE;
            item.type = NO_ITEM;
            status(F("You dropped"));
            status(Item{dropped.type, dropped.amount, {0, 0}});
            status(F("."));
            end_turn();
            return true;
        }
    status(F("No room to drop that."));
    return false;
}

} // namespace rogue
