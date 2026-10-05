#include "../bench/bench.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <cstring>

using namespace rogue;
void require(bool, const char*);
extern "C" void bench_attack_hit();
extern "C" void bench_attack_miss();
extern "C" void bench_attack_kill();
extern "C" void bench_wait_dense();
extern "C" void bench_equip_cursed_amulet();
extern "C" void bench_scroll_mapping();
extern "C" void bench_wand_digging();

void check_benchmark_scenarios()
{
    for(uint8_t index = 0; index < bench_count(); ++index) {
        bench_case = index;
        bench_setup();
        require(game.valid && !game.turns && !session.ended &&
                !wall_at(game.player.x, game.player.y), "benchmark setup is not playable");
        require(game.weapon_slot == NONE && game.armor_slot == NONE &&
                game.amulet_slot == NONE && game.ring_slots[0] == NONE &&
                game.ring_slots[1] == NONE, "benchmark equipment must start unequipped");
        Game first = game;
        bench_setup();
        require(std::memcmp(&first, &game, sizeof game) == 0, "benchmark setup is not deterministic");
    }
    bench_wait_dense();
    uint8_t enemies = 0;
    for(const Monster& monster : game.monsters) enemies += monster.type != NO_MONSTER;
    require(enemies == 12 && game.door_count == 2 && game.ground[3].item.type == FOOD,
            "dense benchmark lost enemies, doors, or items");

    bench_attack_hit();
    move_player(1, 0);
    require(game.monsters[0].hp > 0 && game.monsters[0].hp < 100 && game.turns == 1,
            "benchmark attack must hit a surviving enemy");
    bench_attack_miss();
    move_player(1, 0);
    require(game.monsters[0].hp == 100 && game.turns == 1, "benchmark attack must miss");
    bench_attack_kill();
    move_player(1, 0);
    require(game.monsters[0].type == NO_MONSTER && game.score && game.turns == 1,
            "benchmark attack must kill and score");

    bench_equip_cursed_amulet();
    require(item_is_cursed(game.inventory[0]) && !item_is_identified(game.inventory[0]) &&
            !item_type_identified(AMULET_SPEED), "curse discovery must start unknown");
    require(use_inventory(0) && game.amulet_slot == 0 && game.turns == 1 &&
            item_is_cursed(game.inventory[0]) && item_is_identified(game.inventory[0]) &&
            item_type_identified(AMULET_SPEED), "benchmark must discover and equip the cursed amulet");

    bench_scroll_mapping();
    require(use_inventory(0), "mapping benchmark did not use its scroll");
    for(uint8_t row : game.explored) require(row == 0xff, "mapping benchmark did not reveal the whole map");
    bench_wand_digging();
    require(wall_at(35, 16) && use_wand(0, 1, 0) && !wall_at(35, 16) &&
            wand_charges(game.inventory[0]) == 2, "digging benchmark did not spend a charge and carve");
    bench_case = bench_count();
    bench_setup();
    require(!game.valid, "invalid benchmark selector must not be playable");
}
