#include <avm.h>
#include <string.h>

namespace {

constexpr uint8_t MAP_W = 64;
constexpr uint8_t MAP_H = 32;
constexpr uint8_t FLOORS = 16;
constexpr uint8_t ROOMS = 12;
constexpr uint8_t DOORS = 11;
constexpr uint8_t MONSTERS = 12;
constexpr uint8_t GROUND_ITEMS = 16;
constexpr uint8_t DROPPED_ITEMS = 8;
constexpr uint8_t INVENTORY = 16;
constexpr uint8_t NONE = 0xff;

enum ItemType : uint8_t {
    NO_ITEM, FOOD, HEALING, SWORD, ARMOR, AMULET
};

enum MonsterType : uint8_t {
    NO_MONSTER, RAT, SNAKE, SKELETON, ORC, TROLL, LORD
};

enum Mode : uint8_t {
    TITLE, PLAY, MENU, INVENTORY_MENU, FULL_MAP, END
};

enum Message : uint8_t {
    WELCOME, WALL, OPENED, HIT, MISSED, HURT, KILLED, FOUND,
    FULL, HEALED, FED, EQUIPPED, EMPTY, STAIRS, AMULET_FOUND,
    HUNGRY, NO_ITEM_HERE, DROPPED, PICKED_UP
};

struct Room { uint8_t x, y, w, h; };
struct Door { uint8_t x, y, open; };
struct Monster { uint8_t x, y, type, hp, stun, spawn; };
struct GroundItem { uint8_t x, y, type, amount; };
struct DroppedItem { uint8_t floor, x, y, type, amount; };
struct Item { uint8_t type, amount; };
struct FloorMarks {
    uint16_t taken_items;
    uint16_t killed_monsters;
    uint16_t opened_doors;
    uint16_t visited_rooms;
};

struct Game {
    uint8_t walls[MAP_W * MAP_H / 8];
    uint8_t explored[MAP_W * MAP_H / 32]; // One bit per 2x2 tiles.
    FloorMarks marks[FLOORS];
    Room rooms[ROOMS];
    Door doors[DOORS];
    Monster monsters[MONSTERS];
    GroundItem ground[GROUND_ITEMS];
    Item inventory[INVENTORY];
    uint16_t run_seed, random_state, score, best_score;
    uint8_t magic, version, valid, floor;
    uint8_t px, py, up_x, up_y, down_x, down_y;
    uint8_t hp, max_hp, level, xp, attack, defense, hunger, turns;
    uint8_t has_amulet, door_count, weapon_slot, armor_slot;
    DroppedItem dropped[DROPPED_ITEMS];
};

static_assert(sizeof(Game) <= 900, "Game state needs room for runtime globals");
static Game game __attribute__((section(".saved"))) = {};

struct Ui {
    uint8_t mode, selection, message, result, repeat_slot;
    uint8_t previous_buttons, held_direction, held_frames;
    bool has_save, dirty;
};
static Ui ui = {};
static_assert(sizeof(Game) + sizeof(Ui) <= 800,
    "Leave at least 224 bytes of AVM RAM for the stack");

static const uint8_t AVM_PROGMEM monster_health[] = {0, 3, 5, 8, 12, 17, 48};
static const uint8_t AVM_PROGMEM monster_damage[] = {0, 1, 2, 2, 3, 4, 6};

// Five rows of five pixels for each icon, kept entirely in program memory.
static const uint8_t AVM_PROGMEM icons[][5] = {
    {0x0e, 0x11, 0x17, 0x10, 0x0e}, // player
    {0x00, 0x0e, 0x0a, 0x0e, 0x00}, // ordinary monster
    {0x15, 0x0e, 0x1f, 0x0e, 0x15}, // Lord of Darkness
    {0x04, 0x0e, 0x0e, 0x04, 0x00}, // item
    {0x04, 0x0e, 0x1f, 0x04, 0x04}, // down stairs
    {0x04, 0x04, 0x1f, 0x0e, 0x04}, // up stairs
    {0x11, 0x0a, 0x04, 0x0a, 0x11}, // door
};

uint16_t next_random(uint16_t& state)
{
    uint16_t x = state ? state : 0xace1;
    x ^= static_cast<uint16_t>(x << 7);
    x ^= x >> 9;
    x ^= static_cast<uint16_t>(x << 8);
    state = x;
    return x;
}

uint8_t roll(uint8_t limit)
{
    return static_cast<uint8_t>(next_random(game.random_state) % limit);
}

uint8_t floor_roll(uint16_t& seed, uint8_t limit)
{
    return static_cast<uint8_t>(next_random(seed) % limit);
}

uint8_t distance(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    uint8_t dx = ax > bx ? ax - bx : bx - ax;
    uint8_t dy = ay > by ? ay - by : by - ay;
    return dx + dy;
}

bool marked(uint16_t bits, uint8_t index)
{
    return (bits & static_cast<uint16_t>(1u << index)) != 0;
}

void mark(uint16_t& bits, uint8_t index)
{
    bits |= static_cast<uint16_t>(1u << index);
}

bool wall_at(int16_t x, int16_t y)
{
    if(x < 0 || x >= MAP_W || y < 0 || y >= MAP_H)
        return true;
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    return (game.walls[index >> 3] & (1u << (index & 7))) != 0;
}

void carve(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>(y * MAP_W + x);
    game.walls[index >> 3] &= static_cast<uint8_t>(~(1u << (index & 7)));
}

void explore(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>((y >> 1) * (MAP_W / 2) + (x >> 1));
    game.explored[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

bool explored(uint8_t x, uint8_t y)
{
    uint16_t index = static_cast<uint16_t>((y >> 1) * (MAP_W / 2) + (x >> 1));
    return (game.explored[index >> 3] & (1u << (index & 7))) != 0;
}

uint8_t door_at(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < game.door_count; ++i)
        if(game.doors[i].x == x && game.doors[i].y == y)
            return i;
    return NONE;
}

bool blocked(int16_t x, int16_t y)
{
    if(wall_at(x, y))
        return true;
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    return door != NONE && !game.doors[door].open;
}

bool in_room(uint8_t x, uint8_t y, const Room& room)
{
    return x >= room.x && x < room.x + room.w &&
           y >= room.y && y < room.y + room.h;
}

bool in_any_room(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < ROOMS; ++i)
        if(in_room(x, y, game.rooms[i]))
            return true;
    return false;
}

void visit_room()
{
    for(uint8_t i = 0; i < ROOMS; ++i)
        if(in_room(game.px, game.py, game.rooms[i])) {
            mark(game.marks[game.floor].visited_rooms, i);
            break;
        }
}

void tunnel(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    int16_t x = ax;
    int16_t y = ay;
    while(x != bx) {
        carve(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        x += x < bx ? 1 : -1;
    }
    while(y != by) {
        carve(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        y += y < by ? 1 : -1;
    }
    carve(bx, by);
}

void make_floor()
{
    memset(game.walls, 0xff, sizeof(game.walls));
    memset(game.explored, 0, sizeof(game.explored));
    memset(game.doors, 0, sizeof(game.doors));
    memset(game.monsters, 0, sizeof(game.monsters));
    memset(game.ground, 0, sizeof(game.ground));
    game.door_count = 0;

    uint16_t seed = static_cast<uint16_t>(game.run_seed ^
        static_cast<uint16_t>((game.floor + 1u) * 0x9e37u));
    FloorMarks& marks = game.marks[game.floor];

    for(uint8_t i = 0; i < ROOMS; ++i) {
        Room& room = game.rooms[i];
        room.w = static_cast<uint8_t>(5 + floor_roll(seed, 6));
        room.h = static_cast<uint8_t>(4 + floor_roll(seed, 4));
        room.x = static_cast<uint8_t>((i % 4) * 16 + 2 +
            floor_roll(seed, static_cast<uint8_t>(13 - room.w)));
        room.y = static_cast<uint8_t>((i / 4) * 10 + 1 +
            floor_roll(seed, static_cast<uint8_t>(9 - room.h)));
        for(uint8_t y = room.y; y < room.y + room.h; ++y)
            for(uint8_t x = room.x; x < room.x + room.w; ++x)
                carve(x, y);
    }

    for(uint8_t i = 1; i < ROOMS; ++i) {
        uint8_t parent = i >= 4 && floor_roll(seed, 2) ? i - 4 : i - 1;
        const Room& a = game.rooms[parent];
        const Room& b = game.rooms[i];
        uint8_t ax = static_cast<uint8_t>(a.x + a.w / 2);
        uint8_t ay = static_cast<uint8_t>(a.y + a.h / 2);
        uint8_t bx = static_cast<uint8_t>(b.x + b.w / 2);
        uint8_t by = static_cast<uint8_t>(b.y + b.h / 2);
        tunnel(ax, ay, bx, by);

        uint8_t dx = static_cast<uint8_t>((static_cast<uint16_t>(ax) + bx) / 2);
        if(game.door_count < DOORS && !in_any_room(dx, ay) &&
           door_at(dx, ay) == NONE && floor_roll(seed, 3) != 0) {
            uint8_t id = game.door_count++;
            game.doors[id] = {dx, ay,
                static_cast<uint8_t>(marked(marks.opened_doors, id))};
        }
    }

    const Room& first = game.rooms[0];
    const Room& last = game.rooms[ROOMS - 1];
    game.up_x = static_cast<uint8_t>(first.x + first.w / 2);
    game.up_y = static_cast<uint8_t>(first.y + first.h / 2);
    game.down_x = static_cast<uint8_t>(last.x + last.w / 2);
    game.down_y = static_cast<uint8_t>(last.y + last.h / 2);

    for(uint8_t i = 0; i < ROOMS; ++i) {
        if(!marked(marks.visited_rooms, i))
            continue;
        const Room& room = game.rooms[i];
        for(uint8_t y = room.y; y < room.y + room.h; ++y)
            for(uint8_t x = room.x; x < room.x + room.w; ++x)
                explore(x, y);
    }

    for(uint8_t i = 0; i < MONSTERS; ++i) {
        const Room& room = game.rooms[i];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t type = game.floor == FLOORS - 1 && i == MONSTERS - 1
            ? LORD : static_cast<uint8_t>(1 + floor_roll(seed,
                static_cast<uint8_t>(1 + (game.floor < 10 ? game.floor / 2 : 5))));
        if(type > TROLL)
            type = TROLL;
        if(type == LORD) {
            x = game.down_x;
            y = game.down_y;
        } else if((x == game.up_x && y == game.up_y) ||
                  (x == game.down_x && y == game.down_y)) {
            x = static_cast<uint8_t>(room.x + 1);
        }
        if(marked(marks.killed_monsters, i))
            continue;
        game.monsters[i] = {x, y, type,
            static_cast<uint8_t>(monster_health[type] + game.floor / 2), 0, i};
    }

    for(uint8_t i = 0; i < GROUND_ITEMS; ++i) {
        const Room& room = game.rooms[(i * 7u + 3u) % ROOMS];
        uint8_t x = static_cast<uint8_t>(room.x + 1 + floor_roll(seed, room.w - 2));
        uint8_t y = static_cast<uint8_t>(room.y + 1 + floor_roll(seed, room.h - 2));
        uint8_t chance = floor_roll(seed, 12);
        uint8_t type = chance < 4 ? FOOD : chance < 8 ? HEALING :
                       chance < 10 ? SWORD : ARMOR;
        uint8_t amount = type == SWORD || type == ARMOR
            ? static_cast<uint8_t>(1 + game.floor / 4) : 1;
        if(marked(marks.taken_items, i) || (game.floor == FLOORS - 1 && i == 15))
            continue;
        game.ground[i] = {x, y, type, amount};
    }
    if(game.floor == FLOORS - 1 &&
       marked(marks.killed_monsters, MONSTERS - 1) &&
       !marked(marks.taken_items, 15))
        game.ground[15] = {game.down_x, game.down_y, AMULET, 1};
}

void start_new()
{
    uint16_t best = game.best_score;
    if(ui.has_save) {
        game.valid = 0;
        avm_save();
    }
    memset(&game, 0, sizeof(game));
    game.magic = 0xa7;
    game.version = 2;
    game.valid = 1;
    game.best_score = best;
    game.run_seed = avm_generate_random_seed();
    if(!game.run_seed)
        game.run_seed = 0xace1;
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
    ui.has_save = false;
    ui.mode = PLAY;
    ui.message = WELCOME;
    ui.repeat_slot = NONE;
}

void finish(uint8_t result)
{
    if(game.score > game.best_score)
        game.best_score = game.score;
    game.valid = 0;
    avm_save();
    ui.has_save = false;
    ui.mode = END;
    ui.result = result;
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
                ui.message = HURT;
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
        ui.message = HUNGRY;
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
        ui.message = MISSED;
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
        if(game.xp >= static_cast<uint8_t>(4 + game.level * 3)) {
            game.xp = 0;
            ++game.level;
            game.max_hp = static_cast<uint8_t>(game.max_hp + 3);
            game.hp = game.max_hp;
            if(game.level % 2 == 0)
                ++game.attack;
        }
        if(killed_type == LORD && !marked(game.marks[game.floor].taken_items, 15))
            game.ground[15] = {x, y, AMULET, 1};
        ui.message = KILLED;
    } else {
        target.hp -= damage;
        ui.message = HIT;
    }
    end_turn();
}

void move_player(int8_t dx, int8_t dy)
{
    int16_t x = static_cast<int16_t>(game.px) + dx;
    int16_t y = static_cast<int16_t>(game.py) + dy;
    if(wall_at(x, y)) {
        ui.message = WALL;
        return;
    }
    uint8_t door = door_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    if(door != NONE && !game.doors[door].open) {
        game.doors[door].open = 1;
        mark(game.marks[game.floor].opened_doors, door);
        ui.message = OPENED;
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
    ui.message = item_at(game.px, game.py) != NONE ? FOUND :
        ((game.px == game.up_x && game.py == game.up_y) ||
         (game.px == game.down_x && game.py == game.down_y)) ? STAIRS : EMPTY;
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
        ui.message = AMULET_FOUND;
    } else if(add_inventory(type, amount)) {
        ui.message = PICKED_UP;
    } else {
        ui.message = FULL;
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
    ui.message = STAIRS;
}

void use_inventory(uint8_t slot);

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
    if(ui.repeat_slot != NONE && game.inventory[ui.repeat_slot].type) {
        use_inventory(ui.repeat_slot);
    } else {
        ui.message = NO_ITEM_HERE;
        end_turn();
    }
}

void use_inventory(uint8_t slot)
{
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM)
        return;
    ui.repeat_slot = slot;
    switch(item.type) {
    case FOOD:
        game.hunger = game.hunger > 145 ? 255 : game.hunger + 110;
        ui.message = FED;
        if(--item.amount == 0)
            item.type = NO_ITEM;
        break;
    case HEALING:
        game.hp = static_cast<uint8_t>(game.hp + 10 > game.max_hp
            ? game.max_hp : game.hp + 10);
        ui.message = HEALED;
        if(--item.amount == 0)
            item.type = NO_ITEM;
        break;
    case SWORD:
        game.weapon_slot = slot;
        ui.message = EQUIPPED;
        break;
    case ARMOR:
        game.armor_slot = slot;
        game.defense = item.amount;
        ui.message = EQUIPPED;
        break;
    default:
        return;
    }
    ui.mode = PLAY;
    end_turn();
}

void drop_inventory(uint8_t slot)
{
    Item& item = game.inventory[slot];
    if(item.type == NO_ITEM || item_at(game.px, game.py) != NONE)
        return;
    for(DroppedItem& dropped : game.dropped)
        if(dropped.type == NO_ITEM) {
            dropped = {game.floor, game.px, game.py, item.type, item.amount};
            if(game.weapon_slot == slot)
                game.weapon_slot = NONE;
            if(game.armor_slot == slot) {
                game.armor_slot = NONE;
                game.defense = 0;
            }
            if(ui.repeat_slot == slot)
                ui.repeat_slot = NONE;
            item.type = NO_ITEM;
            ui.mode = PLAY;
            ui.message = DROPPED;
            end_turn();
            return;
        }
    ui.message = FULL;
}

bool can_see(uint8_t tx, uint8_t ty)
{
    int16_t x = game.px, y = game.py;
    int16_t dx = tx > x ? tx - x : x - tx;
    int16_t dy = ty > y ? ty - y : y - ty;
    if(dx > 6 || dy > 6)
        return false;
    int16_t sx = x < tx ? 1 : -1;
    int16_t sy = y < ty ? 1 : -1;
    int16_t error = dx - dy;
    for(;;) {
        if(x == tx && y == ty)
            return true;
        int16_t twice = 2 * error;
        if(twice > -dy) { error -= dy; x += sx; }
        if(twice < dx) { error += dx; y += sy; }
        if(x == tx && y == ty)
            return true;
        if(blocked(x, y))
            return false;
    }
}

void pixel(int16_t x, int16_t y)
{
    if(x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t offset = static_cast<uint16_t>((y >> 3) * 128 + x);
    __avm_framebuffer[offset] |= static_cast<uint8_t>(1u << (y & 7));
}

void icon(uint8_t kind, int16_t x, int16_t y)
{
    for(uint8_t row = 0; row < 5; ++row) {
        uint8_t bits = icons[kind][row];
        for(uint8_t col = 0; col < 5; ++col)
            if(bits & (1u << col))
                pixel(x + col, y + row);
    }
}

void map_tile(uint8_t x, uint8_t y, uint8_t sx, uint8_t sy)
{
    int16_t px = sx * 5;
    int16_t py = sy * 5;
    bool visible = can_see(x, y);
    if(visible)
        explore(x, y);
    if(!visible && !explored(x, y))
        return;
    if(wall_at(x, y)) {
        for(uint8_t k = 0; k < 5; ++k) {
            pixel(px + k, py);
            pixel(px, py + k);
        }
    } else if(visible) {
        pixel(px + 2, py + 2);
    }
    uint8_t door = door_at(x, y);
    if(door != NONE && !game.doors[door].open)
        icon(6, px, py);
    if(x == game.up_x && y == game.up_y)
        icon(5, px, py);
    else if(game.floor < FLOORS - 1 &&
            x == game.down_x && y == game.down_y)
        icon(4, px, py);
    if(!visible)
        return;
    if(item_at(x, y) != NONE)
        icon(3, px, py);
    uint8_t monster = monster_at(x, y);
    if(monster != NONE)
        icon(game.monsters[monster].type == LORD ? 2 : 1, px, py);
    if(x == game.px && y == game.py)
        icon(0, px, py);
}

void render_message()
{
    switch(ui.message) {
    case WELCOME: avm_draw_text_P(66, 55, F("WELCOME")); break;
    case WALL: avm_draw_text_P(66, 55, F("A WALL")); break;
    case OPENED: avm_draw_text_P(66, 55, F("DOOR OPEN")); break;
    case HIT: avm_draw_text_P(66, 55, F("YOU HIT")); break;
    case MISSED: avm_draw_text_P(66, 55, F("YOU MISS")); break;
    case HURT: avm_draw_text_P(66, 55, F("YOU HURT")); break;
    case KILLED: avm_draw_text_P(66, 55, F("FOE DOWN")); break;
    case FOUND: avm_draw_text_P(66, 55, F("A: PICK UP")); break;
    case PICKED_UP: avm_draw_text_P(66, 55, F("PICKED UP")); break;
    case FULL: avm_draw_text_P(66, 55, F("PACK FULL")); break;
    case HEALED: avm_draw_text_P(66, 55, F("HEALED")); break;
    case FED: avm_draw_text_P(66, 55, F("ATE FOOD")); break;
    case EQUIPPED: avm_draw_text_P(66, 55, F("EQUIPPED")); break;
    case STAIRS: avm_draw_text_P(66, 55, F("A: STAIRS")); break;
    case AMULET_FOUND: avm_draw_text_P(66, 55, F("AMULET!")); break;
    case HUNGRY: avm_draw_text_P(66, 55, F("STARVING")); break;
    case DROPPED: avm_draw_text_P(66, 55, F("DROPPED")); break;
    default: avm_draw_text_P(66, 55, F("A: WAIT")); break;
    }
}

void render_play()
{
    for(uint8_t sy = 0; sy < 13; ++sy)
        for(uint8_t sx = 0; sx < 13; ++sx) {
            int16_t x = static_cast<int16_t>(game.px) + sx - 6;
            int16_t y = static_cast<int16_t>(game.py) + sy - 6;
            if(x >= 0 && x < MAP_W && y >= 0 && y < MAP_H)
                map_tile(static_cast<uint8_t>(x), static_cast<uint8_t>(y), sx, sy);
        }
    for(uint8_t y = 0; y < 64; ++y)
        pixel(64, y);
    avm_draw_textf_P(67, 7, F("D%u LV%u"), game.floor + 1, game.level);
    avm_draw_textf_P(67, 15, F("HP%u/%u"), game.hp, game.max_hp);
    avm_draw_textf_P(67, 23, F("AT%u DF%u"), game.attack +
        (game.weapon_slot != NONE ? game.inventory[game.weapon_slot].amount : 0),
        game.defense);
    avm_draw_textf_P(67, 31, F("XP%u"), game.xp);
    avm_draw_textf_P(67, 39, F("FOOD%u"), game.hunger);
    avm_draw_textf_P(67, 47, F("SCORE%u"), game.score);
    render_message();
    avm_draw_text_P(67, 63, F("B:MENU"));
}

void render_title()
{
    avm_draw_text_P(22, 14, F("ARDUROGUE 2"));
    avm_draw_text_P(14, 30, ui.has_save ? F("A: CONTINUE") : F("A: NEW GAME"));
    if(ui.has_save)
        avm_draw_text_P(14, 40, F("B: NEW GAME"));
    avm_draw_textf_P(14, 56, F("BEST %u"), game.best_score);
}

void render_menu()
{
    static const char AVM_PROGMEM* const AVM_PROGMEM names[] = {
        F("WAIT"), F("INVENTORY"), F("FULL MAP"),
        F("SAVE & EXIT"), F("ABANDON")
    };
    avm_draw_text_P(10, 8, F("ACTION MENU"));
    for(uint8_t i = 0; i < 5; ++i) {
        if(i == ui.selection)
            avm_draw_text_P(4, static_cast<int16_t>(20 + 9 * i), F(">"));
        avm_draw_text_P(12, static_cast<int16_t>(20 + 9 * i), names[i]);
    }
}

void render_inventory()
{
    avm_draw_text_P(2, 7, F("A USE > DROP B BACK"));
    uint8_t start = ui.selection < 8 ? 0 : 8;
    for(uint8_t row = 0; row < 8; ++row) {
        uint8_t i = start + row;
        int16_t y = static_cast<int16_t>(14 + row * 7);
        if(i == ui.selection)
            avm_draw_text_P(1, y, F(">"));
        const Item& item = game.inventory[i];
        if(!item.type)
            continue;
        switch(item.type) {
        case FOOD: avm_draw_textf_P(10, y, F("FOOD x%u"), item.amount); break;
        case HEALING: avm_draw_textf_P(10, y, F("HEAL x%u"), item.amount); break;
        case SWORD: avm_draw_textf_P(10, y, F("SWORD +%u"), item.amount); break;
        case ARMOR: avm_draw_textf_P(10, y, F("ARMOR +%u"), item.amount); break;
        default: break;
        }
        if(game.weapon_slot == i || game.armor_slot == i)
            avm_draw_text_P(75, y, F("*"));
    }
}

void render_full_map()
{
    for(uint8_t y = 0; y < MAP_H; ++y)
        for(uint8_t x = 0; x < MAP_W; ++x) {
            if(!explored(x, y))
                continue;
            if(wall_at(x, y)) {
                pixel(x * 2, y * 2);
                pixel(x * 2 + 1, y * 2);
                pixel(x * 2, y * 2 + 1);
                pixel(x * 2 + 1, y * 2 + 1);
            } else if(can_see(x, y)) {
                pixel(x * 2, y * 2);
            }
        }
    pixel(game.px * 2, game.py * 2);
    pixel(game.px * 2 + 1, game.py * 2 + 1);
}

void render_end()
{
    avm_draw_text_P(16, 15, ui.result == 1 ? F("YOU ESCAPED!") :
        ui.result == 2 ? F("RETURNED EMPTY") : F("YOU DIED"));
    avm_draw_textf_P(16, 31, F("SCORE %u"), game.score);
    avm_draw_textf_P(16, 41, F("BEST %u"), game.best_score);
    avm_draw_text_P(16, 57, F("A: TITLE"));
}

void render()
{
    switch(ui.mode) {
    case TITLE: render_title(); break;
    case PLAY: render_play(); break;
    case MENU: render_menu(); break;
    case INVENTORY_MENU: render_inventory(); break;
    case FULL_MAP: render_full_map(); break;
    case END: render_end(); break;
    }
    avm_display(AVM_CLEAR_BUFFER);
    ui.dirty = false;
}

uint8_t directional_press(uint8_t buttons, uint8_t edges)
{
    uint8_t direction = buttons & static_cast<uint8_t>(
        AVM_BUTTON_U | AVM_BUTTON_D | AVM_BUTTON_L | AVM_BUTTON_R);
    if(direction != ui.held_direction) {
        ui.held_direction = direction;
        ui.held_frames = 0;
    } else if(direction && ui.held_frames < 250) {
        ++ui.held_frames;
    }
    if(direction && ((edges & direction) ||
        (ui.held_frames >= 9 && ui.held_frames % 3 == 0))) {
        if(direction & AVM_BUTTON_U) return AVM_BUTTON_U;
        if(direction & AVM_BUTTON_D) return AVM_BUTTON_D;
        if(direction & AVM_BUTTON_L) return AVM_BUTTON_L;
        if(direction & AVM_BUTTON_R) return AVM_BUTTON_R;
    }
    return 0;
}

void handle_input(uint8_t buttons)
{
    uint8_t edges = static_cast<uint8_t>(buttons & ~ui.previous_buttons);
    ui.previous_buttons = buttons;
    uint8_t direction = directional_press(buttons, edges);
    if(ui.mode == TITLE) {
        if(edges & AVM_BUTTON_A) {
            if(ui.has_save) {
                game.valid = 0;
                avm_save(); // One-use save: a crash cannot reload the run.
                game.valid = 1;
                ui.mode = PLAY;
                ui.has_save = false;
                ui.message = WELCOME;
            } else {
                start_new();
            }
            ui.dirty = true;
        } else if(ui.has_save && (edges & AVM_BUTTON_B)) {
            start_new();
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == END) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = TITLE;
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == FULL_MAP) {
        if(edges & (AVM_BUTTON_A | AVM_BUTTON_B)) {
            ui.mode = PLAY;
            ui.dirty = true;
        }
        return;
    }
    if(ui.mode == MENU) {
        if(direction == AVM_BUTTON_U && ui.selection)
            --ui.selection;
        else if(direction == AVM_BUTTON_D && ui.selection < 4)
            ++ui.selection;
        else if(edges & AVM_BUTTON_B)
            ui.mode = PLAY;
        else if(edges & AVM_BUTTON_A) {
            switch(ui.selection) {
            case 0: ui.mode = PLAY; ui.message = EMPTY; end_turn(); break;
            case 1: ui.mode = INVENTORY_MENU; ui.selection = 0; break;
            case 2: ui.mode = FULL_MAP; break;
            case 3:
                game.valid = 1;
                avm_save();
                ui.has_save = true;
                ui.mode = TITLE;
                break;
            case 4: finish(2); break;
            }
        }
        ui.dirty = true;
        return;
    }
    if(ui.mode == INVENTORY_MENU) {
        if(direction == AVM_BUTTON_U && ui.selection)
            --ui.selection;
        else if(direction == AVM_BUTTON_D && ui.selection < INVENTORY - 1)
            ++ui.selection;
        else if(direction == AVM_BUTTON_R)
            drop_inventory(ui.selection);
        else if(edges & AVM_BUTTON_B)
            ui.mode = PLAY;
        else if(edges & AVM_BUTTON_A)
            use_inventory(ui.selection);
        ui.dirty = true;
        return;
    }
    if(edges & AVM_BUTTON_B) {
        ui.mode = MENU;
        ui.selection = 0;
    } else if(direction) {
        int8_t dx = direction == AVM_BUTTON_L ? -1 :
                    direction == AVM_BUTTON_R ? 1 : 0;
        int8_t dy = direction == AVM_BUTTON_U ? -1 :
                    direction == AVM_BUTTON_D ? 1 : 0;
        move_player(dx, dy);
    } else if(edges & AVM_BUTTON_A) {
        action();
    } else {
        return;
    }
    ui.dirty = true;
}

} // namespace

extern "C" int main()
{
    avm_set_frame_rate(30);
    avm_set_text_font(AVM_FONT_5X7);
    if(avm_save_exists() && avm_load() &&
       game.magic == 0xa7 && game.version == 2 && game.valid)
        ui.has_save = true;
    ui.mode = TITLE;
    ui.repeat_slot = NONE;
    ui.dirty = true;

    for(;;) {
        if(!avm_next_frame())
            continue;
        handle_input(avm_buttons());
        if(ui.dirty)
            render();
    }
}
