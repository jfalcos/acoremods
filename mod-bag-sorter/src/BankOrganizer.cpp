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

using namespace BagSorter;

namespace
{
    constexpr uint32 HEARTHSTONE_ITEM_ENTRY = 6948;

    inline uint16 PackPos(uint8 bag, uint8 slot)
    {
        return (uint16(bag) << 8) | slot;
    }

    // Number of bank bag slots the player has actually purchased/unlocked.
    // BANK_SLOT_BAG_END - BANK_SLOT_BAG_START is the maximum possible; slots
    // past GetBankBagSlotCount() are locked and unusable.
    uint8 UsableBankBagCount(Player* player)
    {
        return std::min<uint8>(player->GetBankBagSlotCount(), BANK_SLOT_BAG_END - BANK_SLOT_BAG_START);
    }

    struct BankPool
    {
        std::vector<uint16> slots;
        ItemTemplate const* bagTemplate = nullptr; // null for the general pool
    };

    // Builds the bank's pools from its CURRENT contents: pools[0] is the
    // general pool (the 28 main bank slots + every non-specialized purchased
    // bank bag); one pool per specialized (BagFamily != 0) purchased bank bag
    // follows, in bank-bag-slot order.
    void BuildBankPools(Player* player, std::vector<BankPool>& pools)
    {
        BankPool general;
        for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
            general.slots.push_back(PackPos(INVENTORY_SLOT_BAG_0, slot));

        uint8 const bagCount = UsableBankBagCount(player);
        for (uint8 i = 0; i < bagCount; ++i)
        {
            uint8 const bagSlot = BANK_SLOT_BAG_START + i;
            Bag* pBag = player->GetBagByPos(bagSlot);
            if (!pBag)
                continue;

            ItemTemplate const* bagTemplate = pBag->GetTemplate();
            bool const isSpecialized = bagTemplate && bagTemplate->BagFamily != 0;

            if (isSpecialized)
            {
                BankPool special;
                special.bagTemplate = bagTemplate;
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    special.slots.push_back(PackPos(bagSlot, uint8(j)));

                pools.push_back(std::move(special));
            }
            else
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    general.slots.push_back(PackPos(bagSlot, uint8(j)));
            }
        }

        pools.insert(pools.begin(), std::move(general));
    }

    // Moves an unequipped ("spare") bag - one sitting loose in the backpack or
    // inside an equipped bag, not one of the 4 equipped bag containers
    // themselves - into a purchased bank bag slot whenever it holds more space
    // than what's currently there, greedily pairing the largest spare bag
    // against the smallest bank bag (an empty slot counts as capacity 0) until
    // no more capacity is gained. Equipped bags are never touched. Bag
    // contents travel with the bag - Player::SwapItem moves both containers
    // whole (migrating a full bank bag's contents into an empty spare bag
    // first if needed), so nothing inside either bag is touched or lost.
    uint32 MoveSpareBagsIntoBank(Player* player)
    {
        struct Candidate
        {
            uint16 pos;
            uint32 capacity;
        };

        std::vector<Candidate> spareBags;
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            uint16 const pos = PackPos(INVENTORY_SLOT_BAG_0, slot);
            if (Item* item = player->GetItemByPos(pos))
                if (Bag* pBag = item->ToBag())
                    spareBags.push_back({pos, pBag->GetBagSize()});
        }

        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* equipped = player->GetBagByPos(bag);
            if (!equipped)
                continue;

            for (uint32 j = 0; j < equipped->GetBagSize(); ++j)
            {
                uint16 const pos = PackPos(bag, uint8(j));
                if (Item* item = player->GetItemByPos(pos))
                    if (Bag* pBag = item->ToBag())
                        spareBags.push_back({pos, pBag->GetBagSize()});
            }
        }

        std::vector<Candidate> bank;
        uint8 const bagCount = UsableBankBagCount(player);
        for (uint8 i = 0; i < bagCount; ++i)
        {
            uint8 const bagSlot = BANK_SLOT_BAG_START + i;
            Bag* pBag = player->GetBagByPos(bagSlot);
            bank.push_back({PackPos(INVENTORY_SLOT_BAG_0, bagSlot), pBag ? pBag->GetBagSize() : 0});
        }

        auto const byCapacity = [](Candidate const& a, Candidate const& b) { return a.capacity < b.capacity; };

        uint32 moved = 0;
        for (;;)
        {
            auto biggestSpare = std::max_element(spareBags.begin(), spareBags.end(), byCapacity);
            auto smallestBank = std::min_element(bank.begin(), bank.end(), byCapacity);

            if (biggestSpare == spareBags.end() || smallestBank == bank.end())
                break;
            if (biggestSpare->capacity <= smallestBank->capacity)
                break; // no spare bag beats the bank's smallest slot anymore

            player->SwapItem(biggestSpare->pos, smallestBank->pos);
            ++moved;

            spareBags.erase(biggestSpare);
            bank.erase(smallestBank);
        }

        return moved;
    }

    // Merge partial stacks of the same item across the given slots. Relies on
    // Player::SwapItem, which fills the destination stack from the source and
    // leaves any overflow in the source - exactly like a client drag-merge.
    void ConsolidateStacks(Player* player, std::vector<uint16> const& slots)
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

    std::vector<Item*> CollectItems(Player* player, std::vector<uint16> const& slots)
    {
        std::vector<Item*> items;
        for (uint16 pos : slots)
            if (Item* item = player->GetItemByPos(pos))
                items.push_back(item);

        return items;
    }

    // Move each item to its assigned slot using validated swaps. 'targets' is a
    // partial slot->item assignment. Unlike the carried-bag sorter, an item's
    // current position can be in ANY bank pool (redistribution moves items
    // across pools), so every lookup searches the whole bank rather than just
    // the target pool's own slots.
    uint32 ApplyTargets(Player* player, std::vector<uint16> const& allBankSlots,
                         std::vector<std::pair<uint16, ObjectGuid>> const& targets)
    {
        uint32 placed = 0;
        for (std::pair<uint16, ObjectGuid> const& target : targets)
        {
            uint16 const destPos = target.first;
            ObjectGuid const guid = target.second;

            uint16 curPos = 0xFFFF;
            for (uint16 pos : allBankSlots)
            {
                Item* item = player->GetItemByPos(pos);
                if (item && item->GetGUID() == guid)
                {
                    curPos = pos;
                    break;
                }
            }

            if (curPos == 0xFFFF)
                continue; // merged away earlier in this pass

            if (curPos != destPos)
                player->SwapItem(curPos, destPos);

            ++placed;
        }

        return placed;
    }
}

