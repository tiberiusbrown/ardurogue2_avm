#include "game.hpp"
#include "game_internal.hpp"
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
    game = {}; session = {NONE, DEATH, false};
    game.run_seed = game.random_state = 71;
    game.player = game.up = {20, 15}; game.down = {40, 15};
    game.hp = 20; game.max_hp = 100; game.hunger = 255; game.speed = 4;
    game.weapon_slot = game.armor_slot = game.offhand_slot = game.amulet_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    reset_feature_status();
}
}
void check_recovery() {
    static_assert(NATURAL_REGENERATION_INTERVAL == 128 &&
        256 % NATURAL_REGENERATION_INTERVAL == 0, "recovery must survive clock wrap");
    arena(); auto rng = game.random_state;
    for(unsigned n = 1; n <= 256; ++n) {
        end_turn();
        require(game.hp == 20 + n / NATURAL_REGENERATION_INTERVAL,
            "natural healing cadence or clock wrap");
        require(game.random_state == rng, "natural healing advanced random state");
    }
    require(feature_status_text().find("recover a little health") != std::string::npos,
        "natural healing feedback");
    arena(); game.hp = game.max_hp; game.turns = 127; end_turn();
    require(game.hp == game.max_hp && feature_status_text().empty(), "full HP banked recovery");
    game.hp -= 2; end_turn(); require(game.hp == 98, "full HP recovery tick was banked");
    arena(); game.hunger = 0; game.turns = 127; end_turn();
    require(game.hp == 19, "natural healing offset starvation");
    arena(); game.hp = 1; game.hunger = 0; game.turns = 255; end_turn();
    require(session.ended && !game.hp, "recovery resurrected starvation death");
    // A lethal enemy response on a recovery boundary must also stay lethal.
    bool lethal = false;
    for(unsigned seed = 1; seed <= 128 && !lethal; ++seed) {
        arena(); game.hp = 1; game.turns = 127; game.random_state = uint16_t(seed);
        game.monsters[0] = {{21,15},LORD,100,0,{0,0},0}; end_turn();
        if(session.ended) { lethal = true; require(!game.hp, "recovery healed after enemy death"); }
    }
    require(lethal, "lethal recovery boundary fixture did not kill");
    arena(); game.inventory[0] = {AMULET_REGENERATION,uint8_t(ITEM_CURSED|1)};
    game.amulet_slot = 0; game.turns = 127; end_turn();
    require(game.hp == 21, "one HP recovery changed existing curse rounding");
    // The amulet retains its original random 1-in-20 healing and can stack
    // with natural recovery. Compare whole random states, including misses.
    for(unsigned seed = 1; seed <= 128; ++seed) {
        arena(); game.inventory[0] = {AMULET_REGENERATION,1}; game.amulet_slot = 0;
        game.turns = 127; game.random_state = uint16_t(seed);
        auto expected_rng = game.random_state; bool proc = next_random(expected_rng) % 20 == 0;
        end_turn(); require(game.hp == 21 + proc && game.random_state == expected_rng,
            "natural recovery changed amulet probability or stacking");
    }
    arena(); game.magic = SAVE_MAGIC; game.version = SAVE_VERSION; game.valid = 1;
    game.turns = 127; Game saved = game; game = {}; game = saved;
    require(restore_startup_save(true) && game.turns == 127, "saved recovery phase");
    make_floor(); require(game.turns == 127 && game.hp == 20, "generation reset recovery");
    game.player = game.down; require(take_stairs() && game.turns == 127 && game.hp == 20,
        "stairs advanced recovery");
    std::memset(game.monsters,0,sizeof game.monsters); end_turn();
    require(game.hp == 21, "recovery phase lost through stairs/resume");
    for(uint8_t maximum : {uint8_t(1),uint8_t(3),uint8_t(18),uint8_t(37),uint8_t(100),uint8_t(255)}) {
        for(unsigned seed = 1; seed <= 512; ++seed) {
            for(bool cursed : {false,true}) {
                arena(); game.hp = 1; game.max_hp = maximum; game.weakened = 3;
                game.random_state = uint16_t(seed); auto expected_rng = game.random_state;
                unsigned amount = maximum * HEALING_MIN_QUARTERS / 4 +
                    next_random(expected_rng) % (maximum * HEALING_RANDOM_QUARTERS / 4 + 1);
                if(amount > maximum) amount = maximum;
                if(cursed) { game.inventory[1] = {AMULET_REGENERATION,uint8_t(ITEM_CURSED|1)};
                    game.amulet_slot = 1; amount = (amount + 1) / 2; }
                game.inventory[0] = {POTION_HEALING,2};
                require(use_inventory(0) && game.hp == (1 + amount > maximum ? maximum : 1 + amount) &&
                    !game.weakened && item_value(game.inventory[0]) == 1 &&
                    potion_identified(POTION_HEALING) && game.random_state == expected_rng,
                    "healing potion range, clamp, curse, weakening, consumption or RNG");
            }
        }
    }
}
