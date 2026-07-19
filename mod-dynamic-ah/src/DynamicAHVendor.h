#pragma once

#include "DynamicAHTypes.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"

namespace ModDynamicAH
{

    class DynamicAHVendor
    {
    public:
        // 0 = not vendor, 1 = limited stock, 2 = unlimited
        static uint8 VendorStockType(uint32 itemId, ItemTemplate const *tmpl, bool considerBuyPrice);
        static void ApplyVendorFloor(ItemTemplate const *tmpl, uint32 &startBid, uint32 &buyout, uint32 minPriceCopper, double vendorMinMarkup);

        // True if a player can reliably buy this item from an NPC vendor with plain gold — i.e.
        // it's not a rare one-off (a single vendor with a handful in limited stock) and not
        // bought with a special currency/token (ExtendedCost != 0). Such items shouldn't compete
        // with the AH; a player can just walk up and buy them. Built once (lazily) from
        // `npc_vendor`, cached for the life of the process.
        static bool IsReliableVendorItem(uint32 itemId);

    private:
        static std::unordered_map<uint32, uint8> &Cache();
        static void EnsureReliableVendorSetBuilt();
    };

} // namespace ModDynamicAH
