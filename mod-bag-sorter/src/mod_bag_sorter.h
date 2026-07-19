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

#ifndef MOD_BAG_SORTER_H
#define MOD_BAG_SORTER_H

#include "Define.h"

class Player;
class Item;

namespace BagSorter
{
    enum class SortMode : uint8
    {
        TypeQuality          = 0,    // class/subclass/type, then quality desc, then ilvl desc, then name
        Quality              = 1,    // quality desc, then name
        ItemLevel            = 2,    // ilvl desc, then quality desc, then name
        Name                 = 3,    // name A-Z
        TypeQualityQuestLast = 4,    // TypeQuality ordering, then quest items swept into the last bag
    };

    struct Settings
    {
        bool Enable         = true;
        bool MergeStacks    = true;
        bool Announce       = true;
        bool PinHearthstone = true;  // always place the Hearthstone in the backpack's first slot

        bool BankEnable     = true;  // "Bag & Bank Organizer" NPC gossip: "Deposit everything" + "Organize my bank"
        bool BankSwapBags   = true;  // let bank sorting move higher-capacity spare (unequipped) bags into the bank
    };

    // Runtime config, populated from worldserver config in OnAfterConfigLoad.
    extern Settings settings;

    // Shared item ordering for the chosen SortMode (class/subclass/type, then
    // quality, item level, or name - see SortMode). Used by both carried-bag
    // and bank sorting so "type & quality" groups materials (e.g. all leathers)
    // together and by quality the same way in both places.
    bool CompareItems(Item* a, Item* b, SortMode mode);

    // Sorts the player's carried bags (backpack + the 4 equipped bag containers)
    // using the chosen mode. Returns the number of items that were arranged.
    // Specialized (profession) bags are sorted internally; general bags + the
    // backpack are sorted together as one pool. Never moves items across pools,
    // so every move is a guaranteed-valid SwapItem.
    //
    // If PinHearthstone is set, the Hearthstone is forced into the backpack's
    // first slot for every mode. In TypeQualityQuestLast, quest-class items are
    // additionally packed into the tail of the general pool (the last bag).
    uint32 Sort(Player* player, SortMode mode);

    // Moves bankable items from the player's carried bags (backpack + every
    // equipped bag, general or specialized) into bank storage. Skips items
    // that don't fit or can't legally be banked - nothing is ever forced in or
    // destroyed. Bag containers and (if PinHearthstone) the Hearthstone are
    // never deposited. Returns the number of items deposited.
    uint32 DepositToBank(Player* player);

    // Sorts the player's bank storage (the 28 main bank slots + every
    // purchased bank bag) using the chosen mode. If BankSwapBags is set, first
    // moves any unequipped ("spare") bag - one sitting loose in the backpack
    // or inside an equipped bag, never one of the 4 equipped bag containers
    // themselves - with more capacity than a purchased bank bag into the bank
    // (displacing the smaller/empty bank bag into the spare bag's old spot).
    // Partial stacks of the same item are merged bank-wide. Specialized
    // bank bags (herb bag, enchanting bag, ...) then greedily claim matching
    // items from the rest of the bank before the remainder is grouped/sorted
    // normally. Every item always keeps a slot within the bank's existing
    // storage, so nothing is ever lost. Returns the number of items arranged
    // (plus any bag swaps performed).
    uint32 SortBank(Player* player, SortMode mode);
}

#endif // MOD_BAG_SORTER_H
