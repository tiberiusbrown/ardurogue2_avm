#include "spells.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "features.hpp"
#include "world.hpp"
#include <cstring>
#include <initializer_list>
using namespace rogue;
extern void require(bool,const char*);
namespace {
void arena(uint16_t seed=71) {
    game={}; session={NONE,DEATH,false};
    game.player={20,15}; game.up={2,2}; game.down={40,25};
    game.random_state=game.run_seed=seed; game.hp=game.max_hp=200;
    game.level=1; game.hunger=255; game.speed=1; game.mana=16; game.known_spells=0xffff;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
    game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
}
void monster(uint8_t slot,Position pos,uint8_t hp=100,uint8_t type=GOBLIN) {
    game.monsters[slot]={pos,type,hp,15,{0,0},0};
}
void wall(Position p) { game.walls[(p.y*MAP_W+p.x)>>3]|=uint8_t(1u<<(p.x&7)); }
void wand(WandModifier modifier=WAND_NORMAL,uint8_t charges=3) {
    game.inventory[0]={WAND_LIGHTNING,0}; set_wand_modifier(game.inventory[0],modifier); set_wand_charges(game.inventory[0],charges);
}
void metadata_and_learning() {
    static_assert(SPELL_COUNT==16 && spell_bit(BLOOD_BOLT)==0x8000 && sizeof(Game::known_spells)==2 && sizeof(Item)==2);
    const uint8_t costs[]={2,2,2,3,2,4,4,3,4,4,4,4,4,4,4,2};
    const uint8_t turns[]={40,40,40,40,40,60,60,40,60,60,60,60,60,60,60,60};
    for(uint8_t s=0;s<SPELL_COUNT;++s) {
        require(spell_cost(s)==costs[s] && spell_study_turns(s)==turns[s] &&
                spell_hp_cost(s)==(s==BLOOD_BOLT ? 3 : 0),"explicit spell metadata changed");
        require(std::strcmp(spell_name(s),"Unknown") && std::strcmp(spell_description(s),"Unknown spell."),"missing spell text");
        if(!expansion_spell(s)) continue;
        arena(); game.known_spells=1; game.inventory[0]={SPELLBOOK,spellbook_for_spell(s)};
        require(begin_study(0,s),"new spellbook cannot be studied");
        for(unsigned t=0;t<turns[s];++t) require(advance_study(),"new spell study turn rejected");
        require(game.known_spells==(1|spell_bit(s)) && game.inventory[0].type==SPELLBOOK && !studying() && game.turns==turns[s],"new spellbook learning lifecycle");
        arena(); Game before=game;
        require(!cast_spell(s,2,0) && !std::memcmp(&before,&game,sizeof game),"invalid direction spent resources");
        game.mana=costs[s]-1; before=game;
        require(!cast_spell(s,1,0) && !std::memcmp(&before,&game,sizeof game),"insufficient MP spent resources");
        game.mana=16; game.known_spells&=~spell_bit(s); before=game;
        require(!cast_spell(s,1,0) && !std::memcmp(&before,&game,sizeof game),"unknown spell spent resources");
    }
    require(std::strstr(spell_description(BLOOD_BOLT),"3 HP")!=nullptr,"health sacrifice hidden in description");
}
void fireburst() {
    for(uint16_t seed=1;seed<=64;++seed) {
        arena(seed); monster(0,{22,15}); monster(1,{23,16}); monster(2,{24,15}); monster(3,{22,14},100,DRAGON);
        cast_spell(FIREBURST,1,0);
        require(game.mana==12 && game.turns==1 && game.monsters[0].hp>=94 && game.monsters[0].hp<=97 &&
                game.monsters[1].hp>=94 && game.monsters[1].hp<=97 && game.monsters[2].hp==100 && game.monsters[3].hp==100 && game.hp==200,"Fireburst area/range/immunity");
    }
    arena(); wall({23,15}); monster(0,{22,16}); cast_spell(FIREBURST,1,0);
    require(game.monsters[0].hp<100,"Fireburst failed wall impact/end tile");
    arena(); monster(0,{21,15}); cast_spell(FIREBURST,1,0); require(game.hp<200 && game.hp>=194,"Fireburst self damage");
    arena(); monster(0,{21,15}); game.fire_resistance=10; cast_spell(FIREBURST,1,0); require(game.hp==200,"Fireburst player fire immunity");
    arena(); monster(0,{22,15},1); monster(1,{23,16},1); cast_spell(FIREBURST,1,0);
    require(!game.monsters[0].type && !game.monsters[1].type && game.score && game.xp,"Fireburst kill rewards");
    arena(); cast_spell(FIREBURST,1,0); require(game.turns==1 && game.mana==12,"empty Fireburst cost");
}
void phase() {
    arena(); monster(0,{21,15}); monster(1,{22,15});
    game.features[0]={{21,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    cast_spell(PHASE_STEP,1,0);
    require(game.player==Position{23,15} && game.hp==200 && feature_is_hidden(game.features[0].id) &&
            game.monsters[0].pos==Position{21,15} && game.monsters[1].pos==Position{22,15},"Phase Step intervening monsters/traps");
    arena(); monster(0,{23,15}); cast_spell(PHASE_STEP,1,0); require(game.player==Position{22,15},"Phase Step occupied preferred landing fallback");
    arena(); wall({23,15}); cast_spell(PHASE_STEP,1,0); require(game.player==Position{22,15},"Phase Step wall fallback");
    arena(); wall({22,15}); cast_spell(PHASE_STEP,1,0); require(game.player==Position{20,15} && game.mana==12 && game.turns==1,"Phase Step crossed blocked path or failed cost");
    arena(); game.doors[0]={{21,15}}; game.door_count=1; cast_spell(PHASE_STEP,1,0); require(game.player==Position{20,15},"Phase Step crossed closed door");
    for(uint8_t x : {uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(3)}) {
        arena(); game.player={x,15}; cast_spell(PHASE_STEP,-1,0);
        require(game.player.x==(x<2 ? x : 0),"Phase Step map boundary/preferred distance");
    }
    arena(); game.features[0]={{23,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    game.ground[0]={{23,15},{FOOD,1}}; cast_spell(PHASE_STEP,1,0);
    require(game.player==Position{23,15} && game.hp<200 && !feature_is_hidden(game.features[0].id) && item_at(game.player)==0,"Phase Step landing feature/item");
    require(take_item(0)==PICKUP_TAKEN,"Phase Step landing pickup unavailable");
    arena(); game.down={23,15}; cast_spell(PHASE_STEP,1,0); require(game.floor==0 && game.player==game.down,"forced landing descended stairs automatically");
    arena(); game.features[0]={{23,15},FEATURE_TELEPORT_TRAP}; cast_spell(PHASE_STEP,1,0); require(game.player!=Position{23,15},"Phase Step teleport trap ignored");
    arena(); game.hp=1; game.features[0]={{23,15},FEATURE_SPIKE_TRAP}; cast_spell(PHASE_STEP,1,0);
    require(session.ended && !game.hp && !game.turns && game.mana==12,"Phase Step fatal landing processed enemy turn");
}
void lightning() {
    for(uint16_t seed=1;seed<=64;++seed) {
        arena(seed); monster(0,{22,15}); monster(1,{24,16}); monster(2,{24,14});
        cast_spell(CHAIN_LIGHTNING,1,0);
        require(game.monsters[0].hp>=95 && game.monsters[0].hp<=97 && game.monsters[1].hp>=96 && game.monsters[1].hp<=97 && game.monsters[2].hp==100,"Chain Lightning damage/tie-breaking");
    }
    arena(); monster(0,{22,15},1); monster(1,{23,16}); cast_spell(CHAIN_LIGHTNING,1,0);
    require(!game.monsters[0].type && game.monsters[1].hp<100 && game.score,"primary death lost jump origin/rewards");
    arena(); monster(0,{22,15}); monster(1,{24,16}); monster(2,{23,16}); cast_spell(CHAIN_LIGHTNING,1,0);
    require(game.monsters[1].hp==100 && game.monsters[2].hp<100,"nearest target lost to lower slot");
    arena(); monster(0,{22,15}); monster(1,{25,16}); cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp==100,"lightning jump exceeded Chebyshev two");
    for(uint8_t barrier=0;barrier<2;++barrier) {
        arena(); monster(0,{22,15}); monster(1,{24,15});
        if(barrier) { game.doors[0]={{23,15}}; game.door_count=1; } else wall({23,15});
        require(lightning_target({22,15},1)==NONE,"lightning jumped across blocker");
        cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp==100,"lightning wall/door transmission");
    }
    arena(); monster(0,{22,15}); monster(1,{23,16},100,PHANTOM); cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp==100,"lightning disclosed invisible secondary");
    arena(); monster(0,{22,15}); monster(1,{23,16},100,MIMIC); cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp<100 && (game.monsters[1].state&MON_AGGRO),"lightning failed to strike and reveal disguised mimic");
    arena(); monster(0,{22,15}); monster(1,{23,16},100,MIMIC); game.monsters[1].state|=MON_AGGRO; cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp<100,"lightning skipped revealed mimic");
    arena(); monster(0,{22,15}); monster(1,{23,16}); game.blindness=10; cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp==100,"lightning ignored blindness");
    arena(); monster(0,{25,15}); monster(1,{26,16}); cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[1].hp==100,"lightning secondary outside actual light");
    arena(); monster(0,{21,16}); cast_spell(CHAIN_LIGHTNING,1,0); require(game.monsters[0].hp==100 && game.mana==12 && game.turns==1,"empty ray created arc");
    arena(); wall({23,15}); require(!line_of_effect({22,15},{23,16}),"jump cut a blocked diagonal corner");
}
void slumber_and_blood() {
    bool three=false,four=false;
    for(uint16_t seed=1;seed<=64;++seed) {
        arena(seed); monster(0,{21,15}); game.monsters[0].stun=0; game.speed=monster_speed(GOBLIN);
        uint8_t rolled=3+roll(2); game.random_state=seed; three|=rolled==3; four|=rolled==4;
        cast_spell(SLUMBER,1,0);
        require(game.monsters[0].stun==rolled-1 && game.monsters[0].hp==100 && game.hp==200,"Slumber duration or immediate response timing");
        for(uint8_t t=1;t<rolled;++t) { end_turn(); require(game.monsters[0].stun==rolled-1-t && game.hp==200,"Slumber expired early"); }
    }
    require(three && four,"Slumber did not roll exact 3-4 range");
    arena(); monster(0,{21,15}); game.speed=monster_speed(GOBLIN); game.monsters[0].stun=6; cast_spell(SLUMBER,1,0); require(game.monsters[0].stun==5,"Slumber stacked or reduced existing stun");
    arena(); monster(0,{25,15}); cast_spell(SLUMBER,1,0); require(game.monsters[0].stun==15 && game.mana==12,"Slumber range four/miss");
    for(uint16_t seed=1;seed<=64;++seed) {
        arena(seed); monster(0,{22,15}); game.magic_resistance=255; game.fire_resistance=10;
        game.inventory[1]=make_equipment(TITAN_PLATE,3); game.armor_slot=1;
        cast_spell(BLOOD_BOLT,1,0);
        require(game.hp==197 && game.mana==16-spell_cost(BLOOD_BOLT) && game.monsters[0].hp>=91 && game.monsters[0].hp<=94,"Blood Bolt range/cost bypassed mitigation");
    }
    arena(); wall({21,15}); cast_spell(BLOOD_BOLT,1,0); require(game.hp==197 && game.mana==16-spell_cost(BLOOD_BOLT) && game.turns==1,"Blood Bolt blocked cost");
    arena(); monster(0,{22,15},1); cast_spell(BLOOD_BOLT,1,0); require(game.hp==197 && game.score && !game.monsters[0].type,"Blood Bolt refunded sacrifice on kill");
    for(uint8_t hp=1;hp<=3;++hp) {
        arena(); game.hp=hp; monster(0,{22,15}); cast_spell(BLOOD_BOLT,1,0);
        require(session.ended && !game.hp && !game.turns && game.mana==16-spell_cost(BLOOD_BOLT) && game.monsters[0].hp==100,"fatal Blood Bolt emitted bolt/enemy turn");
    }
    arena(); game.hp=4; cast_spell(BLOOD_BOLT,1,0); require(game.hp==1 && game.turns==1,"low HP Blood Bolt incorrectly forbidden");
    arena(); game.hp=3; game.inventory[1]={AMULET_PHOENIX_HEART,1}; game.amulet_slot=1; monster(0,{22,15}); cast_spell(BLOOD_BOLT,1,0);
    require(!session.ended && game.hp==player_max_hp() && !game.inventory[1].type && game.amulet_slot==NONE && game.monsters[0].hp<100 && game.turns==1,"Blood Bolt resurrection lifecycle");
    arena(); game.hp=3; game.inventory[1]={AMULET_PHOENIX_HEART,uint8_t(ITEM_CURSED|1)}; game.amulet_slot=1; cast_spell(BLOOD_BOLT,1,0);
    require(session.ended && game.inventory[1].type==AMULET_PHOENIX_HEART,"cursed heart revived Blood Bolt");
}
void lightning_wands() {
    for(uint16_t seed=1;seed<=64;++seed) for(WandModifier modifier : {WAND_NORMAL,WAND_POWERFUL}) {
        arena(seed); wand(modifier); monster(0,{22,15}); monster(1,{24,16}); monster(2,{24,18});
        use_wand(0,1,0); bool power=modifier==WAND_POWERFUL;
        require(game.monsters[0].hp>=100-(power ? 14 : 10) && game.monsters[0].hp<=100-(power ? 10 : 6) &&
                game.monsters[1].hp>=100-(power ? 10 : 7) && game.monsters[1].hp<=100-(power ? 7 : 4) &&
                (power ? game.monsters[2].hp>=93 && game.monsters[2].hp<=96 : game.monsters[2].hp==100) &&
                wand_charges(game.inventory[0])==2 && game.mana==16 && game.turns==1,"lightning wand damage/third jump/charges");
    }
    for(WandModifier modifier : {WAND_SPREADING,WAND_OVERPOWERED}) {
        arena(); wand(modifier); monster(0,{20,14}); monster(1,{21,15}); monster(2,{20,16}); monster(3,{19,15});
        uint16_t rng=game.random_state;
        // First ray: N primary, E secondary, S tertiary (powerful only).
        const auto* profile=modifier==WAND_OVERPOWERED ? &LIGHTNING_POWERFUL : &LIGHTNING_WAND;
        uint8_t expected[4]={100,100,100,100};
        for(uint8_t i=0;i<profile->targets;++i) expected[i]-=profile->minimum[i]+roll(profile->spread[i]);
        if(profile->targets==2) { expected[2]-=profile->minimum[0]+roll(profile->spread[0]); expected[3]-=profile->minimum[1]+roll(profile->spread[1]); }
        else expected[3]-=profile->minimum[0]+roll(profile->spread[0]);
        game.random_state=rng; require(use_wand(0,0,0),"spreading lightning rejected");
        for(uint8_t i=0;i<4;++i) require(game.monsters[i].hp==expected[i],"spreading lightning repeat hit or unstable order");
    }
    for(uint16_t seed=1;seed<=64;++seed) {
        arena(seed); wand(WAND_UNRELIABLE); const Position positions[]={{20,12},{23,15},{20,18},{17,15}};
        for(uint8_t i=0;i<4;++i) monster(i,positions[i]);
        uint8_t direction=roll(4); game.random_state=seed; use_wand(0,0,0);
        for(uint8_t i=0;i<4;++i) require((game.monsters[i].hp<100)==(i==direction),"unreliable lightning direction/hidden chaining");
        arena(seed); wand(WAND_CURSED); monster(0,{22,15}); use_wand(0,0,0);
        require(game.hp>=188 && game.hp<=192 && game.monsters[0].hp==100,"cursed lightning backfire damaged monsters/range");
    }
    arena(); wand(WAND_CURSED); game.magic_resistance=255;
    uint16_t rng=game.random_state; uint8_t raw=8+roll(5);
    uint8_t expected=magic_damage_after_save(raw,magic_save(255,8)); game.random_state=rng;
    use_wand(0,0,0); require(game.hp==200-expected,"cursed lightning ignored player MR");
    arena(); wand(WAND_NORMAL,1); monster(0,{22,15},1); monster(1,{23,16},1,DRAGON); use_wand(0,1,0);
    require(!game.inventory[0].type && !game.monsters[0].type && !game.monsters[1].type && game.score && item_type_identified(WAND_LIGHTNING),"lightning kill attribution/dragon/depletion/identity");
    arena(); wand(); use_wand(0,1,0); require(wand_charges(game.inventory[0])==2 && item_is_identified(game.inventory[0]),"empty lightning consumption/individual identity");
    arena(); wand(WAND_CURSED); game.inventory[1]={SCROLL_REMOVE_CURSE,1}; use_inventory(1,0); require(wand_modifier(game.inventory[0])==WAND_NORMAL && wand_charges(game.inventory[0])==3,"lightning remove curse changed encoding");
    game.inventory[1]={SCROLL_ENCHANT,1}; use_inventory(1,0); require(wand_charges(game.inventory[0])>3 && wand_modifier(game.inventory[0])==WAND_NORMAL,"lightning enchant recharge");
    arena(); wand(); Game before=game; require(!use_wand(0,0,0) && !std::memcmp(&before,&game,sizeof game),"lightning invalid direction spent charge");
}
}
void check_magic_expansion() { metadata_and_learning(); fireburst(); phase(); lightning(); slumber_and_blood(); lightning_wands(); }
