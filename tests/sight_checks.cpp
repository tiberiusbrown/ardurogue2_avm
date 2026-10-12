#include "game.hpp"
#include "game_internal.hpp"
#include "persistence.hpp"
#include "inventory_view.hpp"
#include "world.hpp"
#include <cstring>
#include <string>
using namespace rogue;
extern void require(bool,const char*);
extern void reset_feature_status();
extern const std::string& feature_status_text();
namespace {
void set_wall(uint8_t x,uint8_t y) { auto n=uint16_t(y)*MAP_W+x; game.walls[n>>3]|=uint8_t(1u<<(n&7)); }
void arena() {
    game={}; session={NONE,DEATH,false}; game.run_seed=game.random_state=71;
    game.player={20,15}; game.up=game.player; game.down={40,15};
    game.hp=game.max_hp=100; game.hunger=255; game.speed=4;
    game.weapon_slot=game.armor_slot=game.offhand_slot=game.amulet_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE; reset_feature_status();
}
}
void check_temporary_sight() {
    arena(); game.inventory[INVENTORY-1]={POTION_SEE_INVISIBLE,2};
    require(!item_type_identified(POTION_SEE_INVISIBLE),"sight starts unidentified");
    require(use_inventory(INVENTORY-1) && game.see_invisible==SEE_INVISIBLE_DURATION-1 &&
        item_value(game.inventory[INVENTORY-1])==1 && item_type_identified(POTION_SEE_INVISIBLE),"drink sight/identify/high slot");
    require(player_light_radius()==1,"sight extended light");
    Monster phantom={{21,15},PHANTOM,12,0,{0,0},0}; game.monsters[0]=phantom;
    require(player_can_see_monster(0) && can_see(phantom.pos),"sight missed natural invisibility");
    set_monster_effect(game.monsters[1]={{21,15},ORC,20,0,{0,0},0},MON_INVISIBLE,12);
    require(player_can_see_monster(1),"sight missed temporary invisibility");
    game.monsters[0].pos={22,15}; require(!can_see(game.monsters[0].pos),"sight exceeded normal radius");
    game.inventory[0]={GLOWING_ORB,0}; game.offhand_slot=0; set_wall(21,15);
    require(!can_see(game.monsters[0].pos),"sight crossed wall");
    game.blindness=2; require(!player_can_see_monster(0) && player_light_radius()==0,"sight overcame blindness");
    game.blindness=0; game.monsters[0]=phantom;
    game.inventory[1]={RING_SEE_INVISIBLE,uint8_t(ITEM_CURSED|1)}; game.ring_slots[0]=1; game.turns=1;
    require(player_can_see_monster(0) && player_can_see_monster(game.monsters[0]),"dose did not suppress cursed sight flicker");
    game.see_invisible=0; require(!player_can_see_monster(0),"cursed ring changed without dose");
    game.inventory[1].info=1; require(player_can_see_monster(0),"ordinary sight ring changed");
    game.ring_slots[0]=NONE; game.monsters[0]={}; game.monsters[1]={};
    game.see_invisible=3;
    require(use_inventory(INVENTORY-1) && game.see_invisible==SEE_INVISIBLE_DURATION-1,"repeat dose did not refresh timer");
    for(unsigned n=1;n<SEE_INVISIBLE_DURATION;++n) { end_turn(); require(game.see_invisible==SEE_INVISIBLE_DURATION-1-n,"sight duration tick"); }
    require(feature_status_text().find("potion sight fades")!=std::string::npos,"sight expiration feedback missing");
    game.see_invisible=7; game.inventory[1]={RING_SEE_INVISIBLE,1}; game.ring_slots[0]=1;
    for(unsigned n=0;n<7;++n) end_turn(); game.monsters[0]=phantom;
    require(!game.see_invisible && player_can_see_monster(0),"expiration canceled sight ring");
    arena(); game.magic=SAVE_MAGIC; game.version=SAVE_VERSION; game.valid=1; game.see_invisible=19;
    Game saved=game; game={}; game=saved;
    require(restore_startup_save(true) && game.see_invisible==19,"sight save/resume");
    make_floor(); require(game.see_invisible==19,"generation reset sight");
    game.player=game.down; require(take_stairs() && game.see_invisible==19,"floor transition sight tick/persistence");
    game.version=27; require(!restore_startup_save(true) && !game.see_invisible,"old save not rejected");
    arena(); game.monsters[0]=phantom; set_monster_effect(game.monsters[0],MON_INVISIBLE,9);
    Monster before=game.monsters[0]; auto rng=game.random_state;
    apply_monster_potion(POTION_SEE_INVISIBLE,0);
    require(!std::memcmp(&before,&game.monsters[0],sizeof before) && game.random_state==rng && !game.see_invisible,"thrown sight modified monster/RNG/player");
    game.inventory[0]={POTION_SEE_INVISIBLE,2}; game.monsters[0].stun=15;
    require(throw_potion(0,1,0) && item_value(game.inventory[0])==1 && potion_identified(POTION_SEE_INVISIBLE) &&
        game.monsters[0].type==PHANTOM && game.monsters[0].hp==before.hp &&
        (monster_flags(game.monsters[0].type)&MON_NATURAL_INVIS) && !game.see_invisible,"thrown sight consumption/collision/identity");
    arena(); game.inventory[0]={POTION_SEE_INVISIBLE,1}; set_wall(21,15);
    require(throw_potion(0,1,0) && !game.inventory[0].type && !potion_identified(POTION_SEE_INVISIBLE),"empty/wall throw identity behavior");
    arena(); for(auto& i:game.inventory) i=make_equipment(DAGGER,0);
    game.inventory[INVENTORY-1]={}; game.ground[GROUND_ITEMS-1]={game.player,{POTION_SEE_INVISIBLE,1}};
    require(take_item(GROUND_ITEMS-1)==PICKUP_TAKEN && game.inventory[INVENTORY-1].type==POTION_SEE_INVISIBLE,"extended ground/inventory pickup");
    InventoryView view(game,nullptr); require(view.position(INVENTORY-1)!=NONE && view.entry_at(view.position(INVENTORY-1))==INVENTORY-1,"extended inventory pagination");
    require(drop_inventory(INVENTORY-1) && item_at(game.player)!=NONE,"extended inventory drop");
    for(unsigned seed=1;seed<=128;++seed) {
        arena(); game.run_seed=uint16_t(seed); game.floor=4; make_floor();
        for(const auto& m:game.monsters) require(!m.type || m.type==PHANTOM,"fifth floor composition");
        game.floor=FLOORS-1; make_floor(); require(!game.ground[GROUND_ITEMS-1].item.type,"Yendor reservation");
    }
}
