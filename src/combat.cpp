#include "game.hpp"
#include "game_internal.hpp"
#include "status.hpp"
#include "world.hpp"

namespace rogue {

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

uint8_t monster_effect(const Monster& monster, MonsterEffect effect)
{
    uint8_t packed = monster.effects[effect >> 1];
    return effect & 1 ? packed >> 4 : packed & 0x0f;
}

void set_monster_effect(Monster& monster, MonsterEffect effect,
                               uint8_t duration)
{
    uint8_t& packed = monster.effects[effect >> 1];
    if(effect & 1)
        packed = static_cast<uint8_t>((packed & 0x0f) | (duration << 4));
    else
        packed = static_cast<uint8_t>((packed & 0xf0) | duration);
}

void monster_status(const Monster& monster,
                           const char PROGMEM* message)
{
    status(F("The"));
    status(static_cast<MonsterType>(monster.type));
    status(message);
}

__attribute__((noinline)) static void age_monster_effects(Monster& monster)
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

static uint8_t distance(Position a, Position b)
{
    uint8_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    uint8_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx + dy;
}

uint8_t monster_at(Position pos)
{
    for(uint8_t i = 0; i < MONSTERS; ++i)
        if(game.monsters[i].type && game.monsters[i].pos == pos)
            return i;
    return NONE;
}

static bool can_monster_move(uint8_t x, uint8_t y)
{
    if(blocked(x, y)) return false;
    Position pos = {x, y};
    return pos != game.player && monster_at(pos) == NONE;
}

void hurt_player(uint8_t damage)
{
    game.hp = damage >= game.hp ? 0 : static_cast<uint8_t>(game.hp - damage);
    if(!game.hp)
        finish(DEATH);
}

uint8_t player_strength()
{
    int16_t value = static_cast<int16_t>(game.strength) +
        ring_bonus(RING_STRENGTH) - game.weakened;
    if(value < 1) value = 1;
    if(value > 255) value = 255;
    return static_cast<uint8_t>(value);
}

uint8_t player_dexterity()
{
    return clamp_combat_stat(static_cast<int16_t>(game.dexterity) +
                             ring_bonus(RING_DEXTERITY));
}

uint8_t player_accuracy()
{
    // Type supplies accuracy; enchantment affects damage rolls only.
    uint8_t experience = game.level ? (game.level - 1) / 3 : 0;
    uint8_t type = game.weapon_slot < INVENTORY ? game.inventory[game.weapon_slot].type : NO_ITEM;
    return clamp_combat_stat(static_cast<int16_t>(player_dexterity()) +
                             experience + ring_bonus(RING_ATTACK) + weapon_definition(type).accuracy);
}

uint8_t player_armor_rating()
{
    uint8_t rating = game.armor_slot < INVENTORY
        ? armor_definition(game.inventory[game.armor_slot].type).rating : 0;
    return effective_armor_rating(rating, ring_bonus(RING_PROTECTION));
}

int8_t player_armor_enchant()
{
    return game.armor_slot < INVENTORY &&
        is_armor(game.inventory[game.armor_slot].type)
        ? equipment_enchant(game.inventory[game.armor_slot]) : 0;
}

void player_take_magic_damage(uint8_t damage, uint8_t power)
{
    if(damage)
        hurt_player(magic_damage_after_save(damage,
            magic_save(game.magic_resistance, power)));
}

void player_take_fire_damage(uint8_t damage, uint8_t power)
{
    int8_t fire = ring_bonus(RING_FIRE_IMMUNITY);
    if(fire > 0) {
        status(F("The flames do not affect you."));
        return;
    }
    if(fire < 0) damage = saturating_double(damage);
    player_take_magic_damage(damage, power);
}

static void leave_yendor(const Monster& monster)
{
    if(monster.type == LORD)
        game.ground[15] = {monster.pos, {YENDOR_AMULET, 1}};
}

void damage_monster(uint8_t index, uint8_t damage, bool player_attack)
{
    Monster& target = game.monsters[index];
    if(!target.type) return;
    if(player_attack) target.state |= MON_AGGRO;
    if(damage >= target.hp) {
        if(player_attack) defeat_monster(index);
        else {
            leave_yendor(target);
            target.type = NO_MONSTER;
        }
    } else
        target.hp = static_cast<uint8_t>(target.hp - damage);
}

void fire_burst_damage(Position center, bool player_attack, uint8_t radius)
{
    for(uint8_t i = 0; i < MONSTERS; ++i) {
        Monster& target = game.monsters[i];
        if(!target.type || target.type == DRAGON)
            continue;
        uint8_t dx = target.pos.x > center.x ? target.pos.x - center.x :
            center.x - target.pos.x;
        uint8_t dy = target.pos.y > center.y ? target.pos.y - center.y :
            center.y - target.pos.y;
        if(dx > radius || dy > radius)
            continue;
        uint8_t damage = static_cast<uint8_t>(8 + roll(8));
        damage_monster(i, damage, player_attack);
    }
    uint8_t px = game.player.x > center.x ? game.player.x - center.x :
        center.x - game.player.x;
    uint8_t py = game.player.y > center.y ? game.player.y - center.y :
        center.y - game.player.y;
    if(player_attack && px <= radius && py <= radius) {
        uint8_t damage = static_cast<uint8_t>(8 + roll(8));
        uint8_t old_hp = game.hp;
        player_take_fire_damage(damage, 8);
        if(game.hp < old_hp)
            status(F("You are caught in the flames!"));
    }
}

__attribute__((noinline)) static bool fire_line_clear(const Monster& monster)
{
    int8_t dx = monster.pos.x == game.player.x ? 0 :
        monster.pos.x < game.player.x ? 1 : -1;
    int8_t dy = monster.pos.y == game.player.y ? 0 :
        monster.pos.y < game.player.y ? 1 : -1;
    if(dx && dy)
        return false;
    uint8_t range = distance(monster.pos, game.player);
    if(!range || range > 5)
        return false;
    RayResult ray = scan_ray(monster.pos, dx, dy, range);
    return ray.steps == range && ray.monster == NONE && !ray.blocker;
}

// Movement's coordinate and door temporaries are not live during fire animation.
__attribute__((noinline)) static void move_monster(
    Monster& monster, uint16_t flags, bool afraid, bool confused,
    bool pursuing, uint8_t range)
{
    int8_t dx = 0, dy = 0;
    if(afraid) {
        dx = monster.pos.x < game.player.x ? -1 : monster.pos.x > game.player.x ? 1 : 0;
        dy = monster.pos.y < game.player.y ? -1 : monster.pos.y > game.player.y ? 1 : 0;
        if(!dx && !dy) dx = roll(2) ? 1 : -1;
    } else if(confused || !pursuing || range > 8) {
        uint8_t direction = roll(4);
        dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
        dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
    } else {
        dx = monster.pos.x < game.player.x ? 1 : monster.pos.x > game.player.x ? -1 : 0;
        dy = monster.pos.y < game.player.y ? 1 : monster.pos.y > game.player.y ? -1 : 0;
    }
    uint8_t nx = static_cast<uint8_t>(monster.pos.x + dx);
    uint8_t ny = static_cast<uint8_t>(monster.pos.y + dy);
    if(dx && nx < MAP_W && (flags & MON_OPENER) &&
       door_at({nx, monster.pos.y}) != NONE &&
       !door_open(door_at({nx, monster.pos.y})))
        open_door(door_at({nx, monster.pos.y}));
    else if(dy && ny < MAP_H && (flags & MON_OPENER) &&
            door_at({monster.pos.x, ny}) != NONE &&
            !door_open(door_at({monster.pos.x, ny})))
        open_door(door_at({monster.pos.x, ny}));
    else if(dx && can_monster_move(nx, monster.pos.y))
        monster.pos.x = nx;
    else if(dy && can_monster_move(monster.pos.x, ny))
        monster.pos.y = ny;
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
        uint8_t range = distance(monster.pos, game.player);
        bool pursuing = (info.flags & MON_MEAN) || (monster.state & MON_AGGRO);
        if(player_is_invisible() && !(info.flags & MON_SEE_INVIS))
            pursuing = false;
        if(!afraid && pursuing && !confused && (info.flags & MON_FIRE_BREATH) &&
           fire_line_clear(monster) && roll(2)) {
            status(F("The"));
            status(static_cast<MonsterType>(monster.type));
            status(F("breathes fire!"));
            int8_t dx = monster.pos.x == game.player.x ? 0 :
                monster.pos.x < game.player.x ? 1 : -1;
            int8_t dy = monster.pos.y == game.player.y ? 0 :
                monster.pos.y < game.player.y ? 1 : -1;
            animate_ray(monster.pos, dx, dy, range);
            animate_fire_burst(game.player);
            uint8_t damage = static_cast<uint8_t>(8 + roll(8));
            player_take_fire_damage(damage, 12);
            fire_burst_damage(game.player, false);
        } else if(range == 1 && pursuing && !confused && !afraid) {
            if(physical_attack_hits(info.dexterity, player_dexterity())) {
                uint8_t raw = static_cast<uint8_t>(info.strength + roll(3));
                if(monster_effect(monster, MON_WEAKENED))
                    raw = static_cast<uint8_t>((raw + 1) / 2);
                uint8_t damage = physical_damage_after_armor(raw,
                    armor_absorption(player_armor_rating(), player_armor_enchant()));
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
        } else if(afraid || confused || (pursuing && range <= 8) ||
                  (monster.type == BAT && !(monster.state & MON_AGGRO)) ||
                  roll(4) == 0) {
            // Passive bats try one random direction each turn, as in ArduRogue.
            // Movement rejects occupied tiles, including the player's tile.
            move_monster(monster, info.flags, afraid, confused, pursuing, range);
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
    int16_t effective_speed = static_cast<int16_t>(game.speed) -
        amulet_bonus(AMULET_SPEED);
    if(game.slowed)
        effective_speed = static_cast<int16_t>(effective_speed * 2);
    if(effective_speed < 1) effective_speed = 1;
    uint8_t player_speed = static_cast<uint8_t>(effective_speed);
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
            finish(DEATH);
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

void defeat_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    uint8_t killed_type = target.type;
    leave_yendor(target);
    target.type = NO_MONSTER;
    MonsterInfo info = monster_info(killed_type);
    game.score += static_cast<uint16_t>(5 + info.xp * 3);
    status(F("You defeat the"));
    status(static_cast<MonsterType>(killed_type), '.');
    gain_xp(info.xp);
}

static void attack_monster(uint8_t index)
{
    Monster& target = game.monsters[index];
    target.state |= MON_AGGRO;
    MonsterInfo info = monster_info(target.type);
    if(!physical_attack_hits(player_accuracy(), info.dexterity)) {
        status(F("You miss the"));
        status(static_cast<MonsterType>(target.type), '.');
        return;
    }
    bool armed = game.weapon_slot < INVENTORY &&
        is_weapon(game.inventory[game.weapon_slot].type);
    WeaponDefinition weapon = weapon_definition(armed ? game.inventory[game.weapon_slot].type : NO_ITEM);
    uint8_t weapon_roll = weapon_damage_roll(
        weapon.minimum_damage, weapon.maximum_damage,
        armed ? equipment_enchant(game.inventory[game.weapon_slot]) : 0);
    uint8_t raw = physical_raw_damage(weapon_roll, player_strength());
    uint8_t damage = physical_damage_after_armor(raw,
        armor_absorption(info.armor, 0));
    if(damage >= target.hp) {
        defeat_monster(index);
    } else {
        target.hp -= damage;
        status(F("You hit the"));
        status(static_cast<MonsterType>(target.type), '.');
    }
    int8_t vampire_bonus = amulet_bonus(AMULET_VAMPIRE);
    if(vampire_bonus > 0 && game.hp < player_max_hp()) {
        heal_player(1);
        status(F("Your amulet drains a little life."));
    } else if(vampire_bonus < 0) {
        hurt_player(1);
        status(F("Your amulet drains your life."));
    }
}

// Resolve movement and combat before entering the deeper enemy-turn stack.
__attribute__((noinline)) static bool apply_move(int8_t dx, int8_t dy)
{
    if(game.paralyzed) {
        status(F("You cannot move!"));
        return true;
    }
    bool confused_direction = game.confused && roll(2) == 0;
    if(confused_direction) {
        uint8_t direction = roll(4);
        dx = direction == 0 ? 1 : direction == 1 ? -1 : 0;
        dy = direction == 2 ? 1 : direction == 3 ? -1 : 0;
    }
    int16_t x = static_cast<int16_t>(game.player.x) + dx;
    int16_t y = static_cast<int16_t>(game.player.y) + dy;
    if(wall_at(x, y)) {
        status(F("A wall blocks your way."));
        return confused_direction;
    }
    Position destination = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
    uint8_t door = door_at(destination);
    if(door != NONE && !door_open(door)) {
        open_door(door);
        status(F("You open the door."));
        return true;
    }
    uint8_t monster = monster_at(destination);
    if(monster != NONE) {
        attack_monster(monster);
        return !session.ended;
    }
    game.player = destination;
    return true;
}

void move_player(int8_t dx, int8_t dy)
{
    if(apply_move(dx, dy))
        end_turn();
}

void apply_monster_potion(uint8_t type, uint8_t index)
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

} // namespace rogue
