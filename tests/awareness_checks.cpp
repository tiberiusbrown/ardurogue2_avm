#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <initializer_list>

using namespace rogue;
void require(bool, const char*);

void check_monster_awareness()
{
    static_assert(sizeof(Monster)==8 && offsetof(Game,see_invisible)==813 + 4*(GROUND_ITEMS-16) + 2*(INVENTORY-16) && SAVE_VERSION==29);
    game={}; session={}; game.valid=1; game.version=SAVE_VERSION;
    game.hp=game.max_hp=100; game.hunger=255; game.speed=4;
    game.player={10,10}; game.random_state=123; game.strength=4; game.dexterity=4;
    game.weapon_slot=game.armor_slot=game.offhand_slot=game.amulet_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    auto senses=[&](uint8_t type, uint8_t x, uint8_t y) {
        Monster m={{x,y},type,monster_health(type),0,{0,0},0};
        auto rng=game.random_state;
        bool result=monster_detects_player(m,monster_flags(type));
        require(game.random_state==rng,"detection consumed RNG");
        return result;
    };
    for(uint8_t light : {uint8_t(NO_ITEM),uint8_t(TORCH),uint8_t(LANTERN),uint8_t(GLOWING_ORB)}) {
        game.inventory[0]={light,200}; game.offhand_slot=0;
        if(light==GLOWING_ORB) { game.inventory[1]={AMULET_BRILLIANCE,1}; game.amulet_slot=1; }
        else game.amulet_slot=NONE;
        uint8_t radius=player_light_radius();
        require(radius==(light==NO_ITEM ? 1 : light==TORCH ? 3 : light==LANTERN ? 4 : 6),"awareness fixture radius");
        require(senses(ORC,uint8_t(11+radius),10),"enemy failed to detect at lit boundary");
        require(!senses(ORC,uint8_t(12+radius),10),"unalerted enemy detected outside boundary");
        require(senses(ORC,11,10),"adjacent hostile oblivious");
        game.blindness=4;
        require(senses(ORC,uint8_t(11+radius),10),"player blindness extinguished monster-visible illumination");
        game.blindness=0;
        game.doors[0]={{11,10}}; game.door_count=1;
        require(!senses(ORC,12,10),"closed door did not block initial detection");
        open_door(0); require(senses(ORC,12,10),"open door blocks detection");
        game.door_count=0;
        game.walls[(10*MAP_W+11)>>3]|=uint8_t(1u<<(11&7));
        require(!senses(ORC,12,10),"wall did not block awareness");
        carve(11,10);
    }
    game.offhand_slot=game.amulet_slot=NONE;
    require(senses(DRAGON,14,10) && !senses(DRAGON,15,10),"dragon dark sensing");
    require(senses(PHANTOM,13,10) && !senses(PHANTOM,14,10),"phantom dark sensing");
    require(senses(LORD,16,10),"Lord dark sensing");
    require(!senses(BAT,11,10) && !senses(MIMIC,11,10),"passive/disguised monster was alerted");
    game.invisible=10;
    require(!senses(ORC,11,10) && senses(PHANTOM,11,10),"initial invisibility semantics");
    game.invisible=0;
    game.monsters[0]={{12,10},ORC,100,0,{0,0},0};
    end_turn(); require(game.monsters[0].state&MON_AGGRO,"detection did not persist");
    game.monsters[0].pos={15,10}; game.blindness=10;
    end_turn(); require((game.monsters[0].state&MON_AGGRO) && game.monsters[0].pos.x==14,"darkness erased pursuit");
    game.monsters[0].state=0; game.monsters[0].pos={20,10};
    damage_monster(0,1,true);
    require(game.monsters[0].state&MON_AGGRO,"player attack did not alert immediately");
    blind_monster(game.monsters[0],8);
    end_turn(); require(game.monsters[0].state&MON_AGGRO,"temporary impairment erased awareness");
}
