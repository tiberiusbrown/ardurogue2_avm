#pragma once

#include "model.hpp"

namespace rogue {

enum InventoryGroup : uint8_t {
    WEAPONS, ARMORS, RINGS, AMULETS, WANDS, POTIONS, SCROLLS, FOODS, QUEST_ITEMS,
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
    if(is_wand(type)) return WANDS;
    if(is_potion(type)) return POTIONS;
    if(is_scroll(type)) return SCROLLS;
    if(type == FOOD) return FOODS;
    return QUEST_ITEMS;
}

// Item rows contain inventory slot numbers; header rows start at INVENTORY.
struct InventoryView {
    const Game& source;
    ItemTypeFilter filter;

    explicit InventoryView(const Game& source, ItemTypeFilter filter)
        : source(source), filter(filter) {}

    uint8_t entry_at(uint8_t row) const
    {
        for(uint8_t group = 0; group < INVENTORY_GROUPS; ++group) {
            bool has_header = false;
            for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
                uint8_t type = source.inventory[slot].type;
                if(type == NO_ITEM || inventory_group(type) != group ||
                   (filter && !filter(type)))
                    continue;
                if(!has_header) {
                    if(row-- == 0)
                        return static_cast<uint8_t>(INVENTORY + group);
                    has_header = true;
                }
                if(row-- == 0) return slot;
            }
        }
        return NONE;
    }

    uint8_t count() const
    {
        uint8_t rows = 0;
        for(uint8_t group = 0; group < INVENTORY_GROUPS; ++group) {
            bool has_header = false;
            for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
                uint8_t type = source.inventory[slot].type;
                if(type == NO_ITEM || inventory_group(type) != group ||
                   (filter && !filter(type)))
                    continue;
                if(!has_header) {
                    ++rows;
                    has_header = true;
                }
                ++rows;
            }
        }
        return rows;
    }

    uint8_t first_slot() const
    {
        for(uint8_t row = 0, total = count(); row < total; ++row) {
            uint8_t entry = entry_at(row);
            if(entry < INVENTORY) return entry;
        }
        return NONE;
    }

    uint8_t position(uint8_t slot) const
    {
        for(uint8_t row = 0, total = count(); row < total; ++row)
            if(entry_at(row) == slot) return row;
        return NONE;
    }

    uint8_t move(uint8_t slot, int8_t step) const
    {
        uint8_t selected = position(slot);
        if(selected == NONE) return first_slot();
        for(int16_t row = static_cast<int16_t>(selected) + step;
            row >= 0 && row < count(); row += step) {
            uint8_t entry = entry_at(static_cast<uint8_t>(row));
            if(entry < INVENTORY) return entry;
        }
        return slot;
    }

    void keep_visible(uint8_t slot, uint8_t& top) const
    {
        uint8_t selected = position(slot);
        uint8_t total = count();
        uint8_t maximum = total > INVENTORY_VISIBLE_ROWS
            ? static_cast<uint8_t>(total - INVENTORY_VISIBLE_ROWS) : 0;
        if(selected == NONE) top = 0;
        else if(selected < top) top = selected;
        else if(selected >= top + INVENTORY_VISIBLE_ROWS)
            top = static_cast<uint8_t>(selected - INVENTORY_VISIBLE_ROWS + 1);
        if(top > maximum) top = maximum;
    }
};

} // namespace rogue
