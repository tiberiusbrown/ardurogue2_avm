#include "spells.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "features.hpp"
#include "persistence.hpp"
#include "status.hpp"
#include "world.hpp"
#include <cstring>
#include <string>
#include <initializer_list>
using namespace rogue;
extern void require(bool, const char*);
extern void reset_feature_status();
extern const std::string& feature_status_text();
namespace {
void arena() {
    game={}; session={NONE,DEATH,false};
    game.player={20,15}; game.up={2,2}; game.down={40,25};
    game.run_seed=game.random_state=71; game.hp=game.max_hp=200;
    game.hunger=255; game.speed=1; game.level=1; game.mana=16;
    game.known_spells=(1u<<SPELL_COUNT)-1;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[0]={GLOWING_ORB,0}; game.offhand_slot=0;
    reset_feature_status();
}
void wall(Position p) { game.walls[(p.y*MAP_W+p.x)>>3]|=uint8_t(1u<<(p.x&7)); }
void monster(uint8_t index,Position pos,uint8_t type=GOBLIN,uint8_t hp=100) {
    game.monsters[index]={pos,type,hp,15,{0,0},0};
}
void book(uint8_t spell) {
    game.known_spells=spell_bit(MAGIC_DART); game.inventory[2]={SPELLBOOK,spellbook_for_spell(spell)};
}
void learning() {
    for(uint8_t spell : {uint8_t(FROSTBIND),uint8_t(BLINK),uint8_t(EXCHANGE)}) {
        arena(); book(spell); game.known_spells |= spellbook_mask(spellbook_for_spell(spell)) & ~spell_bit(spell); uint16_t initial=game.known_spells; game.mana=0;
        require(use_inventory(2,spell) && studying() && game.turns==0,"study initiation consumed a turn");
        uint8_t duration=spell_study_turns(spell);
        for(uint8_t turn=1;turn<=duration;++turn) {
            require(advance_study(),"study turn rejected");
            require(game.turns==turn,"study skipped ordinary turns");
            require(game.mana==turn/16,"study omitted mana regeneration");
            if(turn<duration) require(studying() && session.study_progress==turn && game.inventory[2].type==SPELLBOOK && game.known_spells==initial,"premature learning/book consumption");
        }
        require(!studying() && game.inventory[2].type==NO_ITEM && game.known_spells==(initial|spell_bit(spell)),"completion corrupted spell bitmask");
        require(game.hunger==255-duration/3,"study applied extra hunger cost");
        game.inventory[2]={SPELLBOOK,spellbook_for_spell(spell)}; uint16_t rng=game.random_state;
        require(!begin_study(2,spell) && game.turns==duration && game.random_state==rng,"duplicate study spent resources");
    }
    arena(); book(EXCHANGE); game.hunger=study_food_required(EXCHANGE)-1;
    require(!begin_study(2,EXCHANGE) && !game.turns,"study accepted inadequate food reserve");
    arena(); book(FROSTBIND); require(begin_study(2,FROSTBIND),"study fixture");
    for(uint8_t i=0;i<10;++i) advance_study();
    Game before=game;
    require(!use_inventory(2) && !cast_spell(MAGIC_DART,1,0) && !drop_inventory(2),"concurrent study action accepted");
    move_player(1,0); action(); wait_search();
    require(!std::memcmp(&before,&game,sizeof game),"ordinary action during study");
    cancel_study(); require(game.inventory[2].type==SPELLBOOK && game.known_spells==1 && !session.study_progress,"cancel taught spell or lost book");
    require(begin_study(2,FROSTBIND) && session.study_progress==0,"partial study resumed");
    game.hunger=STUDY_HUNGRY-1; advance_study();
    require(!studying() && game.inventory[2].type==SPELLBOOK,"hungry study continued");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); game.see_invisible=1; advance_study();
    require(!studying() && feature_status_text().find("potion sight fades")!=std::string::npos && feature_status_text().find("lose your concentration")!=std::string::npos,"expiration interruption suppressed gameplay message");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND);
    for(uint8_t i=0;i<39;++i) advance_study();
    game.haste=1; advance_study();
    require(game.turns==40 && !studying() && game.inventory[2].type==SPELLBOOK && game.known_spells==1,"final-turn interruption lost precedence");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); status_number(7); advance_study();
    require(!studying(),"number-only gameplay output did not interrupt");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); monster(0,{21,15}); game.monsters[0].stun=0; game.speed=monster_speed(GOBLIN);
    advance_study(); require(!studying(),"ordinary attack/miss did not interrupt");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); game.player.x++; advance_study();
    require(!studying() && game.known_spells==1,"displaced reading continued");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); game.hp=1; game.hunger=0; game.turns=3; advance_study();
    require(session.ended && !studying() && game.inventory[2].type==SPELLBOOK,"death retained study");
    arena(); book(FROSTBIND); begin_study(2,FROSTBIND); game.magic=SAVE_MAGIC; game.version=SAVE_VERSION; game.valid=1; game.mana=3;
    require(restore_startup_save(true) && !studying() && game.inventory[2].type==SPELLBOOK && game.mana==3 && game.known_spells==1,"save lifecycle lost magic or book");
    game.version=SAVE_VERSION-1; require(!restore_startup_save(true) && !game.known_spells,"incompatible save accepted");
}
void displacement() {
    arena(); monster(0,{21,15}); require(knockback_monster(0,1,0,3)==3 && game.monsters[0].pos==Position{24,15},"knockback was only one tile");
    arena(); monster(0,{21,15}); wall({23,15}); game.monsters[0].stun=0;
    require(knockback_monster(0,1,0,3)==1 && game.monsters[0].pos==Position{22,15} && game.monsters[0].stun==2,"wall knockback/impact");
    arena(); monster(0,{21,15}); monster(1,{23,15});
    require(knockback_monster(0,1,0,3)==1 && game.monsters[1].pos==Position{23,15},"occupied landing overlapped");
    arena(); monster(0,{21,15}); game.doors[0]={{23,15}}; game.door_count=1;
    require(knockback_monster(0,1,0,3)==1,"closed door knockback");
    open_door(0); require(knockback_monster(0,1,0,2)==2,"open door blocked force");
    arena(); monster(0,{21,16}); wall({22,16});
    require(!knockback_monster(0,1,1,3),"diagonal corner cut");
    arena(); monster(0,{0,0}); require(!knockback_monster(0,-1,0,3),"map boundary crossed");
    arena(); monster(0,{21,15}); game.features[0]={{22,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    game.features[1]={{23,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    require(knockback_monster(0,1,0,3)==3 && game.monsters[0].hp<=96 && game.monsters[0].hp>=92,"intermediate traps skipped or repeated");
    arena(); monster(0,{21,15},GOBLIN,1); game.features[0]={{22,15},FEATURE_SPIKE_TRAP};
    require(knockback_monster(0,1,0,3)==1 && !game.monsters[0].type && !game.score && !game.xp,"lethal knockback trap invalid reference/environment rewards");
    arena(); monster(0,{21,15},GOBLIN,1); game.monsters[0].state=MON_SUMMONED; game.features[0]={{22,15},FEATURE_SPIKE_TRAP};
    knockback_monster(0,1,0,3); require(!game.score && !game.xp,"summoned trap kill granted rewards");
    arena(); monster(0,{21,15}); game.features[0]={{22,15},FEATURE_TELEPORT_TRAP};
    require(knockback_monster(0,1,0,3)==1 && game.monsters[0].pos!=Position{22,15},"teleport trap continued original knockback ray");
    arena(); monster(0,{21,15},PHANTOM); monster(1,{20,14},MIMIC); monster(2,{19,16},BAT);
    require(cast_spell(SHOCKWAVE),"Shockwave rejected");
    require(game.monsters[0].pos.x>=23 && game.monsters[1].pos.y<=12 && game.monsters[2].pos.x<=17 && game.monsters[2].pos.y>=18,"Shockwave lost captured adjacent targets or multi-tile push");
}
void casting() {
    for(uint8_t spell : {uint8_t(MAGIC_DART),uint8_t(FROSTBIND),uint8_t(REPULSION),
                        uint8_t(CONFOUND),uint8_t(PIERCING_BOLT),uint8_t(BANISH)}) {
        for(uint8_t obstacle=0;obstacle<3;++obstacle) {
            arena(); monster(0,{22,15});
            if(obstacle==0) wall({21,15});
            if(obstacle==1) { game.doors[0]={{21,15}}; game.door_count=1; }
            if(obstacle==2) game.player={0,0};
            require(cast_spell(spell,obstacle==2 ? -1 : 1,0) && game.turns==1 &&
                    game.mana==16-spell_cost(spell) && game.monsters[0].hp==100 &&
                    game.monsters[0].pos==Position{22,15} && !monster_effect(game.monsters[0],MON_CONFUSED) &&
                    !monster_effect(game.monsters[0],MON_SLOWED),"spell ray crossed a wall, door or map edge");
        }
    }
    for(uint8_t spell=0;spell<SPELL_COUNT;++spell) {
        arena(); uint16_t rng=game.random_state;
        game.mana=spell_cost(spell)-1; monster(0,{22,15});
        require(!cast_spell(spell,1,0,0) && game.mana==spell_cost(spell)-1 && !game.turns && game.random_state==rng,"insufficient mana spent RNG/turn");
        game.mana=16; game.paralyzed=1;
        require(!cast_spell(spell,1,0,0) && !game.turns,"paralyzed cast");
        game.paralyzed=0; game.known_spells=0; require(!cast_spell(spell,1,0,0),"unlearned cast");
    }
    arena(); monster(0,{22,15},GOBLIN,10); cast_spell(MAGIC_DART,1,0);
    require(game.turns==1 && game.mana==14 && game.monsters[0].hp>=6 && game.monsters[0].hp<=8,"dart range/damage/cost");
    arena(); monster(0,{26,15}); cast_spell(MAGIC_DART,1,0); require(game.monsters[0].hp==100,"dart exceeded five tiles");
    arena(); monster(0,{22,15}); wall({21,15}); cast_spell(MAGIC_DART,1,0); require(game.monsters[0].hp==100,"dart passed wall");
    arena(); monster(0,{22,15}); game.doors[0]={{21,15}}; game.door_count=1; cast_spell(MAGIC_DART,1,0); require(game.monsters[0].hp==100,"dart passed closed door");
    arena(); monster(0,{22,15},GOBLIN,1); cast_spell(MAGIC_DART,1,0); require(!game.monsters[0].type && game.score>0 && game.xp>0,"spell kill reward");
    arena(); monster(0,{21,15}); monster(1,{23,15}); monster(2,{25,15}); cast_spell(PIERCING_BOLT,1,0);
    require(game.monsters[0].hp>=95 && game.monsters[0].hp<=97 && game.monsters[1].hp>=95 && game.monsters[1].hp<=97 && game.monsters[2].hp==100,"piercing hit count/damage");
    for(uint8_t spell : {uint8_t(FROSTBIND),uint8_t(CONFOUND)}) {
        arena(); monster(0,{22,15}); cast_spell(spell,1,0);
        uint8_t duration=monster_effect(game.monsters[0],spell==FROSTBIND ? MON_SLOWED : MON_CONFUSED);
        require(duration>=3 && duration<=6,"monster condition duration");
    }
    arena(); monster(0,{21,15}); cast_spell(REPULSION,1,0); require(game.monsters[0].pos.x>=22 && game.monsters[0].pos.x<=23,"Repulsion range");
    arena(); monster(0,{22,15}); cast_spell(BANISH,1,0); require(game.monsters[0].pos!=Position{22,15} && !blocked(game.monsters[0].pos.x,game.monsters[0].pos.y) && game.monsters[0].pos!=game.player,"Banish placement");
    arena(); game.features[0]={{24,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN}; game.features[1]={{24,16},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN};
    game.features[2]={{21,15},FEATURE_FOUNTAIN}; cast_spell(SENSE_TRAPS);
    require(!feature_is_hidden(game.features[0].id) && explored({24,15}) && feature_is_hidden(game.features[1].id) && !explored({21,15}) && game.features[0].id==FEATURE_SPIKE_TRAP,"Sense Traps revealed unrelated objects or disarmed trap");
    arena(); std::memset(game.walls,255,sizeof game.walls); carve(20,15); carve(22,15); game.features[0]={{22,15},FEATURE_SPIKE_TRAP};
    cast_spell(BLINK); require(game.player==Position{22,15} && game.hp<200 && !session.forced_entry,"Blink failed through wall/trap entry");
    arena(); game.hp=1; std::memset(game.walls,255,sizeof game.walls); carve(20,15); carve(22,15);
    game.features[0]={{22,15},FEATURE_SPIKE_TRAP}; cast_spell(BLINK);
    require(session.ended && !game.hp && game.mana==12 && !session.forced_entry,"Blink fatal trap resolution");
    arena(); std::memset(game.walls,255,sizeof game.walls); carve(20,15); uint16_t rng=game.random_state;
    cast_spell(BLINK); require(game.player==Position{20,15} && game.mana==12 && game.turns==1 && game.random_state==rng,"failed Blink unbounded/extra RNG");
    arena(); monster(0,{22,15},PHANTOM); require(!exchange_target(0),"Exchange saw invisible phantom");
    game.see_invisible=10; require(exchange_target(0),"Exchange ignored potion sight");
    game.blindness=1; require(!exchange_target(0),"Exchange ignored blindness"); game.blindness=0;
    wall({21,15}); require(!exchange_target(0),"Exchange ignored LOS");
    arena(); monster(0,{24,15}); require(!exchange_target(0),"Exchange exceeded radius");
    arena(); monster(0,{22,15}); require(cast_spell(EXCHANGE,0,0,0) && game.player==Position{22,15} && game.monsters[0].pos==Position{20,15},"Exchange swap");
    arena(); monster(0,{22,15},GOBLIN,1); game.hp=1; game.features[0]={{20,15},FEATURE_SPIKE_TRAP}; game.features[1]={{22,15},FEATURE_SPIKE_TRAP};
    cast_spell(EXCHANGE,0,0,0); require(!game.monsters[0].type && session.ended && !game.hp && !game.turns,"Exchange lethal destination resolution");
    arena(); monster(0,{22,15}); game.features[0]={{20,15},FEATURE_TELEPORT_TRAP}; game.features[1]={{22,15},FEATURE_TELEPORT_TRAP};
    cast_spell(EXCHANGE,0,0,0);
    require(game.monsters[0].pos!=Position{20,15} && game.player!=Position{22,15} &&
            game.monsters[0].pos!=game.player && game.turns==1,"Exchange double teleport traps/displacement");
    arena(); game.blindness=game.confused=game.slowed=12; cast_spell(CLEANSE);
    require(!game.blindness && game.confused && game.slowed,"Cleanse cleared more than one condition");
    game.mana=16; cast_spell(CLEANSE); require(!game.confused && game.slowed,"Cleanse priority");
    game.mana=16; cast_spell(CLEANSE); require(!game.slowed,"Cleanse omitted slowing");
    game.mana=16; cast_spell(CLEANSE); require(game.mana==16-spell_cost(CLEANSE) && game.turns==4,"empty Cleanse resource rule");
}
void books_and_generation() {
    arena(); game.known_spells=1;
    require(valid_spellbooks() && !spellbook_mask(255) && spellbook_for_spell(MAGIC_DART)==NONE,"book coverage/invalid IDs");
    const char* names[]={"Book of Control","Book of Displacement","Book of Survival","Book of Destruction"};
    for(uint8_t b=0;b<BOOK_COUNT;++b) require(!std::strcmp(spellbook_name(b),names[b]),"public book names");
    for(uint8_t s=1;s<SPELL_COUNT;++s) {
        unsigned count=0; for(uint8_t b=0;b<BOOK_COUNT;++b) count+=bool(spellbook_mask(b)&spell_bit(s));
        require(count==1,"learnable spell has multiple/no books");
    }
    unsigned counts[4][BOOK_COUNT]={}, attempts[4]={};
    const uint8_t floors[]={0,3,7,11};
    const unsigned weights[4][BOOK_COUNT]={{50,35,12,3},{35,35,22,8},{20,30,30,20},{10,20,30,40}};
    for(unsigned seed=1;seed<=65535;++seed) for(uint8_t band=0;band<4;++band) {
        arena(); game.known_spells=1; game.run_seed=uint16_t(seed); game.floor=floors[band];
        game.ground[0]={{1,1},make_equipment(LONG_SWORD,0)};
        uint16_t rng=game.random_state; generate_spellbooks();
        require(game.random_state==rng && game.ground[0].pos==Position{1,1},"book generation changed RNG/location");
        if(game.ground[0].item.type==SPELLBOOK) { ++attempts[band]; ++counts[band][game.ground[0].item.info]; }
    }
    for(uint8_t band=0;band<4;++band) {
        require(attempts[band]>3500 && attempts[band]<4300,"first slot book replacement chance");
        for(uint8_t b=0;b<BOOK_COUNT;++b) {
            double expected=attempts[band]*weights[band][b]/100.0;
            double difference=counts[band][b]-expected;
            require(difference*difference<36*expected,"book depth frequencies outside six sigma");
        }
    }
    for(unsigned seed=1;seed<=2048;++seed) {
        arena(); game.known_spells=1; game.run_seed=uint16_t(seed);
        game.ground[0]={{1,1},{FOOD,1}}; game.ground[1]={{2,1},{ARROWS,3}};
        game.ground[2]={{3,1},{TORCH,50}}; game.ground[3]={{4,1},make_equipment(STORMBRINGER,0)};
        for(uint8_t slot=4;slot<GROUND_ITEMS;++slot) game.ground[slot]={{uint8_t(slot+1),1},make_equipment(LONG_SWORD,0)};
        Game before=game; generate_spellbooks(); uint8_t seen=0;
        require(!std::memcmp(game.ground,before.ground,4*sizeof(GroundItem)),"book displaced essential supply/artifact");
        for(const auto& g:game.ground) if(g.item.type==SPELLBOOK) {
            require(g.item.info<BOOK_COUNT && !(seen & (1u<<g.item.info)),"duplicate book on floor"); seen|=uint8_t(1u<<g.item.info);
        }
        for(uint8_t b=0;b<BOOK_COUNT;++b) {
            game=before; game.inventory[1]={SPELLBOOK,b}; generate_spellbooks();
            for(const auto& g:game.ground) require(g.item.type!=SPELLBOOK || g.item.info!=b,"carried duplicate generated");
            game=before; game.known_spells|=spellbook_mask(b); generate_spellbooks();
            for(const auto& g:game.ground) require(g.item.type!=SPELLBOOK || g.item.info!=b,"completed book generated");
        }
        game=before; game.has_amulet=1; Game ascent=game; generate_spellbooks(); require(!std::memcmp(&game,&ascent,sizeof game),"ascent generated books");
        game=before; game.floor=4; Game phantom=game; generate_spellbooks(); require(!std::memcmp(&game,&phantom,sizeof game),"phantom floor generated books");
    }
}
}
extern void check_magic_expansion();
extern void check_book_mana();
void check_spells() {
    check_book_mana();
    check_magic_expansion();
    start_new(123); require(game.mana==6 && game.known_spells==1 && player_max_mana()==6,"starting magic");
    arena();
    for(uint8_t level=1;level<=50;++level) { game.level=level; require(player_max_mana()==(6+level/3>16 ? 16 : 6+level/3),"max mana level cap"); }
    arena(); game.mana=0; uint16_t rng=game.random_state;
    for(unsigned n=1;n<=256;++n) { end_turn(); require(game.mana==(n/16>6 ? 6 : n/16),"mana clock cadence/wrap"); }
    require(game.random_state==rng,"mana regeneration RNG");
    game.mana=0; game.turns=255; end_turn(); require(game.mana==1,"mana wrap tick");
    game.hunger=0; game.turns=15; game.mana=0; end_turn(); require(!game.mana,"starving mana regenerated");
    learning(); displacement(); casting(); books_and_generation();
}
