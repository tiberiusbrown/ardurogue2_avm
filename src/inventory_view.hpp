#pragma once

#include "game.hpp"

namespace rogue {

enum InventoryGroup : uint8_t {
    WEAPONS, ARMORS, RINGS, AMULETS, POTIONS, SCROLLS, FOODS, QUEST_ITEMS,
    INVENTORY_GROUPS
};

constexpr uint8_t INVENTORY_VISIBLE_ROWS = 7;
using ItemTypeFilter = bool (*)(uint8_t type);

inline InventoryGroup inventory_group(uint8_t type)
{
    if(type == SWORD) return WEAPONS;
    if(type == ARMOR) return ARMORS;
    if(is_ring(type)) return RINGS;
    if(is_amulet(type)) return AMULETS;
    if(is_potion(type)) return POTIONS;
    if(is_scroll(type)) return SCROLLS;
    if(type == FOOD) return FOODS;
    return QUEST_ITEMS;
}

// Item rows contain inventory slot numbers; header rows start at INVENTORY.
struct InventoryView {
    uint8_t rows[INVENTORY + INVENTORY_GROUPS];
    uint8_t count = 0;

    explicit InventoryView(const Game& source, ItemTypeFilter filter)
    {
        for(uint8_t group = 0; group < INVENTORY_GROUPS; ++group) {
            bool has_header = false;
            for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
                uint8_t type = source.inventory[slot].type;
                if(type == NO_ITEM || inventory_group(type) != group ||
                   (filter && !filter(type)))
                    continue;
                if(!has_header) {
                    rows[count++] = static_cast<uint8_t>(INVENTORY + group);
                    has_header = true;
                }
                rows[count++] = slot;
            }
        }
    }

    uint8_t first_slot() const
    {
        for(uint8_t row = 0; row < count; ++row)
            if(rows[row] < INVENTORY) return rows[row];
        return NONE;
    }

    uint8_t position(uint8_t slot) const
    {
        for(uint8_t row = 0; row < count; ++row)
            if(rows[row] == slot) return row;
        return NONE;
    }

    uint8_t move(uint8_t slot, int8_t step) const
    {
        uint8_t selected = position(slot);
        if(selected == NONE) return first_slot();
        for(int16_t row = static_cast<int16_t>(selected) + step;
            row >= 0 && row < count; row += step)
            if(rows[row] < INVENTORY)
                return rows[row];
        return slot;
    }

    void keep_visible(uint8_t slot, uint8_t& top) const
    {
        uint8_t selected = position(slot);
        uint8_t maximum = count > INVENTORY_VISIBLE_ROWS
            ? static_cast<uint8_t>(count - INVENTORY_VISIBLE_ROWS) : 0;
        if(selected == NONE) top = 0;
        else if(selected < top) top = selected;
        else if(selected >= top + INVENTORY_VISIBLE_ROWS)
            top = static_cast<uint8_t>(selected - INVENTORY_VISIBLE_ROWS + 1);
        if(top > maximum) top = maximum;
    }
};

} // namespace rogue
