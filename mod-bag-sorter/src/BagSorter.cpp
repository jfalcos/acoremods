/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mod_bag_sorter.h"
#include "Bag.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BagSorter
{
    Settings settings;
}

using namespace BagSorter;

namespace
{
    // Pack a (bag, slot) pair into the uint16 position used by Player::SwapItem.
    inline uint16 PackPos(uint8 bag, uint8 slot)
    {
        return (uint16(bag) << 8) | slot;
    }

    // A "pool" is an ordered list of slot positions that items may freely move
    // between. Pool 0 is the backpack + every general (BagFamily == 0) equipped
    // bag; each specialized bag forms its own pool so we never attempt a move a
    // bag family would reject.
    void BuildPools(Player* player, std::vector<std::vector<uint16>>& pools)
    {
        std::vector<uint16> general;

        // Backpack (INVENTORY_SLOT_BAG_0, slots 23..38).
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            general.push_back(PackPos(INVENTORY_SLOT_BAG_0, slot));

        // The 4 equipped bag containers (bag slots 19..22).
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* pBag = player->GetBagByPos(bag);
            if (!pBag)
                continue;

            ItemTemplate const* bagTemplate = pBag->GetTemplate();
            bool const isSpecialized = bagTemplate && bagTemplate->BagFamily != 0;

            if (isSpecialized)
            {
                std::vector<uint16> special;
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    special.push_back(PackPos(bag, uint8(j)));

                pools.push_back(std::move(special));
            }
            else
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    general.push_back(PackPos(bag, uint8(j)));
            }
        }

        pools.insert(pools.begin(), std::move(general));
    }

    // Merge partial stacks of the same item within a pool. Relies on
    // Player::SwapItem, which fills the destination stack from the source and
    // leaves any overflow in the source - exactly like a client drag-merge.
    void ConsolidatePool(Player* player, std::vector<uint16> const& slots)
    {
        std::unordered_map<uint32, std::vector<uint16>> byEntry;
        for (uint16 pos : slots)
            if (Item* item = player->GetItemByPos(pos))
                if (item->GetTemplate()->GetMaxStackSize() > 1)
                    byEntry[item->GetEntry()].push_back(pos);

        for (auto& kvp : byEntry)
        {
            std::vector<uint16>& positions = kvp.second;
            if (positions.size() < 2)
                continue;

            for (std::size_t d = 0; d < positions.size(); ++d)
            {
                Item* dst = player->GetItemByPos(positions[d]);
                if (!dst)
                    continue;

                uint32 const maxStack = dst->GetTemplate()->GetMaxStackSize();
                for (std::size_t s = d + 1; s < positions.size() && dst->GetCount() < maxStack; ++s)
                {
                    if (!player->GetItemByPos(positions[s]))
                        continue;

                    player->SwapItem(positions[s], positions[d]); // merges src -> dst
                    dst = player->GetItemByPos(positions[d]);
                    if (!dst)
                        break;
                }
            }
        }
    }

    bool CompareItems(Item* a, Item* b, SortMode mode)
    {
        ItemTemplate const* ta = a->GetTemplate();
        ItemTemplate const* tb = b->GetTemplate();

        switch (mode)
        {
            case SortMode::TypeQuality:
                if (ta->Class != tb->Class)                 return ta->Class < tb->Class;
                if (ta->SubClass != tb->SubClass)           return ta->SubClass < tb->SubClass;
                if (ta->InventoryType != tb->InventoryType) return ta->InventoryType < tb->InventoryType;
                if (ta->Quality != tb->Quality)             return ta->Quality > tb->Quality;
                if (ta->ItemLevel != tb->ItemLevel)         return ta->ItemLevel > tb->ItemLevel;
                break;
            case SortMode::Quality:
                if (ta->Quality != tb->Quality)             return ta->Quality > tb->Quality;
                break;
            case SortMode::ItemLevel:
                if (ta->ItemLevel != tb->ItemLevel)         return ta->ItemLevel > tb->ItemLevel;
                if (ta->Quality != tb->Quality)             return ta->Quality > tb->Quality;
                break;
            case SortMode::Name:
                break;
        }

        if (ta->Name1 != tb->Name1)
            return ta->Name1 < tb->Name1;

        return a->GetGUID() < b->GetGUID(); // stable, deterministic tiebreak
    }

    // Reorder the pool's items into sorted order using selection-by-swap.
    // Identity is tracked by ObjectGuid (re-resolved each step) so an item that
    // is merged away mid-pass is simply skipped rather than dereferenced stale.
    uint32 PlacePool(Player* player, std::vector<uint16> const& slots, SortMode mode)
    {
        std::vector<Item*> items;
        for (uint16 pos : slots)
            if (Item* item = player->GetItemByPos(pos))
                items.push_back(item);

        if (items.empty())
            return 0;

        std::sort(items.begin(), items.end(), [mode](Item* a, Item* b)
        {
            return CompareItems(a, b, mode);
        });

        std::vector<ObjectGuid> order;
        order.reserve(items.size());
        for (Item* item : items)
            order.push_back(item->GetGUID());

        uint32 placed = 0;
        for (std::size_t i = 0; i < order.size() && i < slots.size(); ++i)
        {
            uint16 desiredPos = 0xFFFF;
            for (uint16 pos : slots)
            {
                Item* item = player->GetItemByPos(pos);
                if (item && item->GetGUID() == order[i])
                {
                    desiredPos = pos;
                    break;
                }
            }

            if (desiredPos == 0xFFFF)
                continue; // merged away during this pass

            if (desiredPos != slots[i])
                player->SwapItem(desiredPos, slots[i]); // move/swap desired into target slot

            ++placed;
        }

        return placed;
    }
}

uint32 BagSorter::Sort(Player* player, SortMode mode)
{
    if (!player)
        return 0;

    std::vector<std::vector<uint16>> pools;
    BuildPools(player, pools);

    uint32 total = 0;
    for (std::vector<uint16> const& pool : pools)
    {
        if (settings.MergeStacks)
            ConsolidatePool(player, pool);

        total += PlacePool(player, pool, mode);
    }

    return total;
}
