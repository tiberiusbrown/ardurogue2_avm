#include "inventory_view.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace rogue;

static void require(bool condition, const char* reason)
{
    if(!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(1);
    }
}

int main()
{
    Game source = {};
    uint32_t random = 0x7123;
    const ItemTypeFilter filters[] = {nullptr, is_potion, is_wand, is_equipment};
    // Compare against independently sorted rows, including empty and sparse views.
    for(uint16_t sample = 0; sample < 256; ++sample) {
        for(Item& item : source.inventory) {
            random = random * 1664525u + 1013904223u;
            item = {static_cast<uint8_t>((random >> 16) % (WAND_POLYMORPH + 1)), 1};
            if(sample == 0) item.type = NO_ITEM;
            if(sample == 1) item.type = WAND_FORCE;
            if(sample % 3 == 0 && (random & 1)) item.type = NO_ITEM;
        }
        for(ItemTypeFilter filter : filters) {
            uint8_t slots[INVENTORY], items = 0;
            for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
                uint8_t type = source.inventory[slot].type;
                if(type != NO_ITEM && (!filter || filter(type))) slots[items++] = slot;
            }
            std::sort(slots, slots + items, [&source](uint8_t a, uint8_t b) {
                InventoryGroup ga = inventory_group(source.inventory[a].type);
                InventoryGroup gb = inventory_group(source.inventory[b].type);
                return ga < gb || (ga == gb && a < b);
            });
            uint8_t rows[INVENTORY + INVENTORY_GROUPS], total = 0, previous = NONE;
            for(uint8_t index = 0; index < items; ++index) {
                uint8_t group = inventory_group(source.inventory[slots[index]].type);
                if(group != previous) rows[total++] = static_cast<uint8_t>(INVENTORY + group);
                rows[total++] = slots[index];
                previous = group;
            }
            InventoryView actual(source, filter);
            require(actual.count() == total && actual.first_slot() == (items ? slots[0] : NONE),
                    "inventory count/first slot disagrees with sorted reference");
            for(uint8_t row = 0; row < total; ++row)
                require(actual.entry_at(row) == rows[row] && actual.position(rows[row]) == row,
                        "inventory row/position disagrees with sorted reference");
            require(actual.entry_at(total) == NONE && actual.position(NONE) == NONE,
                    "inventory lookup accepted an invalid row or slot");
            for(uint8_t slot = 0; slot < INVENTORY + INVENTORY_GROUPS; ++slot) {
                uint8_t expected_position = NONE;
                for(uint8_t row = 0; row < total; ++row)
                    if(rows[row] == slot) expected_position = row;
                require(actual.position(slot) == expected_position,
                        "inventory position accepted an empty or filtered slot/group");
            }
            for(uint8_t index = 0; index < items; ++index) {
                require(actual.move(slots[index], -1) == slots[index ? index - 1 : 0] &&
                        actual.move(slots[index], 1) == slots[index + 1 < items ? index + 1 : index],
                        "inventory movement disagrees with sorted reference");
                for(int8_t step : {-1, 1}) {
                    uint8_t destination = slots[step < 0 ? (index ? index - 1 : 0)
                        : (index + 1 < items ? index + 1 : index)];
                    uint8_t row = actual.position(slots[index]);
                    require(actual.move(slots[index], step, row, total) == destination &&
                            row == actual.position(destination),
                            "cached selection row diverged from selected slot");
                }
            }
            uint8_t row = NONE;
            require(actual.move(NONE, 1, row, total) == (items ? slots[0] : NONE) &&
                    row == (items ? 1 : NONE),
                    "cached selection did not recover from invalid/empty view");
            for(uint8_t index = 0; index < items; ++index)
                for(uint8_t old_top = 0; old_top <= total; ++old_top) {
                    int selected = std::find(rows, rows + total, slots[index]) - rows;
                    int last_top = std::max(0, int(total) - INVENTORY_VISIBLE_ROWS);
                    int first_top = std::max(0, selected - INVENTORY_VISIBLE_ROWS + 1);
                    int expected_top = std::max(first_top,
                        std::min(int(old_top), std::min(selected, last_top)));
                    uint8_t top = old_top, cached_top = old_top;
                    actual.keep_visible(slots[index], top);
                    actual.keep_row_visible(static_cast<uint8_t>(selected), total, cached_top);
                    require(top == expected_top && cached_top == expected_top,
                            "cached row visibility changed viewport boundaries");
                }
        }
    }
    std::puts("inventory view reference checks passed");
}
