#include "spells.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "persistence.hpp"
#include "inventory_view.hpp"
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
    game.run_seed=game.random_state=71; game.hp=game.max_hp=100;
    game.hunger=255; game.speed=1; game.level=30; game.known_spells=1;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[19]={GLOWING_ORB,0}; game.offhand_slot=19;
    reset_feature_status();
}
void books() {
    for(uint8_t spell=1;spell<SPELL_COUNT;++spell) {
        arena(); uint8_t book=spellbook_for_spell(spell);
        game.inventory[0]={SPELLBOOK,book}; session.repeat_slot=0;
        require(begin_study(0,spell),"book refused a valid member");
        for(uint8_t n=0;n<spell_study_turns(spell);++n) advance_study();
        require(!studying() && game.inventory[0].type==SPELLBOOK && game.inventory[0].info==book &&
                game.known_spells==(1|spell_bit(spell)),"partial book lost/learned extra spells");
        require(drop_inventory(0),"partially learned book could not drop");
        uint8_t ground=item_at(game.player);
        require(ground!=NONE && take_item(ground)==PICKUP_TAKEN && game.inventory[0].info==book &&
                (game.known_spells&spell_bit(spell)),"partial book pickup lost identity/knowledge");
        Game snapshot=game;
        for(uint8_t invalid=0;invalid<SPELL_COUNT;++invalid) if(!(unknown_book_spells(book,game.known_spells)&spell_bit(invalid)))
            require(!begin_study(0,invalid) && !std::memcmp(&snapshot,&game,sizeof game),"invalid/known spell spent resources");
        game.known_spells=uint16_t(0xffffu & ~spell_bit(spell)); game.hunger=255;
        require(begin_study(0,spell),"final member refused");
        for(uint8_t n=0;n<spell_study_turns(spell);++n) advance_study();
        require(!game.inventory[0].type && game.known_spells==0xffff && !studying() &&
                session.study_spell==NONE && session.repeat_slot==NONE,"final member stale references");
    }
    arena(); game.inventory[0]={SPELLBOOK,255}; Game invalid=game;
    require(!begin_study(0,FROSTBIND) && !begin_study(INVENTORY,FROSTBIND) &&
            !begin_study(0,255) && !std::memcmp(&invalid,&game,sizeof game),"invalid book/slot accepted");
    arena(); for(auto& item:game.inventory) item={GLOWING_ORB,0}; game.inventory[0]={SPELLBOOK,BOOK_CONTROL};
    game.ground[0]={game.player,{SPELLBOOK,BOOK_SURVIVAL}};
    require(take_item(0)==PICKUP_NEEDS_SWAP && game.ground[0].item.info==BOOK_SURVIVAL,"full inventory discarded book");
    require(swap_ground_item(0,0) && game.inventory[0].info==BOOK_SURVIVAL &&
            game.ground[0].item.info==BOOK_CONTROL,"book swap changed ID");
    game.known_spells|=spell_bit(CLEANSE); game.magic=SAVE_MAGIC; game.version=SAVE_VERSION; game.valid=1;
    Game saved=game; game={}; std::memcpy(&game,&saved,sizeof game);
    require(restore_startup_save(true) && game.inventory[0].info==BOOK_SURVIVAL &&
            (game.known_spells&spell_bit(CLEANSE)),"save lost public book/partial knowledge");
    SpellbookView view{0xfffe,spell_bit(CONFOUND)|spell_bit(FROSTBIND)};
    uint8_t selected=view.move(NONE,1),top=0;
    require(selected==REPULSION && view.position(selected)==1,"book navigation did not skip known members");
    while(view.move(selected,1)!=selected) selected=view.move(selected,1);
    InventoryView::keep_row_visible(view.position(selected),view.count(),top);
    require(selected==BLOOD_BOLT && top==8 && view.entry_at(top)==EXCHANGE,"seven-row book scrolling");
    require(view.move(selected,-1)==SLUMBER,"book upward navigation");
    SpellbookView completed{spellbook_mask(BOOK_CONTROL),0xffff};
    require(completed.move(NONE,1)==NONE,"completed book entered empty selection");
}
void mana_potions() {
    for(uint8_t maximum=6;maximum<=16;++maximum) {
        bool seen[17]={};
        for(unsigned seed=1;seed<=65535;++seed) {
            game.random_state=uint16_t(seed);
            uint8_t amount=mana_restoration_amount(maximum);
            require(amount>=(maximum+1)/2 && amount<=maximum,"mana dose outside inclusive range");
            seen[amount]=true;
        }
        for(uint8_t n=(maximum+1)/2;n<=maximum;++n) require(seen[n],"mana potion unreachable endpoint");
    }
    for(uint8_t level : {uint8_t(1),uint8_t(30),uint8_t(255)}) for(unsigned seed=1;seed<=256;++seed) {
        arena(); game.level=level; game.random_state=uint16_t(seed);
        game.inventory[0]={POTION_MANA_RESTORATION,2}; game.mana=0;
        uint8_t maximum=player_max_mana();
        require(use_inventory(0) && game.turns==1 && game.mana>=(maximum+1)/2 && game.mana<=maximum &&
                game.hp==100 && potion_identified(POTION_MANA_RESTORATION) && item_value(game.inventory[0])==1,
                "mana potion scaling/action/identification/consumption");
        game.mana=maximum-1; use_inventory(0);
        require(game.mana==maximum && !game.inventory[0].type,"mana potion clamp/overflow");
    }
    arena(); game.inventory[0]={POTION_MANA_RESTORATION,1};
    game.monsters[0]={{22,15},GOBLIN,100,15,{0,0},0}; Monster before=game.monsters[0];
    uint16_t before_random=game.random_state;
    apply_monster_potion(POTION_MANA_RESTORATION,0);
    require(!std::memcmp(&before,&game.monsters[0],sizeof before) && game.random_state==before_random,
            "mana potion changed monster aggression/state or RNG");
    require(throw_potion(0,1,0) && game.monsters[0].hp==before.hp &&
            !game.monsters[0].effects[0] && !game.monsters[0].effects[1] && !game.mana,
            "thrown mana potion affected monster/player");
    arena(); game.inventory[0]={POTION_MANA_RESTORATION,2}; game.ground[0]={game.player,{POTION_MANA_RESTORATION,3}};
    require(take_item(0)==PICKUP_TAKEN && item_value(game.inventory[0])==5,"mana potion stacking");
}
void regeneration() {
    for(uint8_t mode=0;mode<3;++mode) {
        arena(); uint8_t interval=mode==0 ? 16 : mode==1 ? 8 : 32;
        if(mode) { game.inventory[0]={AMULET_MEDITATION,uint8_t(1|(mode==2 ? ITEM_CURSED : 0))}; game.amulet_slot=0; }
        uint16_t rng=game.random_state;
        require(mana_regeneration_interval()==interval,"Meditation interval");
        for(unsigned n=1;n<=768;++n) {
            game.turns=uint8_t(n); game.mana=0; regenerate_mana();
            require(game.mana==(n%interval==0) && game.random_state==rng,"MP wrap/cadence/RNG");
        }
        game.mana=16; regenerate_mana(); require(game.mana==16,"full MP regenerated");
        game.mana=0; game.hunger=0; regenerate_mana(); require(!game.mana,"starving MP regenerated");
        game.hunger=255; game.hp=0; regenerate_mana(); require(!game.mana,"dead MP regenerated");
        game.hp=100; session.ended=true; regenerate_mana(); require(!game.mana,"ended MP regenerated");
    }
    arena(); game.turns=8; game.inventory[0]={AMULET_MEDITATION,1}; game.amulet_slot=0;
    regenerate_mana(); require(game.mana==1,"equipment cadence did not apply immediately");
    game.amulet_slot=NONE; game.mana=0; regenerate_mana(); require(!game.mana,"removed Meditation kept fast cadence");
    game.turns=16; regenerate_mana(); require(game.mana==1,"ordinary interval did not resume");
    arena(); game.inventory[0]={AMULET_MEDITATION,uint8_t(ITEM_CURSED|1)}; use_inventory(0);
    uint8_t turns=game.turns; require(!use_inventory(0) && game.amulet_slot==0 && game.turns==turns,"cursed Meditation removable");
    arena(); game.inventory[0]={SPELLBOOK,BOOK_CONTROL}; game.inventory[1]={AMULET_MEDITATION,1}; game.amulet_slot=1;
    begin_study(0,FROSTBIND); for(uint8_t n=0;n<40;++n) advance_study();
    require(game.mana==5 && (game.known_spells&spell_bit(FROSTBIND)),"silent MP regeneration interrupted study");
    arena(); game.hp=50; game.inventory[0]={SPELLBOOK,BOOK_CONTROL}; game.inventory[1]={AMULET_REGENERATION,1}; game.amulet_slot=1;
    uint16_t rng=game.random_state; unsigned ticks=0;
    for(unsigned n=0;n<40;++n) ticks+=next_random(rng)%20==0;
    require(ticks>0 && begin_study(0,FROSTBIND),"HP regeneration fixture"); reset_feature_status();
    for(uint8_t n=0;n<39;++n) { advance_study(); require(studying(),"HP regeneration interrupted study"); }
    advance_study(); require(game.hp==50+ticks && game.random_state==rng &&
            feature_status_text().find("restores a little health")==std::string::npos,
            "HP amulet probability/effect/message changed");
    arena(); game.turns=255; game.floor=3; make_floor();
    require(game.turns==255,"stairs reset MP clock");
    game.magic=SAVE_MAGIC; game.version=SAVE_VERSION; game.valid=1; restore_startup_save(true);
    require(game.turns==255,"resume reset MP clock");
}
}
void check_book_mana() { books(); mana_potions(); regeneration(); }
