#include "../bench/bench.hpp"
#include "game.hpp"
#include "game_internal.hpp"
#include "inventory_view.hpp"
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
extern "C" void bench_inventory_open_full();
extern "C" void bench_inventory_open_wands();
extern "C" void bench_inventory_open_singletons();

void check_benchmark_scenarios()
{
    for(uint8_t index = 0; index < bench_count(); ++index) {
        bench_case = index;
        bench_setup();
        require(game.valid && (game.turns == 0 || game.turns == 255) && !session.ended &&
                !wall_at(game.player.x, game.player.y), "benchmark setup is not playable");
        require((game.weapon_slot == NONE || is_weapon(game.inventory[game.weapon_slot].type)) &&
                (game.armor_slot == NONE || is_artifact(game.inventory[game.armor_slot].type)) &&
                (game.amulet_slot == NONE || is_artifact(game.inventory[game.amulet_slot].type)) &&
                (game.ring_slots[0] == NONE || is_artifact(game.inventory[game.ring_slots[0]].type)) &&
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

    bench_inventory_open_full();
    uint8_t groups[INVENTORY_GROUPS] = {};
    for(const Item& item : game.inventory) {
        bool has_appearance = is_potion(item.type) || is_scroll(item.type) || is_ring(item.type) ||
                              is_amulet(item.type) || is_wand(item.type);
        require(item.type != NO_ITEM && item_is_identified(item) &&
                (!has_appearance || item_type_identified(item.type)),
                "full inventory benchmark must fill every slot with known items");
        ++groups[inventory_group(item.type)];
    }
    for(uint8_t count : groups) require(count != 0, "full inventory benchmark lost a group");
    InventoryView full(game, nullptr);
    require(full.count() == INVENTORY + INVENTORY_GROUPS && full.first_slot() == 6 &&
            full.entry_at(full.count() - 1) == 8,
            "full inventory benchmark must span all groups with interleaved slots");

    bench_inventory_open_wands();
    bool wand_types[WAND_COUNT] = {};
    for(const Item& item : game.inventory) {
        require(is_wand(item.type) && item_is_identified(item) && item_type_identified(item.type) &&
                wand_charges(item) >= 10 && wand_charges(item) <= 15 &&
                (wand_modifier(item) == WAND_OVERPOWERED || wand_modifier(item) == WAND_UNRELIABLE),
                "wand browsing benchmark lost its long identified names or two-digit charges");
        wand_types[item.type - WAND_FORCE] = true;
    }
    for(bool present : wand_types) require(present, "wand browsing benchmark lost a wand type");
    InventoryView wands(game, nullptr);
    require(wands.count() == INVENTORY + 1 && wands.entry_at(0) == INVENTORY + WANDS,
            "wand browsing benchmark must be one full group");

    bench_inventory_open_singletons();
    memset(groups, 0, sizeof groups);
    for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
        const Item& item = game.inventory[slot];
        require((slot < INVENTORY - INVENTORY_GROUPS) == (item.type == NO_ITEM),
                "singleton inventory must occupy only late pack slots");
        if(item.type != NO_ITEM) ++groups[inventory_group(item.type)];
    }
    for(uint8_t count : groups) require(count == 1, "singleton inventory must contain every group once");
    InventoryView singletons(game, nullptr);
    require(singletons.count() == 2 * INVENTORY_GROUPS && singletons.first_slot() == 15 &&
            singletons.entry_at(singletons.count() - 1) == 7,
            "singleton inventory benchmark lost its interleaved groups");

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
