#include "companions.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "spells.hpp"
#include "features.hpp"
#include "persistence.hpp"
#include "world.hpp"
#include <cstring>
#include <string>
using namespace rogue;
extern void require(bool, const char*);
extern void reset_feature_status();
extern const std::string& feature_status_text();
namespace {
void arena() {
    game={}; session={NONE,DEATH,false};
    game.player={20,15}; game.up={2,2}; game.down={40,25};
    game.run_seed=game.random_state=71; game.hp=game.max_hp=200;
    game.hunger=255; game.speed=1; game.level=30; game.mana=16;
    game.known_spells=ALL_SPELLS;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
    age_pet_leashes();
    reset_feature_status();
}
void creature(uint8_t slot, Position p, uint8_t type, uint8_t hp=100) {
    game.monsters[slot]={p,type,hp,15,{0,0},0};
}
void slots_and_summons() {
    static_assert(HOSTILE_MONSTERS==12 && PETS==4 && MONSTERS==16 && sizeof(Monster)==8);
    static_assert(SPELL_COUNT==20 && sizeof(Game::known_spells)==4 && SUMMON_HOUND==16 && SUMMON_GOLEM==19);
    for(uint8_t type=0;type<MONSTER_TYPES;++type) {
        require(naturally_invisible(type)==bool(monster_flags(type)&MON_NATURAL_INVIS),"invisibility fast path disagrees with table");
        require((type==TROLL || type==LORD)==bool(monster_flags(type)&MON_REGENS),"regeneration fast path disagrees with table");
    }
    for(uint8_t s=SUMMON_HOUND;s<=SUMMON_GOLEM;++s) {
        arena(); game.mana=spell_cost(s)-1; Game before=game;
        require(!cast_spell(s) && !std::memcmp(&before,&game,sizeof game),"summon insufficient MP changes state");
        game.mana=16; require(cast_spell(s) && game.turns==1 && game.mana==16-spell_cost(s),"summon costs/action");
        require(pet_count()==1 && game.monsters[12].type==HOUND+s-SUMMON_HOUND &&
                game.monsters[12].hp==monster_health(game.monsters[12].type),"summon species/stats");
        Position p=game.monsters[12].pos;
        require(p!=game.player && !blocked(p.x,p.y) && feature_at(p)==NONE && monster_at(p)==12,"summon occupancy");
        game.mana=16; before=game;
        require(!cast_spell(s) && !std::memcmp(&before,&game,sizeof game),"duplicate species spends resources");
    }
    arena();
    for(uint8_t s=SUMMON_HOUND;s<=SUMMON_GOLEM;++s) { game.mana=16; require(cast_spell(s),"four species summon"); }
    require(pet_count()==4 && free_pet_slot()==NONE,"four pet cap");
    Game before=game; require(!cast_spell(SUMMON_HOUND) && !std::memcmp(&before,&game,sizeof game),"full party changes state");
    for(uint8_t i=0;i<HOSTILE_MONSTERS;++i) creature(i,{uint8_t(30+i),15},SNAKE);
    require(!summon_adjacent_monster() && !summon_monster(12,{10,10},true),"hostile summon steals pet slot");
    damage_monster(12,255,true); require(free_pet_slot()==12 && !game.score && !game.xp,"pet death rewards/frees slot");
    game.mana=16; require(cast_spell(SUMMON_HOUND),"dead pet cannot be replaced");
    uint8_t turn=game.turns,mp=game.mana;
    require(dismiss_pet(12) && game.turns==uint8_t(turn+1) && game.mana>=mp && free_pet_slot()==12 && !game.score,"dismissal costs/rewards");
    arena(); std::memset(game.walls,255,sizeof game.walls); carve(20,15); carve(21,15);
    game.features[0]={{21,15},FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN}; before=game;
    require(!cast_spell(SUMMON_HOUND) && !std::memcmp(&before,&game,sizeof game),"summon accepts dangerous trap");
    game.features[0]={}; game.doors[0]={{21,15}}; game.door_count=1; before=game;
    require(!cast_spell(SUMMON_HOUND) && !std::memcmp(&before,&game,sizeof game),"summon accepts closed door");
}
void taming() {
    const uint8_t eligible[]={BAT,SNAKE,RATTLESNAKE,GOBLIN,ORC,TARANTULA,HOBGOBLIN,TROLL,GRIFFIN};
    for(uint8_t type:eligible) {
        arena(); creature(0,{22,15},type,1);
        game.monsters[0].stun=7; game.monsters[0].effects[0]=0x43; game.monsters[0].effects[1]=0xb2;
        Monster before=game.monsters[0];
        require(taming_chance(before,TamingMethod::Scroll)==(type==TROLL || type==GRIFFIN ? 50 : 100),"scroll resistance");
        for(unsigned seed=1;seed<100 && !pet_count();++seed) { game.random_state=uint16_t(seed); tame_monster(0,TamingMethod::Scroll); }
        require(pet_count()==1 && !game.monsters[0].type && !std::memcmp(&before,&game.monsters[12],sizeof before),"taming loses identity/HP/status");
        require(!tame_monster(12,TamingMethod::Scroll) && pet_count()==1,"retaming a pet");
    }
    for(uint8_t type:{ZOMBIE,PHANTOM,MIMIC,INCUBUS,DRAGON,ANGEL,LORD}) {
        arena(); creature(0,{22,15},type,1); Game before=game;
        require(!tame_monster(0,TamingMethod::Scroll) && !std::memcmp(&before,&game,sizeof game),"immune monster converted/rolled RNG");
    }
    arena(); creature(0,{22,15},ORC,1); game.monsters[0].state=MON_SUMMONED;
    require(!tame_monster(0,TamingMethod::Wand) && !pet_count(),"rewardless summon tamed");
    arena(); creature(0,{22,15},GOBLIN,monster_health(GOBLIN));
    require(taming_chance(game.monsters[0],TamingMethod::Potion)==50 && taming_chance(game.monsters[0],TamingMethod::Wand)==75,"base taming odds");
    game.monsters[0].hp=1;
    require(taming_chance(game.monsters[0],TamingMethod::Potion)==75 && taming_chance(game.monsters[0],TamingMethod::Wand)==100,"injury taming bonus");
    creature(0,{22,15},TROLL,monster_health(TROLL));
    require(taming_chance(game.monsters[0],TamingMethod::Potion)==5 && taming_chance(game.monsters[0],TamingMethod::Wand)==25,"strong healthy taming odds");
    game.monsters[0].hp=monster_health(TROLL)/2;
    require(taming_chance(game.monsters[0],TamingMethod::Potion)==15 && taming_chance(game.monsters[0],TamingMethod::Wand)==40,"strong half-health taming odds");
    game.monsters[0].hp=1;
    require(taming_chance(game.monsters[0],TamingMethod::Potion)==25 && taming_chance(game.monsters[0],TamingMethod::Wand)==50,"strong quarter-health taming odds");
    arena(); creature(0,{22,15},GOBLIN,1); game.inventory[0]={SCROLL_TAMING,2};
    Game before=game;
    require(!use_inventory(0,NONE) && !std::memcmp(&before,&game,sizeof game),"scroll cancellation consumes resources");
    require(use_inventory(0,0) && pet_count()==1 && game.turns==1 && item_value(game.inventory[0])==1 && item_type_identified(SCROLL_TAMING),"scroll taming pipeline");
    arena(); creature(0,{22,15},GOBLIN,1); game.inventory[0]={WAND_DOMINATION,3};
    require(use_wand(0,1,0) && pet_count()==1 && wand_charges(game.inventory[0])==2,"domination ray/charge conversion");
    arena(); creature(0,{22,15},GOBLIN,1); game.inventory[0]={POTION_FRIENDSHIP,2};
    bool success=false;
    for(unsigned seed=1;seed<100 && !success;++seed) {
        game.random_state=uint16_t(seed); game.inventory[0]={POTION_FRIENDSHIP,2};
        require(throw_potion(0,1,0) && item_value(game.inventory[0])==1,"friendship throw/stack"); success=pet_count()!=0;
    }
    require(success,"friendship never tames wounded target");
    arena(); game.inventory[0]={POTION_FRIENDSHIP,1}; require(use_inventory(0) && !pet_count(),"friendship drinking tames");
}
void movement_and_persistence() {
    arena(); creature(12,{21,15},GOBLIN,10); game.monsters[12].stun=255;
    move_player(1,0);
    require(game.player==Position{21,15} && game.monsters[12].pos==Position{20,15} && game.turns==1,"player/pet swap");
    arena(); creature(12,{21,15},HOUND,14); game.monsters[12].stun=255; game.floor=5;
    game.features[0]={{20,15},FEATURE_SPIKE_TRAP}; game.features[1]={{21,15},FEATURE_POISON_DART_TRAP};
    move_player(1,0);
    require(game.turns==1 && game.player==Position{21,15} && game.weakened==1 && game.monsters[12].hp<14 && game.hp>=197,"swap trap entries duplicated/lost");
    arena(); creature(12,{24,15},HOUND,9); game.monsters[12].stun=0; session.pet_trail={23,15};
    reset_feature_status(); require(pet_action(12)==3 && game.monsters[12].pos==Position{23,15} && feature_status_text().empty(),"following trail/message");
    arena(); creature(12,{22,15},HOUND,9); creature(13,{21,15},SPIDER,18); game.monsters[12].stun=0;
    require(pet_action(12)==3 && game.monsters[12].pos==Position{21,15} && game.monsters[13].pos==Position{22,15},"pet corridor exchange");
    arena(); creature(12,{22,15},HOUND,14); creature(0,{21,14},GOBLIN,10);
    game.monsters[12].stun=0; begin_pet_turn(12);
    require(pet_action(12)==3 && game.monsters[12].pos==Position{21,15},"pet does not approach a nearby defensive threat");
    require(pet_action(12)==0 && game.monsters[0].hp==10,"approach grants an extra attack");
    end_pet_turn(); begin_pet_turn(12);
    require(pet_action(12)==16,"pet fails to attack after its approach turn"); end_pet_turn();
    arena(); creature(12,{22,15},GOLEM,36); game.monsters[12].stun=0;
    game.features[0]={{22,15},FEATURE_SPIKE_TRAP}; game.doors[0]={{21,15}}; game.door_count=1;
    game.speed=monster_speed(GOLEM); end_turn();
    require(door_open(0) && game.monsters[12].pos==Position{22,15} && game.monsters[12].hp==36,"opening a door retriggers a pet's current trap");
    arena(); creature(12,{50,20},HOUND,9); game.monsters[12].stun=0; game.monsters[12].effects[1]=0xa0;
    std::memset(game.walls,255,sizeof game.walls);
    for(uint8_t y=13;y<18;++y) for(uint8_t x=18;x<23;++x) carve(x,y);
    carve(50,20); reset_feature_status();
    delay_pet_recall(12);
    require(game.monsters[12].state & MON_RECALL_GRACE,"displacement grace is not saved");
    session.pet_leash[0]=0; // A loaded run loses transient clocks.
    for(uint8_t t=0;t<5;++t) { age_pet_leashes(); pet_action(12); require(game.monsters[12].pos==Position{50,20},"recall undoes displacement immediately"); }
    for(uint8_t t=0;t<16;++t) { age_pet_leashes(); pet_action(12); }
    require(square_contains(game.player,game.monsters[12].pos,3) && game.monsters[12].hp==9 && game.monsters[12].effects[1]==0xa0 && feature_status_text().empty(),"stranded recall loses state/noise");
    Position recalled=game.monsters[12].pos;
    for(uint8_t t=0;t<10;++t) { age_pet_leashes(); pet_action(12); }
    require(game.monsters[12].pos==recalled,"routine recall oscillates");
    start_new(123); creature(12,{10,10},HOUND,9); game.monsters[12].effects[0]=0x84; game.monsters[12].stun=7;
    Monster before=game.monsters[12];
    game.player=game.down; require(take_stairs() && game.floor==1,"pet descent");
    require(!std::memcmp(reinterpret_cast<uint8_t*>(&before)+2,reinterpret_cast<uint8_t*>(&game.monsters[12])+2,6) && game.monsters[12].pos.x<MAP_W,"descent clears/cures pet");
    game.has_amulet=1; game.player=game.up; require(take_stairs() && !game.floor,"pet ascent");
    require(!std::memcmp(reinterpret_cast<uint8_t*>(&before)+2,reinterpret_cast<uint8_t*>(&game.monsters[12])+2,6),"ascent loses pet state");
    game.valid=1; game.magic=SAVE_MAGIC; game.version=SAVE_VERSION;
    Game saved=game; require(restore_startup_save(true) && !std::memcmp(&saved,&game,sizeof game),"save/resume pet state");
    for(uint8_t i=0;i<MONSTERS;++i) if(game.monsters[i].type) {
        require(monster_at(game.monsters[i].pos)==i && game.monsters[i].pos!=game.player && !blocked(game.monsters[i].pos.x,game.monsters[i].pos.y),"generation stale/overlapping pet");
    }
    for(uint16_t seed=1;seed<=64;++seed) {
        start_new(seed);
        for(uint8_t p=0;p<PETS;++p) creature(HOSTILE_MONSTERS+p,{uint8_t(10+p),10},HOUND+p,9);
        game.player=game.down; require(take_stairs() && pet_count()==4,"full-party descent drops companion");
        for(uint8_t p=HOSTILE_MONSTERS;p<MONSTERS;++p) {
            const auto& pet=game.monsters[p];
            require(pet.type==HOUND+p-HOSTILE_MONSTERS && pet.hp==9 && pet.stun==15,"full-party transition changes state");
            if(pet.pos.x!=NONE) require(monster_at(pet.pos)==p && pet.pos!=game.player && !blocked(pet.pos.x,pet.pos.y),"full-party transition overlaps creatures");
        }
    }
}
void combat_and_study() {
    arena(); creature(12,{21,15},WISP,10); creature(0,{23,15},PHANTOM,100); game.monsters[12].stun=0;
    require(pet_action(12)==32 && game.monsters[0].hp==100,"wisp target/invisible detection");
    creature_attack(12,0,true); require(game.monsters[0].hp>=96 && game.monsters[0].hp<=98 && game.hp==200 && game.monsters[12].hp==10,"wisp magical armor/friendly damage");
    game.walls[(15*MAP_W+22)>>3]|=uint8_t(1u<<(22&7)); require(pet_action(12)!=32,"wisp shoots through walls");
    for(bool rewardless:{false,true}) {
        arena(); creature(12,{21,15},HOUND,14); creature(0,{22,15},SNAKE,1);
        game.monsters[0].state=rewardless ? MON_SUMMONED : 0;
        for(unsigned seed=1;seed<100 && game.monsters[0].type;++seed) { game.random_state=uint16_t(seed); creature_attack(12,0); }
        require(!game.monsters[0].type && game.score==(rewardless ? 0 : 11) && game.xp==(rewardless ? 0 : 2),"pet XP/score reward");
        uint16_t score=game.score; damage_monster(0,255,true); defeat_monster(0); require(score==game.score,"duplicate pet kill reward");
    }
    arena(); creature(12,{21,15},WISP,10); creature(0,{22,15},LORD,1); creature_attack(12,0,true);
    require(!game.monsters[0].type && game.ground[yendor_ground_slot()].item.type==YENDOR_AMULET && game.score==275,"pet Lord/Yendor reward");
    arena(); creature(0,{22,15},ORC,100); creature(12,{21,15},GOLEM,36); game.monsters[0].stun=0;
    require(blocking_pet(0)==12,"hostile blocker targeting");
    for(unsigned seed=1;seed<100 && game.monsters[12].hp==36;++seed) { game.random_state=uint16_t(seed); creature_attack(0,12); }
    require(game.monsters[12].hp<36 && game.hp==200,"hostile attacks pet through player-damage API");
    for(uint8_t spell:{MAGIC_DART,FROSTBIND,CONFOUND,REPULSION,PIERCING_BOLT,BANISH,SLUMBER,BLOOD_BOLT,CHAIN_LIGHTNING,FIREBURST}) {
        arena(); creature(12,{21,15},HOUND,100); creature(0,{22,15},ORC,100); game.monsters[12].stun=255;
        require(scan_ray(game.player,1,0,5).monster==12 && cast_spell(spell,1,0),"spell passes friendly first hit");
        if(spell==MAGIC_DART || spell==BLOOD_BOLT) require(game.monsters[0].hp==100 && game.monsters[12].hp<100,"damage ray friendly interception");
        if(spell==FROSTBIND) require(monster_effect(game.monsters[12],MON_SLOWED)!=0,"pet slowing spell");
        if(spell==CONFOUND) require(monster_effect(game.monsters[12],MON_CONFUSED)!=0,"pet confusion spell");
        if(spell==PIERCING_BOLT || spell==CHAIN_LIGHTNING || spell==FIREBURST) require(game.monsters[12].hp<100 && game.monsters[0].hp<100,"multi-target effects skip friend");
        require(friendly_slot(12) && pet_count()==1,"friendly fire changes allegiance");
    }
    arena(); creature(12,{21,15},HOUND,5); game.inventory[0]={POTION_HEALING,1}; apply_monster_potion(POTION_HEALING,12);
    require(game.monsters[12].hp>5 && game.monsters[12].hp<=14,"thrown healing pet HP");
    game.turns=32; game.monsters[12].hp=13; recover_pets(); recover_pets(); require(game.monsters[12].hp==14,"pet regeneration exceeds maximum");
    arena(); creature(12,{24,15},HOUND,9); game.monsters[12].stun=0; game.inventory[0]={SPELLBOOK,BOOK_COMPANIONS}; game.known_spells=1;
    require(begin_study(0,SUMMON_HOUND),"companion book study"); reset_feature_status();
    for(uint8_t t=0;t<60;++t) require(advance_study(),"pet study turn");
    require(!studying() && (game.known_spells&spell_bit(SUMMON_HOUND)) && game.inventory[0].type==SPELLBOOK && game.monsters[12].hp>9,"routine pet interrupts study/regeneration");
    game.known_spells=ALL_SPELLS&~spell_bit(SUMMON_GOLEM); game.hunger=255;
    require(begin_study(0,SUMMON_GOLEM),"book last spell"); for(uint8_t t=0;t<60;++t) advance_study();
    require(game.known_spells==ALL_SPELLS && !game.inventory[0].type,"four-spell book completion");
}
void generation() {
    for(unsigned seed=1;seed<=1000;++seed) {
        start_new(uint16_t(seed)); unsigned books=0;
        for(const auto& g:game.ground) books+=g.item.type==SPELLBOOK && g.item.info==BOOK_COMPANIONS;
        require(books==1,"early companion book guarantee/duplicate");
        for(uint8_t i=12;i<16;++i) require(!game.monsters[i].type,"generation occupies pet slots");
        game.inventory[0]={SPELLBOOK,BOOK_COMPANIONS}; make_floor();
        for(const auto& g:game.ground) require(g.item.type!=SPELLBOOK || g.item.info!=BOOK_COMPANIONS,"carried companion book duplicated");
    }
    arena(); game.floor=4; game.ground[0]={{1,1},make_equipment(DAGGER,0)}; Game before=game;
    generate_spellbooks(); require(!std::memcmp(&before,&game,sizeof game),"phantom floor gets new books");
    game.floor=2; game.has_amulet=1; before=game; generate_spellbooks(); require(!std::memcmp(&before,&game,sizeof game),"ascent generates new books");
}
}
void check_companions() {
    slots_and_summons(); taming(); movement_and_persistence(); combat_and_study(); generation();
}