uint32 BagSorter::DepositToBank(Player* player)
{
    if (!player)
        return 0;

    std::vector<uint16> carried;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        carried.push_back(PackPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag* pBag = player->GetBagByPos(bag);
        if (!pBag)
            continue;

        for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            carried.push_back(PackPos(bag, uint8(j)));
    }

    uint32 deposited = 0;
    for (uint16 pos : carried)
    {
        Item* item = player->GetItemByPos(pos);
        if (!item || item->IsBag())
            continue; // never bank the bags themselves

        if (settings.PinHearthstone && item->GetEntry() == HEARTHSTONE_ITEM_ENTRY)
            continue; // keep the Hearthstone on hand

        uint8 const bag = uint8(pos >> 8);
        uint8 const slot = uint8(pos & 0xFF);

        ItemPosCountVec dest;
        InventoryResult msg = player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
            continue; // no room, or this item can't be banked - leave it, never force it

        player->RemoveItem(bag, slot, true);
        player->ItemRemovedQuestCheck(item->GetEntry(), item->GetCount());
        player->BankItem(dest, item, true);
        player->UpdateTitansGrip();

        ++deposited;
    }

    return deposited;
}

uint32 BagSorter::SortBank(Player* player, SortMode mode)
{
    if (!player)
        return 0;

    uint32 swapped = 0;
    if (settings.BankSwapBags)
        swapped = MoveSpareBagsIntoBank(player);

    std::vector<BankPool> pools;
    BuildBankPools(player, pools);

    std::vector<uint16> allBankSlots;
    for (BankPool const& pool : pools)
        allBankSlots.insert(allBankSlots.end(), pool.slots.begin(), pool.slots.end());

    if (settings.MergeStacks)
        ConsolidateStacks(player, allBankSlots);

    // Specialized pools greedily claim their matching items first (sorted, so
    // the "best" candidates win when a bag can't hold every match); whatever
    // is left over falls through to the general pool.
    std::vector<Item*> remaining = CollectItems(player, allBankSlots);

    std::vector<std::vector<Item*>> bucket(pools.size());
    for (std::size_t p = 1; p < pools.size(); ++p)
    {
        ItemTemplate const* bagTemplate = pools[p].bagTemplate;

        std::vector<Item*> matches;
        std::vector<Item*> rest;
        for (Item* item : remaining)
        {
            if (ItemCanGoIntoBag(item->GetTemplate(), bagTemplate))
                matches.push_back(item);
            else
                rest.push_back(item);
        }

        std::sort(matches.begin(), matches.end(), [mode](Item* a, Item* b)
        {
            return CompareItems(a, b, mode);
        });

        std::size_t const take = std::min(matches.size(), pools[p].slots.size());
        bucket[p].assign(matches.begin(), matches.begin() + take);
        rest.insert(rest.end(), matches.begin() + take, matches.end());
        remaining = std::move(rest);
    }

    std::sort(remaining.begin(), remaining.end(), [mode](Item* a, Item* b)
    {
        return CompareItems(a, b, mode);
    });
    bucket[0] = std::move(remaining);

    std::vector<std::pair<uint16, ObjectGuid>> targets;
    for (std::size_t p = 0; p < pools.size(); ++p)
    {
        std::vector<uint16> const& slots = pools[p].slots;
        std::vector<Item*> const& items = bucket[p];
        for (std::size_t i = 0; i < items.size() && i < slots.size(); ++i)
            targets.emplace_back(slots[i], items[i]->GetGUID());
    }

    uint32 const placed = ApplyTargets(player, allBankSlots, targets);
    return placed + swapped;
}
