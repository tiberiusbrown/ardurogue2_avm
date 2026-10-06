#pragma once

#include "model.hpp"

namespace rogue {

enum InventoryGroup : uint8_t {
    WEAPONS, AMMO, ARMORS, RINGS, AMULETS, WANDS, POTIONS, SCROLLS, FOODS, QUEST_ITEMS,
    INVENTORY_GROUPS
};

constexpr uint8_t INVENTORY_VISIBLE_ROWS = 7;
using ItemTypeFilter = bool (*)(uint8_t type);

inline InventoryGroup inventory_group(uint8_t type)
{
    if(is_weapon(type)) return WEAPONS;
    if(is_ammo(type)) return AMMO;
    if(is_armor(type)) return ARMORS;
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
        // Locate the group once, then scan only that group's item slots.
        uint8_t counts[INVENTORY_GROUPS] = {};
        for(const Item& item : source.inventory) {
            if(item.type == NO_ITEM || (filter && !filter(item.type))) continue;
            ++counts[inventory_group(item.type)];
        }
        for(uint8_t group = 0; group < INVENTORY_GROUPS; ++group) {
            if(!counts[group]) continue;
            if(row > counts[group]) {
                row = static_cast<uint8_t>(row - counts[group] - 1);
                continue;
            }
            if(row == 0) return static_cast<uint8_t>(INVENTORY + group);
            for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
                uint8_t type = source.inventory[slot].type;
                if(type == NO_ITEM || inventory_group(type) != group ||
                   (filter && !filter(type)))
                    continue;
                if(--row == 0) return slot;
            }
            break;
        }
        return NONE;
    }

    uint8_t count() const
    {
        uint8_t rows = 0;
        uint16_t groups = 0;
        for(const Item& item : source.inventory) {
            if(item.type == NO_ITEM || (filter && !filter(item.type))) continue;
            uint16_t bit = static_cast<uint16_t>(1u << inventory_group(item.type));
            rows = static_cast<uint8_t>(rows + 1 + !(groups & bit));
            groups |= bit;
        }
        return rows;
    }

    uint8_t first_slot() const
    {
        uint8_t first = NONE, first_group = INVENTORY_GROUPS;
        for(uint8_t slot = 0; slot < INVENTORY; ++slot) {
            uint8_t type = source.inventory[slot].type;
            if(type == NO_ITEM || (filter && !filter(type))) continue;
            uint8_t group = inventory_group(type);
            if(group < first_group) {
                first = slot;
                first_group = group;
            }
        }
        return first;
    }

    uint8_t position(uint8_t slot) const
    {
        if(slot >= INVENTORY + INVENTORY_GROUPS) return NONE;
        bool header = slot >= INVENTORY;
        uint8_t target;
        if(header) target = static_cast<uint8_t>(slot - INVENTORY);
        else {
            uint8_t type = source.inventory[slot].type;
            if(type == NO_ITEM || (filter && !filter(type))) return NONE;
            target = inventory_group(type);
        }
        uint8_t row = header ? 0 : 1;
        uint16_t groups = 0;
        bool has_group = false;
        for(uint8_t index = 0; index < INVENTORY; ++index) {
            uint8_t type = source.inventory[index].type;
            if(type == NO_ITEM || (filter && !filter(type))) continue;
            uint8_t group = inventory_group(type);
            if(group < target) {
                uint16_t bit = static_cast<uint16_t>(1u << group);
                row = static_cast<uint8_t>(row + 1 + !(groups & bit));
                groups |= bit;
            } else if(group == target) {
                has_group = true;
                if(!header && index < slot) ++row;
            }
        }
        return has_group ? row : NONE;
    }

    uint8_t move(uint8_t slot, int8_t step) const
    {
        uint8_t selected = position(slot);
        return move(slot, step, selected, count());
    }

    // A modal can retain this row while its inventory and filter stay unchanged.
    uint8_t move(uint8_t slot, int8_t step, uint8_t& selected, uint8_t total) const
    {
        if(selected == NONE) {
            slot = first_slot();
            selected = position(slot);
            return slot;
        }
        for(int16_t row = static_cast<int16_t>(selected) + step;
            row >= 0 && row < total; row += step) {
            uint8_t entry = entry_at(static_cast<uint8_t>(row));
            if(entry < INVENTORY) {
                selected = static_cast<uint8_t>(row);
                return entry;
            }
        }
        return slot;
    }

    void keep_visible(uint8_t slot, uint8_t& top) const
    {
        keep_row_visible(position(slot), count(), top);
    }

    static void keep_row_visible(uint8_t selected, uint8_t total, uint8_t& top)
    {
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
