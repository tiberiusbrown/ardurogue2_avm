#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include "inventory_view.hpp"
#include <cstring>
#include <initializer_list>
#include <cstdio>

using namespace rogue;
void require(bool, const char*);

namespace {
void arena(uint8_t bow=SHORT_BOW, uint8_t ammo=4) {
    game={}; session={NONE,DEATH,false};
    game.player={10,10}; game.hp=game.max_hp=240; game.hunger=240;
    game.strength=5; game.dexterity=4; game.speed=1; game.level=1;
    game.random_state=1; game.weapon_slot=bow==NO_ITEM ? NONE : 0;
    game.armor_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[0]=make_equipment(bow,0); game.inventory[1]={ARROWS,ammo};
}
void target(uint8_t range, uint8_t type=ORC, uint8_t hp=200) {
    game.monsters[0]={{static_cast<uint8_t>(10+range),10},type,hp,15,{0,0},0};
}
void hit_seed(bool hit) {
    for(unsigned s=1;s<=65535;++s) {
        game.random_state=static_cast<uint16_t>(s);
        bool result=physical_attack_hits(player_ranged_accuracy(game.inventory[0].type),monster_dexterity(game.monsters[0].type));
        if(result==hit) { game.random_state=static_cast<uint16_t>(s); return; }
    }
    require(false,"no arrow hit/miss fixture seed");
}
}

