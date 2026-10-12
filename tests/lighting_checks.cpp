#include "game.hpp"
#include "features.hpp"
#include "game_internal.hpp"
#include "persistence.hpp"
#include "world.hpp"
#include "world_gen.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern void reset_feature_status();
extern const std::string& feature_status_text();
using namespace rogue;
namespace {
void check(bool ok, const char* message) {
    if(!ok) { std::fprintf(stderr, "Lighting: %s\n", message); std::exit(1); }
}
void arena() {
    game = {}; game.offhand_slot = NONE; session = {NONE, DEATH, false};
    game.player = {20, 15}; game.up = game.down = {NONE, NONE};
    game.hp = game.max_hp = 200; game.hunger = 255;
    game.speed = 4; game.strength = 5; game.dexterity = 4; game.random_state = 1;
    game.weapon_slot = game.armor_slot = game.amulet_slot = game.offhand_slot = NONE;
    game.ring_slots[0] = game.ring_slots[1] = NONE;
    reset_feature_status();
}
}
void check_lighting_offhand() {
    static_assert(sizeof(Item) == 2 && sizeof(Monster) == 8, "item/monster RAM grew");
    static_assert(offsetof(Game, offhand_slot) == offsetof(Game, features) + sizeof(Game::features) &&
                  offsetof(Game, blindness) == offsetof(Game, offhand_slot) + 1,
                  "saved append changed");
    arena(); check(player_light_radius() == 1, "innate radius");
    for(uint8_t type = CANDLE; type <= GLOWING_ORB; ++type) {
        arena(); game.inventory[0] = {type, light_initial_fuel(type)};
        game.offhand_slot = 0;
        check(player_light_radius() == 1 + light_bonus(type), "light type bonus");
        check(item_is_identified(game.inventory[0]) && !item_is_cursed(game.inventory[0]), "raw fuel flags");
        check(!is_stackable(type) && !is_artifact(type), "ordinary singleton light");
        identify_item(0); check(game.inventory[0].info == light_initial_fuel(type), "identify corrupted fuel");
        for(unsigned fuel = 0; fuel <= 255; ++fuel) {
            set_item_value(game.inventory[0], uint8_t(fuel));
            check(item_value(game.inventory[0]) == fuel, "eight-bit fuel round trip");
        }
        if(is_fueled_light(type)) {
            game.inventory[0].info = 0; check(player_light_radius() == 1, "empty source lit");
        }
    }
    for(uint8_t type = CANDLE; type <= TOWER_SHIELD; ++type)
        for(unsigned curses = 0; curses < 8; ++curses) {
            arena(); game.inventory[0] = {type, light_initial_fuel(type)}; game.offhand_slot = 0;
            game.inventory[1] = {RING_ILLUMINATION, uint8_t(63 | ((curses & 1) ? ITEM_CURSED : 0))};
            game.inventory[2] = {RING_ILLUMINATION, uint8_t(63 | ((curses & 2) ? ITEM_CURSED : 0))};
            game.inventory[3] = {AMULET_BRILLIANCE, uint8_t(63 | ((curses & 4) ? ITEM_CURSED : 0))};
            game.ring_slots[0] = 1; game.ring_slots[1] = 2; game.amulet_slot = 3;
            int expected = 1 + light_bonus(type) + ((curses & 1) ? -1 : 2) +
                ((curses & 2) ? -1 : 2) + ((curses & 4) ? -1 : 3);
            expected = expected < 0 ? 0 : expected > 6 ? 6 : expected;
            check(player_light_radius() == expected, "signed fixed jewelry/clamp");
            game.blindness = 1; check(player_light_radius() == 0, "blind override");
            end_turn(); check(player_light_radius() == expected, "sight recovery");
        }
    arena(); std::memset(game.explored, 0xff, sizeof game.explored);
    game.monsters[0] = {{24, 15}, ORC, 100, 0, {0, 0}, 0};
    game.ground[0] = {{24, 15}, {FOOD, 1}};
    check(!can_see(game.ground[0].pos) && (game.explored[(15*MAP_W+24)>>3] & (1u << ((15*MAP_W+24)&7))), "memory became sight");
    game.blindness = 4;
    check(can_see(game.player) && !player_can_see_monster(0), "blind center/monster detection");
    for(uint8_t type = CANDLE; type <= LANTERN; ++type) {
        double total = 0;
        for(unsigned seed = 1; seed <= 4096; ++seed) {
            arena(); game.random_state = uint16_t(seed);
            game.inventory[0] = {type, light_initial_fuel(type)}; game.offhand_slot = 0;
            unsigned turns = 0;
            while(game.offhand_slot != NONE) { burn_offhand_fuel(); ++turns; }
            total += turns;
        }
        double expected = light_initial_fuel(type)*fuel_denominator(type), mean = total/4096;
        check(mean > expected*.97 && mean < expected*1.03, "seeded expected lifetime");
        std::printf("fuel type=%u expected=%.0f mean=%.3f trials=4096\n", type, expected, mean);
        for(unsigned seed = 1; seed <= 1024; ++seed) {
            arena(); game.random_state = uint16_t(seed); game.blindness = 10; game.paralyzed = 3;
            game.inventory[0] = {type, 200}; game.offhand_slot = 0;
            uint16_t rng = uint16_t(seed); bool burns = next_random(rng)%fuel_denominator(type) == 0;
            end_turn(); check(game.inventory[0].info == 200 - burns, "one roll per blinded/paralyzed turn");
            check(game.random_state == rng, "extra fuel rolls");
        }
    }
    arena(); game.inventory[0] = {TORCH, 175};
    end_turn(); check(game.inventory[0].info == 175, "unequipped fuel burned");
    game.offhand_slot = 0; uint16_t rng = game.random_state;
    check(!use_inventory(NONE) && !throw_potion(0, 0, 0) && game.random_state == rng && game.inventory[0].info == 175,
          "cancelled action burned fuel");
    game.inventory[0].info = 1; session.repeat_slot = 0;
    for(unsigned i = 0; i < 100 && game.offhand_slot != NONE; ++i) burn_offhand_fuel();
    check(game.offhand_slot == NONE && session.repeat_slot == NONE && !game.inventory[0].type && player_light_radius() == 1,
          "extinguishing cleanup");
    check(feature_status_text().find("goes out.") != std::string::npos, "extinction message");
    arena(); game.inventory[0] = {GLOWING_ORB, 0}; game.offhand_slot = 0; rng = game.random_state;
    for(unsigned i = 0; i < 1000; ++i) burn_offhand_fuel();
    check(game.random_state == rng && game.offhand_slot == 0 && player_light_radius() == 3, "orb fuel");
    arena(); game.inventory[0] = {TORCH, 175}; game.inventory[1] = make_equipment(ROUND_SHIELD, 1);
    check(use_inventory(0) && game.offhand_slot == 0, "equip light");
    check(use_inventory(1) && game.offhand_slot == 1 && game.inventory[0].type == TORCH, "replace light");
    check(use_inventory(1) && game.offhand_slot == NONE, "unequip shield");
    game.inventory[1].info |= ITEM_CURSED;
    check(use_inventory(1), "equip cursed shield"); rng = game.turns;
    check(!use_inventory(1) && !use_inventory(0) && !drop_inventory(1) && game.turns == rng, "cursed removal/replacement");
    game.inventory[2] = {SCROLL_REMOVE_CURSE, 1};
    check(use_inventory(2, 1) && !item_is_cursed(game.inventory[1]), "shield uncurse");
    game.inventory[2] = {SCROLL_ENCHANT, 1};
    check(use_inventory(2, 1) && equipment_enchant(game.inventory[1]) == 2 && shield_rating(game.inventory[1].type) == 2, "shield enchanting");
    game.inventory[3] = make_equipment(PLATE_MAIL, 2); game.armor_slot = 3;
    check(player_armor_rating() == 8 && player_armor_enchant() == 4, "combined shield armor");
    set_equipment_enchant(game.inventory[1], 5); check(player_armor_enchant() == 5, "combined enchant upper bound");
    set_equipment_enchant(game.inventory[1], -5); set_equipment_enchant(game.inventory[3], -5);
    check(player_armor_enchant() == -5, "combined enchant lower bound");
    destroy_inventory_item(1); check(game.offhand_slot == NONE, "destroy shield reference");
    arena(); game.inventory[0] = {LANTERN, 150}; game.offhand_slot = 0;
    check(drop_inventory(0) && game.offhand_slot == NONE && game.ground[0].item.info == 150, "drop fuel/reference");
    check(take_item(0) == PICKUP_TAKEN && game.inventory[0].info == 150, "pickup fuel");
    game.offhand_slot = 0;
    for(uint8_t s = 1; s < INVENTORY; ++s) game.inventory[s] = {FOOD, 1};
    game.ground[0] = {game.player, {TORCH, 175}};
    check(take_item(0) == PICKUP_NEEDS_SWAP && swap_ground_item(0, 0) && game.offhand_slot == NONE &&
          game.inventory[0].info == 175 && game.ground[0].item.info == 150, "full pack swap fuel");
    check(use_inventory(0), "equip swapped torch");
    game.magic = SAVE_MAGIC; game.version = SAVE_VERSION; game.valid = 1; game.blindness = 5;
    Game saved = game; game = {}; game.offhand_slot = NONE; std::memcpy(&game, &saved, sizeof game);
    check(restore_startup_save(true) && std::memcmp(&game, &saved, sizeof game) == 0, "saved fuel/slot/blindness");
    game.version = SAVE_VERSION - 1; check(!restore_startup_save(true), "old save reinterpreted");
    arena(); game.inventory[0] = {POTION_BLINDNESS, 2};
    check(use_inventory(0) && game.blindness >= 7 && game.blindness <= 14 && potion_identified(POTION_BLINDNESS), "drink duration/knowledge");
    game.blindness = 100; check(use_inventory(0) && game.blindness < 15, "blind duration refresh");
    game.blindness = 1; reset_feature_status(); end_turn();
    check(game.blindness == 0 && feature_status_text().find("You can see again.") != std::string::npos, "player recovery message");
    arena(); game.monsters[0] = {{21, 15}, ORC, 100, 0, {0, 0}, MON_AGGRO};
    apply_monster_potion(POTION_BLINDNESS, 0);
    check((game.monsters[0].state & MON_BLINDED) && monster_effect(game.monsters[0], MON_CONFUSED) >= 8 &&
          feature_status_text().find("confused") == std::string::npos, "monster blind classification");
    apply_monster_potion(POTION_CONFUSION, 0); check(!(game.monsters[0].state & MON_BLINDED), "confusion replacement");
    apply_monster_potion(POTION_BLINDNESS, 0); reset_feature_status();
    blind_monster(game.monsters[0], 1); end_turn();
    check(!(game.monsters[0].state & MON_BLINDED) && !monster_effect(game.monsters[0], MON_CONFUSED) &&
          feature_status_text().find("can see again.") != std::string::npos && feature_status_text().find("confused") == std::string::npos,
          "monster blind expiration message");
    blind_monster(game.monsters[0], 8); defeat_monster(0); check(!(game.monsters[0].state & MON_BLINDED), "dead blind flag");
    arena(); game.monsters[0] = {{21, 15}, ORC, 100, 15, {0, 0}, MON_AGGRO};
    game.inventory[0] = {POTION_BLINDNESS, 1};
    check(throw_potion(0, 1, 0) && !game.inventory[0].type &&
          (game.monsters[0].state & MON_BLINDED), "thrown blindness hit");
    check(teleport_monster(0) && !(game.monsters[0].state & MON_BLINDED) &&
          monster_effect(game.monsters[0], MON_CONFUSED) == 8, "teleport confusion cause");
    game.monsters[0].pos = {21, 15}; blind_monster(game.monsters[0], 12);
    game.inventory[0] = {SCROLL_MASS_CONFUSE, 1};
    check(use_inventory(0) && !(game.monsters[0].state & MON_BLINDED), "scroll confusion cause");
    game.monsters[0].pos = {21, 15}; game.monsters[0].type = ORC;
    blind_monster(game.monsters[0], 12);
    game.inventory[0] = {WAND_POLYMORPH, uint8_t(ITEM_IDENTIFIED | 3)};
    check(use_wand(0, 1, 0) && !(game.monsters[0].state & MON_BLINDED) &&
          !monster_effect(game.monsters[0], MON_CONFUSED), "polymorph blind cleanup");
    for(uint8_t type = BUCKLER; type <= TOWER_SHIELD; ++type) {
        arena(); game.inventory[0] = make_equipment(type, 2); game.offhand_slot = 0;
        check(player_armor_rating() == shield_rating(type), "shield intrinsic defense");
        game.features[0] = {game.player, FEATURE_ENCHANTED_FORGE};
        check(resolve_feature(0, {true, 0}) && equipment_enchant(game.inventory[0]) >= 1 &&
              equipment_enchant(game.inventory[0]) <= 3 && player_armor_rating() == shield_rating(type),
              "forge shield intrinsic/enchant");
    }
    for(uint8_t depth : {uint8_t(0), uint8_t(5), uint8_t(12)}) {
        unsigned light[4] = {}, shields[3] = {}, blind = 0, rings = 0, amulets = 0;
        for(unsigned seed = 1; seed <= 4096; ++seed) {
            arena(); game.floor = depth;
            generation::populate_ordinary_items(uint16_t(seed), uint16_t(seed ^ 0xd421));
            for(const auto& ground : game.ground) {
                uint8_t type = ground.item.type;
                if(is_light(type)) { ++light[type-CANDLE]; check(ground.item.info == light_initial_fuel(type), "generated fuel"); }
                if(is_shield(type)) ++shields[type-BUCKLER];
                blind += type == POTION_BLINDNESS; rings += type == RING_ILLUMINATION; amulets += type == AMULET_BRILLIANCE;
            }
        }
        const unsigned weights[3][4] = {{30,60,10,0},{15,45,35,5},{5,25,60,10}};
        unsigned band = depth == 0 ? 0 : depth == 5 ? 1 : 2;
        unsigned total = light[0]+light[1]+light[2]+light[3], shield_total = shields[0]+shields[1]+shields[2];
        check(total > 6500 * GROUND_ITEMS / 16 && total < 8500 * GROUND_ITEMS / 16 && shield_total > 3200 * GROUND_ITEMS / 16 && shield_total < 4300 * GROUND_ITEMS / 16, "ordinary category weights");
        for(unsigned i=0;i<4;++i) {
            double share = 100.0*light[i]/total;
            check(weights[band][i] ? share > weights[band][i]-2 && share < weights[band][i]+2 : light[i]==0,
                  "depth light weights/orb rarity");
        }
        for(unsigned i=0;i<3;++i) {
            const unsigned expected[] = {45,35,20};
            double share = 100.0*shields[i]/shield_total;
            check(share > expected[i]-3 && share < expected[i]+3, "shield relative weights");
        }
        check(blind && rings && amulets, "new potion/jewelry missing");
        std::printf("generation depth=%u lights=%u/%u/%u/%u shields=%u/%u/%u blind=%u rings=%u amulets=%u\n",
            depth, light[0],light[1],light[2],light[3],shields[0],shields[1],shields[2],blind,rings,amulets);
    }
    for(unsigned seed = 1; seed <= 256; ++seed) {
        start_new(uint16_t(seed));
        check(game.offhand_slot == 0 && game.inventory[0].type == TORCH && game.inventory[0].info == 175 && player_light_radius() == 3 && !game.turns,
              "starting torch");
        for(uint8_t floor = 1; floor <= 3; ++floor) {
            game.floor = floor; make_floor(); bool light = false;
            for(const auto& ground : game.ground) light |= is_light(ground.item.type);
            check(light, "guaranteed early replacement");
        }
    }
    arena();
    for(uint8_t type = POTION_FIRST; type <= WAND_LAST; ++type)
        if(is_potion(type) || is_ring(type) || is_amulet(type) || is_scroll(type) || is_wand(type)) {
            identify_type(type); check(item_type_identified(type), "identification bit indexing");
        }
    check((game.identified_items[7] & 0xc0) == 0, "knowledge overrun");
    check(!is_artifact(RING_ILLUMINATION) && !is_artifact(AMULET_BRILLIANCE), "new jewelry became artifact");
}
