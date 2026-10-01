#include "game.hpp"
#include <string.h>

namespace rogue {

Session session = {NONE, 0, false};

// ArduRogue's MONSTER_INFO, excluding its player entry. Speed is a turn cost:
// smaller values act more often. Flags retain the original two-byte layout.
static const MonsterInfo PROGMEM monster_table[] = {
    {0, 0, 0, 0, 0, 0, 0},
    {0, 1, 6, 8, 0, 1, 1},                         // bat
    {MON_MEAN, 2, 3, 3, 0, 3, 2},                // snake
    {MON_MEAN | MON_POISON, 3, 3, 3, 0, 4, 3}, // rattlesnake
    {MON_MEAN | MON_OPENER, 4, 2, 2, 0, 6, 5}, // zombie
    {MON_MEAN | MON_OPENER, 5, 4, 4, 1, 10, 6}, // goblin
    {MON_MEAN | MON_NATURAL_INVIS | MON_OPENER | MON_SEE_INVIS,
        6, 4, 4, 1, 12, 7},                     // phantom
    {MON_MEAN | MON_OPENER, 7, 4, 4, 3, 16, 8}, // orc
    {MON_MEAN | MON_PARALYZE_HIT, 5, 4, 4, 0, 12, 9}, // tarantula
    {MON_MEAN | MON_OPENER, 8, 4, 4, 2, 20, 11}, // hobgoblin
    {MON_MEAN | MON_NOMOVE, 7, 4, 4, 3, 20, 11}, // mimic
    {MON_MEAN | MON_CONFUSE_HIT | MON_OPENER | MON_SEE_INVIS,
        9, 4, 4, 3, 24, 14},                    // incubus
    {MON_MEAN | MON_REGENS | MON_OPENER, 10, 3, 3, 5, 32, 18}, // troll
    {MON_MEAN, 7, 6, 6, 1, 24, 18},            // griffin
    {MON_MEAN | MON_FIRE_BREATH, 12, 4, 4, 8, 48, 25}, // dragon
    {MON_MEAN | MON_CONFUSE_HIT | MON_PARALYZE_HIT |
         MON_OPENER | MON_SEE_INVIS, 10, 6, 6, 3, 24, 35}, // angel
    {MON_MEAN | MON_REGENS | MON_POISON | MON_CONFUSE_HIT |
         MON_PARALYZE_HIT | MON_SEE_INVIS, 16, 6, 8, 8, 128, 90} // Lord
};

MonsterInfo monster_info(uint8_t type)
{
    MonsterInfo info = {};
    if(type <= LORD)
        memcpy_P(&info, &monster_table[type], sizeof(info));
    return info;
}

static int8_t ring_bonus(uint8_t type)
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

static int8_t amulet_bonus(uint8_t type)
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

static bool player_is_invisible()
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

static void heal_player(uint8_t amount)
{
    if(amulet_bonus(AMULET_REGENERATION) < 0)
        amount = static_cast<uint8_t>((amount + 1) / 2);
    uint8_t maximum = player_max_hp();
    uint16_t hp = static_cast<uint16_t>(game.hp) + amount;
    game.hp = hp > maximum ? maximum : static_cast<uint8_t>(hp);
}

uint8_t monster_effect(const Monster& monster, MonsterEffect effect)
{
    uint8_t packed = monster.effects[effect >> 1];
    return effect & 1 ? packed >> 4 : packed & 0x0f;
}

static void set_monster_effect(Monster& monster, MonsterEffect effect,
                               uint8_t duration)
{
    uint8_t& packed = monster.effects[effect >> 1];
    if(effect & 1)
        packed = static_cast<uint8_t>((packed & 0x0f) | (duration << 4));
    else
        packed = static_cast<uint8_t>((packed & 0xf0) | duration);
}

static void monster_status(const Monster& monster,
                           const char PROGMEM* message)
{
    status(F("The"));
    status(static_cast<MonsterType>(monster.type));
    status(message);
}

static void age_monster_effects(Monster& monster)
{
    for(uint8_t i = MON_CONFUSED; i <= MON_INVISIBLE; ++i) {
        auto effect = static_cast<MonsterEffect>(i);
        uint8_t duration = monster_effect(monster, effect);
        if(duration) {
            set_monster_effect(monster, effect,
                               static_cast<uint8_t>(duration - 1));
            if(duration == 1) {
                switch(effect) {
                case MON_CONFUSED:
                    monster_status(monster, F("is no longer confused."));
                    break;
                case MON_SLOWED:
                    monster_status(monster, F("moves normally again."));
                    break;
                case MON_INVISIBLE:
                    if(!(monster_info(monster.type).flags & MON_NATURAL_INVIS))
                        monster_status(monster, F("becomes visible again."));
                    break;
                default: break;
                }
            }
        }
    }
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

static void identify_type(uint8_t type)
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

static void gain_xp(uint8_t amount)
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

bool can_monster_move(uint8_t x, uint8_t y)
{
    return !blocked(x, y) && !(x == game.px && y == game.py) &&
           monster_at(x, y) == NONE;
}

static void hurt_player(uint8_t damage)
{
    game.hp = damage >= game.hp ? 0 : static_cast<uint8_t>(game.hp - damage);
    if(!game.hp)
        finish(0);
}

static void fire_splash_monsters()
{
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        Monster& target = game.monsters[i];
        if(!target.type || target.type == DRAGON)
            continue;
        uint8_t dx = target.x > game.px ? target.x - game.px :
            game.px - target.x;
        uint8_t dy = target.y > game.py ? target.y - game.py :
            game.py - target.y;
        if(dx > 1 || dy > 1)
            continue;
        uint8_t damage = static_cast<uint8_t>(8 + roll(8));
        if(damage >= target.hp) {
            if(target.type == LORD &&
               !marked(game.marks[game.floor], TAKEN_ITEMS, 15))
                game.ground[15] = {target.x, target.y, {YENDOR_AMULET, 1}};
            mark(game.marks[game.floor], KILLED_MONSTERS, i);
            target.type = NO_MONSTER;
        } else {
            target.hp = static_cast<uint8_t>(target.hp - damage);
        }
    }
}

static bool fire_line_clear(const Monster& monster)
{
    int8_t dx = monster.x == game.px ? 0 :
        monster.x < game.px ? 1 : -1;
    int8_t dy = monster.y == game.py ? 0 :
        monster.y < game.py ? 1 : -1;
    if(dx && dy)
        return false;
    uint8_t range = distance(monster.x, monster.y, game.px, game.py);
    if(!range || range > 5)
        return false;
    int16_t x = monster.x, y = monster.y;
    for(uint8_t i = 1; i < range; ++i) {
        x += dx;
        y += dy;
        if(blocked(x, y) || monster_at(static_cast<uint8_t>(x),
                                       static_cast<uint8_t>(y)) != NONE)
            return false;
    }
    return true;
}

static void advance_monster(uint8_t index)
{
    Monster& monster = game.monsters[index];
    if(!monster.type)
        return;
    MonsterInfo info = monster_info(monster.type);
    bool confused = monster_effect(monster, MON_CONFUSED) != 0;
    bool afraid = (monster.state & MON_AFRAID) != 0;
    if(afraid && roll(32) == 0)
        monster.state &= static_cast<uint8_t>(~MON_AFRAID);
    if(monster.stun) {
        --monster.stun;
        if(!monster.stun)
            monster_status(monster, F("can move again."));
    } else if(!(info.flags & MON_NOMOVE) || (monster.state & MON_AGGRO)) {
        uint8_t range = distance(monster.x, monster.y, game.px, game.py);
        bool pursuing = (info.flags & MON_MEAN) || (monster.state & MON_AGGRO);
        if(player_is_invisible() && !(info.flags & MON_SEE_INVIS))
            pursuing = false;
        if(!afraid && pursuing && !confused && (info.flags & MON_FIRE_BREATH) &&
           fire_line_clear(monster) && roll(2)) {
            status(F("The"));
            status(static_cast<MonsterType>(monster.type));
            status(F("breathes fire!"));
            uint8_t damage = static_cast<uint8_t>(8 + roll(8));
            int8_t protection = ring_bonus(RING_FIRE_IMMUNITY);
            if(protection > 0) damage = 0;
            if(protection < 0) damage = static_cast<uint8_t>(damage * 2);
            if(!damage)
                status(F("The flames do not affect you."));
            else
                hurt_player(damage);
            fire_splash_monsters();
        } else if(range == 1 && pursuing && !confused && !afraid) {
            uint8_t attacker_dex = info.dexterity;
            uint8_t player_dex = game.dexterity;
            if(roll(static_cast<uint8_t>(attacker_dex * 3 + player_dex + 1)) >=
               player_dex) {
                uint8_t raw = static_cast<uint8_t>(info.strength + roll(3));
                if(monster_effect(monster, MON_WEAKENED))
                    raw = static_cast<uint8_t>((raw + 1) / 2);
                int16_t defense = static_cast<int16_t>(game.defense) +
                    ring_bonus(RING_PROTECTION);
                if(defense < 0) defense = 0;
                uint8_t damage = raw > defense
                    ? static_cast<uint8_t>(raw - defense) : 1;
                hurt_player(damage);
                status(F("The"));
                status(static_cast<MonsterType>(monster.type));
                status(F("hits you!"));
                if(info.flags & MON_VAMPIRE) {
                    game.vamp_drain = static_cast<uint8_t>(game.vamp_drain + 3);
                    if(game.vamp_drain >= game.max_hp)
                        game.vamp_drain = static_cast<uint8_t>(game.max_hp - 1);
                    if(game.hp > player_max_hp())
                        game.hp = player_max_hp();
                    uint8_t maximum = info.health;
                    uint16_t healed = static_cast<uint16_t>(monster.hp) + 3;
                    monster.hp = healed > maximum ? maximum :
                        static_cast<uint8_t>(healed);
                    status(F("Your life force drains away!"));
                }
                if(!session.ended && (info.flags & MON_CONFUSE_HIT) &&
                   roll(4) == 0 && !game.confused &&
                   amulet_bonus(AMULET_CLARITY) <= 0) {
                    game.confused = static_cast<uint8_t>(4 + roll(4));
                    status(F("You feel confused."));
                }
                if(!session.ended && (info.flags & MON_POISON) &&
                   roll(4) == 0 && !game.weakened) {
                    game.weakened = 1;
                    status(F("You feel weaker."));
                }
                if(!session.ended && (info.flags & MON_PARALYZE_HIT) &&
                   roll(4) == 0 && !game.paralyzed &&
                   amulet_bonus(AMULET_IRONBLOOD) <= 0) {
                    game.paralyzed = static_cast<uint8_t>(3 + roll(4));
                    status(F("You are paralyzed!"));
                }
            }
        } else if(afraid || confused || (pursuing && range <= 8) || roll(4) == 0) {
            int8_t dx = 0, dy = 0;
            if(afraid) {
                dx = monster.x < game.px ? -1 : monster.x > game.px ? 1 : 0;
                dy = monster.y < game.py ? -1 : monster.y > game.py ? 1 : 0;
                if(!dx && !dy) dx = roll(2) ? 1 : -1;
            } else if(confused || !pursuing || range > 8) {
                uint8_t direction = roll(4);
                dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
                dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
            } else {
                dx = monster.x < game.px ? 1 : monster.x > game.px ? -1 : 0;
                dy = monster.y < game.py ? 1 : monster.y > game.py ? -1 : 0;
            }
            uint8_t nx = static_cast<uint8_t>(monster.x + dx);
            uint8_t ny = static_cast<uint8_t>(monster.y + dy);
            if(dx && (info.flags & MON_OPENER) &&
               door_at(nx, monster.y) != NONE &&
               !door_open(door_at(nx, monster.y)))
                mark(game.marks[game.floor], OPENED_DOORS,
                     door_at(nx, monster.y));
            else if(dy && (info.flags & MON_OPENER) &&
                    door_at(monster.x, ny) != NONE &&
                    !door_open(door_at(monster.x, ny)))
                mark(game.marks[game.floor], OPENED_DOORS,
                     door_at(monster.x, ny));
            else if(dx && can_monster_move(nx, monster.y))
                monster.x = nx;
            else if(dy && can_monster_move(monster.x, ny))
                monster.y = ny;
        }
    }
    if((info.flags & MON_REGENS) && monster.hp < info.health &&
       roll(8) == 0) {
        uint16_t healed = static_cast<uint16_t>(monster.hp) + 3;
        monster.hp = healed > info.health ? info.health :
            static_cast<uint8_t>(healed);
    }
    age_monster_effects(monster);
}

static void enemy_turn(uint8_t player_speed)
{
    for(uint8_t i = 0; i < MONSTERS && !session.ended; ++i) {
        Monster& monster = game.monsters[i];
        if(!monster.type)
            continue;
        uint8_t speed = monster_info(monster.type).speed;
        if(monster_effect(monster, MON_SLOWED))
            speed = static_cast<uint8_t>(speed * 2);
        if(!speed) speed = 1;
        uint8_t budget = player_speed;
        while(budget >= speed && !session.ended) {
            advance_monster(i);
            budget = static_cast<uint8_t>(budget - speed);
        }
        if(budget && !session.ended && roll(speed) < budget)
            advance_monster(i);
    }
}

void end_turn()
{
    int16_t effective_speed = static_cast<int16_t>(game.speed) +
        amulet_bonus(AMULET_SPEED);
    if(effective_speed < 1) effective_speed = 1;
    uint8_t player_speed = static_cast<uint8_t>(effective_speed);
    if(game.slowed)
        player_speed = static_cast<uint8_t>(player_speed / 2);
    if(!player_speed) player_speed = 1;
    ++game.turns;
    int8_t sustenance = ring_bonus(RING_SUSTENANCE);
    bool hunger_tick = sustenance > 0 ? game.turns % 6 == 0 :
        sustenance < 0 ? game.turns % 3 != 2 : game.turns % 3 == 0;
    if(hunger_tick && game.hunger)
        --game.hunger;
    if(game.hunger == 0 && game.turns % 4 == 0) {
        --game.hp;
        status(F("You are starving!"));
        if(game.hp == 0) {
            finish(0);
            return;
        }
    }
    enemy_turn(player_speed);
    if(game.confused && !--game.confused)
        status(F("You are no longer confused."));
    if(game.paralyzed && !--game.paralyzed)
        status(F("You can move again."));
    if(game.slowed && !--game.slowed)
        status(F("You move normally again."));
    if(game.invisible && !--game.invisible && !player_is_invisible())
        status(F("You become visible again."));
    if(amulet_bonus(AMULET_REGENERATION) > 0 &&
       game.hp < player_max_hp() && roll(20) == 0) {
        heal_player(1);
        status(F("Your amulet restores a little health."));
    }
    if(!game.confused && amulet_bonus(AMULET_CLARITY) < 0 && roll(64) == 0) {
        game.confused = static_cast<uint8_t>(4 + roll(4));
        status(F("Your cursed amulet confuses you."));
    }
    if(!game.paralyzed && amulet_bonus(AMULET_IRONBLOOD) < 0 &&
       roll(64) == 0) {
        game.paralyzed = static_cast<uint8_t>(3 + roll(4));
        status(F("Your cursed amulet paralyzes you."));
    }
}

static void defeat_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    uint8_t killed_type = target.type;
    uint8_t x = target.x, y = target.y;
    mark(game.marks[game.floor], KILLED_MONSTERS, index);
    target.type = NO_MONSTER;
    MonsterInfo info = monster_info(killed_type);
    game.score += static_cast<uint16_t>(5 + info.xp * 3);
    if(killed_type == LORD &&
       !marked(game.marks[game.floor], TAKEN_ITEMS, 15))
        game.ground[15] = {x, y, {YENDOR_AMULET, 1}};
    status(F("You defeat the"));
    status(static_cast<MonsterType>(killed_type), '.');
    gain_xp(info.xp);
}