void check_bows() {
    static_assert(sizeof(Item)==2 && sizeof(Game)==774 && SAVE_VERSION==24,"bow enlarged saved state");
    for(unsigned t=0;t<=255;++t) {
        require(is_ammo(t)==(t==ARROWS),"ammo range overlaps another category");
        require(is_bow(t)==(t==SHORT_BOW || t==LONG_BOW),"bow range overlaps another category");
    }
    for(uint8_t n:{1,63,64,127,128,254,255}) {
        Item i={ARROWS,0}; set_item_value(i,n);
        require(item_value(i)==n && !item_is_cursed(i) && item_is_identified(i),"arrow flag/quantity alias");
        arena(); game.inventory[1]=i; identify_item(1);
        require(game.inventory[1].info==n,"identify corrupted arrow quantity");
        game.inventory[2]={SCROLL_ENCHANT,1}; use_inventory(2,1);
        game.inventory[2]={SCROLL_REMOVE_CURSE,1}; use_inventory(2,1);
        require(game.inventory[1].info==n,"scroll changed ammo quantity");
        require(drop_inventory(1),"large arrow drop failed");
        auto slot=item_at(game.player);
        require(slot!=NONE && item_value(game.ground[slot].item)==n && take_item(slot)==PICKUP_TAKEN,
                "large arrow repick failed");
        unsigned units=0; for(auto a:game.inventory) if(is_ammo(a.type)) units+=item_value(a);
        require(units==n,"drop/repick lost quantity bits");
    }
    for(uint8_t incoming:{5,10}) {
        arena(); game.inventory[1]={ARROWS,250}; game.ground[0]={game.player,{ARROWS,incoming}};
        require(take_item(0)==PICKUP_TAKEN && item_value(game.inventory[1])==255,"arrow merge to 255 failed");
        if(incoming==10) require(game.inventory[2].type==ARROWS && item_value(game.inventory[2])==5,"arrow remainder not in second slot");
    }
    arena(); for(auto& i:game.inventory) i={FOOD,63}; game.inventory[1]={ARROWS,250};
    game.ground[0]={game.player,{ARROWS,10}}; Game before=game;
    require(take_item(0)==PICKUP_NEEDS_SWAP && !std::memcmp(&game,&before,sizeof game),"full-pack arrow failure was not atomic");
    game.ground[0].item.info=5;
    require(take_item(0)==PICKUP_TAKEN && game.inventory[1].info==255,"full-pack arrow merge failed");
    arena(); game.inventory[1]={ARROWS,250}; game.ground[0]={game.player,{ARROWS,10}};
    require(drop_inventory(1) && game.ground[0].item.info==255 && game.ground[1].item.info==5,"ground merge overflow failed");

    for(uint8_t bow:{SHORT_BOW,LONG_BOW}) {
        auto m=weapon_definition(bow); auto r=ranged_weapon_definition(bow);
        require(m.minimum_damage==1 && m.maximum_damage==2 && m.accuracy==-2,"bow melee definition wrong");
        require(r.minimum_damage==(bow==SHORT_BOW ? 4 : 5) && r.maximum_damage==(bow==SHORT_BOW ? 7 : 8) &&
            r.accuracy==(bow==SHORT_BOW ? 1 : 0) && r.range==(bow==SHORT_BOW ? 5 : 6),"bow ranged definition wrong");
        for(uint8_t d:{r.range,static_cast<uint8_t>(r.range+1)}) {
            arena(bow); target(d); hit_seed(true);
            require(throw_or_shoot(1,1,0) && game.inventory[1].info==3 && game.turns==1,"shot did not consume turn/arrow");
            require((game.monsters[0].hp<200)==(d==r.range),"bow range boundary wrong");
        }
        for(int8_t enchant:{-5,0,5}) for(uint8_t strength:{1,5,20}) for(unsigned seed=1;seed<150;++seed) {
            arena(bow); target(3); game.strength=strength; set_equipment_enchant(game.inventory[0],enchant);
            game.level=10; game.inventory[2]={RING_ATTACK,3}; game.ring_slots[0]=2;
            require(player_ranged_accuracy(bow)==game.dexterity+3+3+r.accuracy,"ranged accuracy leaked enchant or omitted bonus");
            game.random_state=seed;
            bool hit=physical_attack_hits(player_ranged_accuracy(bow),monster_dexterity(ORC));
            uint8_t damage=0;
            if(hit) {
                uint8_t rolled=weapon_damage_roll(r.minimum_damage,r.maximum_damage,enchant);
                uint8_t raw=physical_raw_damage(rolled,strength);
                uint8_t absorbed=armor_absorption(monster_armor(ORC),0);
                damage=physical_damage_after_armor(raw,absorbed);
            }
            game.random_state=seed;
            bool accepted=throw_or_shoot(1,1,0);
            if(!accepted || game.monsters[0].hp!=200-damage) std::fprintf(stderr,"bow=%u enchant=%d str=%u seed=%u hit=%u damage=%u hp=%u accepted=%u rng=%u\n",bow,enchant,strength,seed,hit,damage,game.monsters[0].hp,accepted,game.random_state);
            require(accepted && game.monsters[0].hp==200-damage,"shot diverged from physical combat pipeline");
            require(game.monsters[0].state & MON_AGGRO,"ranged miss failed to aggro");
        }
    }
    for(bool door:{false,true}) for(bool open:{false,true}) {
        arena(); target(4); hit_seed(true);
        if(door) { game.door_count=1; game.doors[0]={{12,10}}; if(open) open_door(0); }
        else if(!open) game.walls[(10*MAP_W+12)>>3]|=1u<<(12&7);
        require(throw_or_shoot(1,1,0) && game.inventory[1].info==3,"blocker shot did not consume ammo");
        require((game.monsters[0].hp<200)==open,"wall/door passage wrong");
    }
    for(bool hit:{false,true}) {
        arena(SHORT_BOW,1); target(2); hit_seed(hit);
        game.monsters[1]={{14,10},ORC,200,15,{0,0},0};
        require(throw_or_shoot(1,1,0) && game.inventory[1].type==NO_ITEM && game.monsters[1].hp==200,
            "miss penetrated first monster or final arrow remained");
        require((game.monsters[0].hp<200)==hit,"hit/miss fixture wrong");
    }
    arena(); require(!throw_or_shoot(1,1,1) && !throw_or_shoot(1,0,0) && game.inventory[1].info==4,"noncardinal shot accepted");
    require(throw_or_shoot(1,0,-1) && game.inventory[1].info==3,"empty shot consumption failed");
    for(uint8_t d:{3,4}) {
        arena(NO_ITEM); target(d); hit_seed(true);
        require(throw_or_shoot(1,1,0) && (game.monsters[0].hp<200)==(d==3) && game.inventory[1].info==3,"thrown arrow range/damage wrong");
    }
    arena(LONG_BOW); target(1,LORD,1); hit_seed(true); game.floor=15;
    require(throw_or_shoot(1,1,0) && game.monsters[0].type==NO_MONSTER && game.score && game.level>1 &&
            game.ground[15].item.type==YENDOR_AMULET,"ranged Lord kill bypassed XP/drop");
    arena(); game.inventory[2]={POTION_POISON,1};
    InventoryView view(game,is_throwable_or_shootable);
    require(view.first_slot()==1 && inventory_group(ARROWS)==AMMO && AMMO<POTIONS && view.count()==4,
            "projectile picker ammo priority/eligibility wrong");

    // Hold type/equipment/placement seeds fixed while changing only the independent
    // run seed used by AMMO_QUANTITY. Every non-ammo instance must be identical.
    using namespace rogue::generation;
    unsigned changed=0;
    for(uint16_t seed=1;seed<=256;++seed) {
        arena(); populate_items(seed,seed^0x4a71); Game original=game;
        game.run_seed=seed; std::memset(game.ground,0,sizeof game.ground);
        populate_items(seed,seed^0x4a71);
        require(game.random_state==original.random_state,"ammo generation used gameplay RNG");
        for(unsigned s=0;s<GROUND_ITEMS;++s) {
            auto a=original.ground[s], b=game.ground[s];
            require(a.pos==b.pos && a.item.type==b.item.type,"ammo quantity perturbed supplies/placement");
            if(is_ammo(a.item.type)) { require(b.item.info>=ARROW_BUNDLE_MIN && b.item.info<=ARROW_BUNDLE_MAX,"arrow bundle outside specified limits"); changed+=a.item.info!=b.item.info; }
            else require(a.item.info==b.item.info,"ammo quantity perturbed equipment/other instance stream");
        }
    }
    require(changed>30,"ammo quantity isolation fixture had no independent variation");
    // Every one of the 72 category outcomes is reached through the real generator.
    unsigned food=0,ammo=0;
    for(unsigned chance=0;chance<72;++chance) {
        uint16_t seed=1;
        for(;;++seed) { uint16_t state=seed; if(next_random(state)%72==chance) break; }
        arena(); populate_items(seed,0x1234);
        auto type=game.ground[0].item.type;
        food+=type==FOOD; ammo+=type==ARROWS;
        require((chance<FOOD_WEIGHT)==(type==FOOD) && (chance>=FOOD_WEIGHT && chance<20)==is_ammo(type),
                "category remapping changed a later supply boundary");
    }
    require(food==FOOD_WEIGHT && ammo==AMMO_WEIGHT,"exact Food/Arrow category allocation wrong");
    for(uint8_t floor=0;floor<FLOORS;++floor) {
        unsigned short_bows=0,long_bows=0;
        for(uint8_t chance=0;chance<100;++chance) {
            auto type=weapon_type_for_roll(chance,floor);
            short_bows+=type==SHORT_BOW; long_bows+=type==LONG_BOW;
            if(chance>=75) {
                unsigned heavy=floor<4 ? 2 : floor<8 ? floor : floor<10 ? 10 : 20;
                require(type==(chance<100-heavy ? MACE : TWO_HANDED_SWORD),"bow changed depth-dependent heavy region");
            }
        }
        require(short_bows==6 && long_bows==4,"bow subtype weights wrong");
    }
}
