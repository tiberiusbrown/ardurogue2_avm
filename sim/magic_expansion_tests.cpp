#include "simulator.hpp"
#include "spells.hpp"
#include "world.hpp"
#include "game_internal.hpp"
#include <cstring>
#include <stdexcept>
using namespace rogue;
using namespace sim;
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void arena() {
    game={}; session={NONE,DEATH,false}; game.player={20,15}; game.up={1,1}; game.down={40,25};
    game.run_seed=game.random_state=71; game.hp=game.max_hp=100; game.level=1; game.mana=16;
    game.known_spells=0xffff; game.hunger=255; game.speed=1;
    game.weapon_slot=game.armor_slot=game.amulet_slot=game.offhand_slot=NONE;
    game.ring_slots[0]=game.ring_slots[1]=NONE;
    game.inventory[18]={GLOWING_ORB,0}; game.offhand_slot=18;
    game.inventory[19]={AMULET_BRILLIANCE,1}; game.amulet_slot=19;
    game.monsters[0]={{22,15},GOBLIN,100,15,{0,0},0};
    game.monsters[1]={{23,16},ORC,100,15,{0,0},0};
}
}
void check_magic_expansion_telemetry() {
    arena(); Collector c; CollectScope sink(c); c.enter_floor();
    check(cast_spell(CHAIN_LIGHTNING,1,0),"telemetry Chain Lightning rejected");
    const auto& spell=c.data.spells[CHAIN_LIGHTNING];
    check(spell.casts==1 && spell.mana_spent==spell_cost(CHAIN_LIGHTNING) && spell.direct_damage>=3 && spell.direct_damage<=5 &&
          spell.chained_damage>=3 && spell.chained_damage<=4 && spell.damage==spell.direct_damage+spell.chained_damage,"spell chain source/progression telemetry");
    game.monsters[0].hp=game.monsters[1].hp=100; game.inventory[0]={WAND_LIGHTNING,uint8_t(ITEM_IDENTIFIED|3)};
    check(use_wand(0,1,0),"telemetry lightning wand rejected");
    const auto& wand=c.data.items[WAND_LIGHTNING];
    check(wand.charges_used==1 && wand.direct_damage>=6 && wand.chained_damage>=4 &&
          wand.damage==wand.direct_damage+wand.chained_damage && spell.casts==1,"wand/spell damage attribution conflated");
    const auto& visit=c.data.floors.back().items[WAND_LIGHTNING];
    check(visit.damage==wand.damage && visit.direct_damage==wand.direct_damage && visit.chained_damage==wand.chained_damage,"visit lightning telemetry lost damage");
    game.mana=16; game.monsters[0].hp=game.monsters[1].hp=1;
    cast_spell(CHAIN_LIGHTNING,1,0); check(c.data.spells[CHAIN_LIGHTNING].kills==2 && !wand.kills,"chain kill attribution");
    game.monsters[0]={{22,15},GOBLIN,100,0,{0,0},0}; game.mana=16;
    cast_spell(SLUMBER,1,0); check(c.data.spells[SLUMBER].disabled==1 && c.data.spells[SLUMBER].stun_turns>=3 && c.data.spells[SLUMBER].stun_turns<=4,"Slumber disable telemetry");
    for(uint8_t s : {uint8_t(FROSTBIND),uint8_t(CONFOUND)}) {
        game.mana=16; game.monsters[0].pos={22,15};
        cast_spell(s,1,0);
        check(c.data.spells[s].status_targets==1 && c.data.spells[s].status_turns>=4 &&
              c.data.floors.back().magic.spells[s].status_turns==c.data.spells[s].status_turns,"control spell effect telemetry");
    }
    game.features[0]={{24,15},uint8_t(FEATURE_SPIKE_TRAP|MAP_FEATURE_HIDDEN)}; game.mana=16;
    cast_spell(SENSE_TRAPS); check(c.data.spells[SENSE_TRAPS].traps_revealed==1,"trap reveal spell telemetry");
    game.blindness=12; game.mana=16; cast_spell(CLEANSE);
    check(c.data.spells[CLEANSE].statuses_cleared==1,"Cleanse useful effect telemetry");
    game.hp=3; game.mana=16; auto turns=c.data.turns; game.monsters[0].hp=100;
    cast_spell(BLOOD_BOLT,1,0);
    check(c.data.spells[BLOOD_BOLT].hp_paid==3 && c.data.spells[BLOOD_BOLT].self_deaths==1 &&
          !c.data.spells[BLOOD_BOLT].damage && c.data.turns==turns && c.data.death_cause=="spell:15","fatal Blood Bolt telemetry/turn attribution");
    for(const char* rule : {"balance:expanded-spells=0","balance:expanded-spells=1","balance:lightning-wand-percent=0","balance:lightning-wand-percent=12"}) parse_rule(rule);
    for(const char* rule : {"balance:expanded-spells=2","balance:lightning-wand-percent=101"}) {
        bool rejected=false; try { parse_rule(rule); } catch(const std::runtime_error&) { rejected=true; }
        check(rejected,"expansion control accepted invalid value");
    }
    // Content controls retain exact generation locations and resource streams.
    auto exp=std::make_shared<RuleExperiment>(); c.experiment=exp;
    uint64_t completed=c.data.floors.back().magic.books_completed;
    game.inventory[0]={SPELLBOOK,BOOK_CONTROL}; destroy_inventory_item(0);
    check(c.data.floors.back().magic.books_completed==completed,"destroyed book counted as completed study");
    exp->rules={parse_rule("balance:legacy-spell-rules=1")};
    check(legacy_spell_rules() && active_spell_cost(FROSTBIND)==3 && active_spell_cost(CHAIN_LIGHTNING)==6 &&
          active_study_turns(FIREBURST)==80 && active_study_food_required(EXCHANGE)==152,"legacy cost profile missing");
    arena(); std::memset(game.monsters,0,sizeof game.monsters); game.known_spells=1;
    game.inventory[0]={SPELLBOOK,uint8_t(0x80|FIREBURST)};
    check(begin_study(0,FIREBURST),"legacy study rejected");
    for(unsigned n=0;n<79;++n) advance_study();
    check(studying() && !(game.known_spells & spell_bit(FIREBURST)),"legacy study used candidate duration");
    advance_study(); check(!studying() && (game.known_spells & spell_bit(FIREBURST)) && !game.inventory[0].type,"legacy final study lifecycle");
    exp->rules={parse_rule("balance:legacy-spell-rules=0")};
    check(!legacy_spell_rules() && active_spell_cost(CHAIN_LIGHTNING)==spell_cost(CHAIN_LIGHTNING),"candidate cost profile not restored");
    unsigned ordinary_wands=0, lightning_wands=0;
    for(uint16_t seed=1;seed<=64;++seed) for(uint8_t floor=0;floor<FLOORS;++floor) {
        exp->rules={parse_rule("balance:expanded-spells=0"),parse_rule("balance:lightning-wand-percent=0")};
        start_new(seed); game.floor=floor; make_floor(); Game before=game;
        exp->rules={parse_rule("balance:expanded-spells=1"),parse_rule("balance:lightning-wand-percent=12")};
        make_floor();
        check(!std::memcmp(game.walls,before.walls,sizeof game.walls) && !std::memcmp(game.doors,before.doors,sizeof game.doors) &&
              !std::memcmp(game.features,before.features,sizeof game.features) && !std::memcmp(game.monsters,before.monsters,sizeof game.monsters) &&
              game.random_state==before.random_state,"expansion content changed terrain/artifacts/monsters/RNG");
        for(uint8_t i=0;i<GROUND_ITEMS;++i) {
            const auto& a=before.ground[i]; const auto& b=game.ground[i];
            check(a.pos==b.pos,"expansion replacement moved loot");
            if(a.item.type==SPELLBOOK) check(b.item.type==SPELLBOOK,"expansion changed book availability");
            else if(is_wand(a.item.type)) {
                check(is_wand(b.item.type) && a.item.info==b.item.info,"lightning replacement changed wand modifier/charge");
                ++ordinary_wands; lightning_wands+=b.item.type==WAND_LIGHTNING;
            }
            else check(!std::memcmp(&a.item,&b.item,sizeof(Item)),"expansion displaced ordinary resource/artifact");
        }
    }
    check(ordinary_wands>=100 && lightning_wands*100>=ordinary_wands*8 &&
          lightning_wands*100<=ordinary_wands*16,"lightning generation departed from the 12% replacement rate");
}