void attack_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    target.state |= MON_AGGRO;
    MonsterInfo info = monster_info(target.type);
    int16_t dexterity = static_cast<int16_t>(game.dexterity) +
        ring_bonus(RING_DEXTERITY);
    if(dexterity < 0) dexterity = 0;
    uint8_t hit_range = static_cast<uint8_t>(dexterity * 3 + info.dexterity + 1);
    if(roll(hit_range) < info.dexterity) {
        status(F("You miss the"));
        status(static_cast<MonsterType>(target.type), '.');
        end_turn();
        return;
    }
    uint8_t bonus = game.weapon_slot != NONE
        ? item_value(game.inventory[game.weapon_slot]) : 0;
    int16_t strength = static_cast<int16_t>(game.attack) - game.weakened +
        ring_bonus(RING_STRENGTH);
    if(strength < 1) strength = 1;
    int16_t damage_value = strength + bonus + ring_bonus(RING_ATTACK) +
        roll(3) - info.defense;
    if(damage_value < 1) damage_value = 1;
    if(damage_value > 255) damage_value = 255;
    uint8_t damage = static_cast<uint8_t>(damage_value);
    if(damage >= target.hp) {
        defeat_monster(index);
    } else {
        target.hp -= damage;
        status(F("You hit the"));
        status(static_cast<MonsterType>(target.type), '.');
    }
    if(amulet_bonus(AMULET_VAMPIRE) > 0 && game.hp < player_max_hp()) {
        heal_player(1);
        status(F("Your amulet drains a little life."));
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
    end_turn();
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

void change_floor(int8_t delta)
{
    game.floor = static_cast<uint8_t>(game.floor + delta);
    make_floor();
    game.px = delta > 0 ? game.up_x : game.down_x;
    game.py = delta > 0 ? game.up_y : game.down_y;
    visit_room();
    status(F("You take the stairs."));
}

bool use_inventory(uint8_t slot, uint8_t target_slot);

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

bool take_stairs()
{
    if(game.paralyzed) return false;
    if(game.px == game.up_x && game.py == game.up_y) {
        if(game.floor)
            change_floor(-1);
        else
            finish(game.has_amulet ? 1 : 2);
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
            if(!game.hp) finish(0);
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

static void apply_monster_potion(uint8_t type, uint8_t index)
{
    Monster& target = game.monsters[index];
    target.state |= MON_AGGRO;
    uint8_t maximum = monster_info(target.type).health;
    switch(type) {
    case HEALING: {
        uint8_t old_hp = target.hp;
        bool was_weakened = monster_effect(target, MON_WEAKENED) != 0;
        uint8_t healed = static_cast<uint8_t>(maximum / 4 +
            roll(static_cast<uint8_t>(maximum / 2 + 1)));
        uint16_t hp = static_cast<uint16_t>(target.hp) + healed;
        target.hp = hp > maximum ? maximum : static_cast<uint8_t>(hp);
        set_monster_effect(target, MON_WEAKENED, 0);
        if(target.hp > old_hp)
            monster_status(target, F("looks healthier."));
        if(was_weakened)
            monster_status(target, F("regains its strength."));
        break;
    }
    case STRENGTH:
        if(monster_effect(target, MON_WEAKENED))
            monster_status(target, F("regains its strength."));
        else
            status(F("It has no effect."));
        set_monster_effect(target, MON_WEAKENED, 0);
        break;
    case HARMING: {
        uint8_t base = static_cast<uint8_t>(maximum / 8 + 1);
        uint8_t damage = static_cast<uint8_t>(base + roll(base * 2));
        if(damage > 10) damage = 10;
        if(damage >= target.hp)
            defeat_monster(index);
        else {
            target.hp = static_cast<uint8_t>(target.hp - damage);
            monster_status(target, F("is hurt!"));
        }
        break;
    }
    case POISON:
        if(!monster_effect(target, MON_WEAKENED))
            monster_status(target, F("grows weaker."));
        set_monster_effect(target, MON_WEAKENED, 15);
        break;
    case CONFUSION:
        if(!monster_effect(target, MON_CONFUSED))
            monster_status(target, F("becomes confused."));
        set_monster_effect(target, MON_CONFUSED,
                           static_cast<uint8_t>(8 + roll(8)));
        break;
    case PARALYSIS:
        if(!target.stun)
            monster_status(target, F("is paralyzed!"));
        target.stun = static_cast<uint8_t>(3 + roll(4));
        break;
    case SLOWING:
        if(!monster_effect(target, MON_SLOWED))
            monster_status(target, F("slows down."));
        set_monster_effect(target, MON_SLOWED,
                           static_cast<uint8_t>(8 + roll(8)));
        break;
    case INVISIBILITY:
        if(!monster_effect(target, MON_INVISIBLE) &&
           !(monster_info(target.type).flags & MON_NATURAL_INVIS))
            monster_status(target, F("vanishes."));
        set_monster_effect(target, MON_INVISIBLE,
                           static_cast<uint8_t>(12 + roll(4)));
        break;
    case DEXTERITY:
    case EXPERIENCE:
        status(F("It has no effect."));
        break;
    default:
        break;
    }
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
