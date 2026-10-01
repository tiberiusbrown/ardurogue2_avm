#include "game.hpp"
#include <string.h>

namespace rogue {

Session session = {NONE, 0, false};

static const uint8_t PROGMEM monster_damage[] = {0, 1, 2, 2, 3, 4, 6};

uint8_t roll(uint8_t limit)
{
    return static_cast<uint8_t>(next_random(game.random_state) % limit);
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
    game.version = 2;
    game.valid = 1;
    game.best_score = best;
    game.run_seed = seed ? seed : 0xace1;
    game.random_state = game.run_seed;
    game.hp = game.max_hp = 18;
    game.level = 1;
    game.attack = 2;
    game.defense = 0;
    game.hunger = 220;
    game.weapon_slot = game.armor_slot = NONE;
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
}

void attack_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    if(roll(100) >= 80) {
        status(F("You miss the"));
        status(static_cast<MonsterType>(target.type));
        status(F("."));
        end_turn();
        return;
    }
    uint8_t bonus = game.weapon_slot != NONE
        ? game.inventory[game.weapon_slot].amount : 0;
    uint8_t damage = static_cast<uint8_t>(game.attack + bonus + roll(3));
    if(damage >= target.hp) {
        uint8_t killed_type = target.type;
        uint8_t x = target.x, y = target.y;
        mark(game.marks[game.floor].killed_monsters, target.spawn);
        target.type = 0;
        game.score += static_cast<uint16_t>(5 + killed_type * 3);
        ++game.xp;
        bool leveled = false;
        if(game.xp >= static_cast<uint8_t>(4 + game.level * 3)) {
            game.xp = 0;
            ++game.level;
            game.max_hp = static_cast<uint8_t>(game.max_hp + 3);
            game.hp = game.max_hp;
            if(game.level % 2 == 0)
                ++game.attack;
            leveled = true;
        }
        if(killed_type == LORD && !marked(game.marks[game.floor].taken_items, 15))
            game.ground[15] = {x, y, AMULET, 1};
        status(F("You defeat the"));
        status(static_cast<MonsterType>(killed_type));
        status(F("."));
        if(leveled)
            status(F("You gained a level!"));
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
    int16_t x = static_cast<int16_t>(game.px) + dx;
    int16_t y = static_cast<int16_t>(game.py) + dy;
    if(wall_at(x, y)) {
        status(F("A wall blocks your way."));
        return;
    }
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    if(door != NONE && !game.doors[door].open) {
        game.doors[door].open = 1;
        mark(game.marks[game.floor].opened_doors, door);
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
            ? Item{game.ground[item].type, game.ground[item].amount}
            : Item{game.dropped[item - GROUND_ITEMS].type,
                   game.dropped[item - GROUND_ITEMS].amount});
        status(F("here. A: pick up."));
    }
    else if((game.px == game.up_x && game.py == game.up_y) ||
            (game.px == game.down_x && game.py == game.down_y))
        status(F("Stairs here. Press A."));
    end_turn();
}

bool add_inventory(uint8_t type, uint8_t amount)
{
    if(type == FOOD || type == HEALING) {
        for(Item& item : game.inventory)
            if(item.type == type && item.amount < 99) {
                item.amount += amount;
                return true;
            }
    }
    for(Item& item : game.inventory)
        if(item.type == NO_ITEM) {
            item = {type, amount};
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
        status(Item{type, amount});
        status(F("."));
    } else {
        status(F("Your pack is full."));
        return;
    }
    if(index < GROUND_ITEMS) {
        mark(game.marks[game.floor].taken_items, index);
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
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM)
        return false;
    session.repeat_slot = slot;
    switch(item.type) {
    case FOOD:
        game.hunger = game.hunger > 145 ? 255 : game.hunger + 110;
        status(F("You eat"));
        status(Item{item.type, 1});
        status(F("."));
        if(--item.amount == 0)
            item.type = NO_ITEM;
        break;
    case HEALING:
        game.hp = static_cast<uint8_t>(game.hp + 10 > game.max_hp
            ? game.max_hp : game.hp + 10);
        status(F("You drink"));
        status(Item{item.type, 1});
        status(F("."));
        if(--item.amount == 0)
            item.type = NO_ITEM;
        break;
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
    end_turn();
    return true;
}

bool drop_inventory(uint8_t slot)
{
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
            status(Item{dropped.type, dropped.amount});
            status(F("."));
            end_turn();
            return true;
        }
    status(F("No room to drop that."));
    return false;
}

} // namespace rogue
