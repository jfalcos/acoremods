#include "DynamicAHVendor.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"
#include <unordered_set>

namespace ModDynamicAH
{

    static std::unordered_map<uint32, uint8> g_vendorCache;
    std::unordered_map<uint32, uint8> &DynamicAHVendor::Cache() { return g_vendorCache; }

    static std::unordered_set<uint32> g_reliableVendorItems;
    static bool g_reliableVendorBuilt = false;

    void DynamicAHVendor::EnsureReliableVendorSetBuilt()
    {
        if (g_reliableVendorBuilt)
            return;
        g_reliableVendorBuilt = true;

        // "Reliable" = sold for plain gold (ExtendedCost = 0 — excludes token/currency-only
        // purchases like Frozo's Frost Lotus) from either at least one unlimited-stock vendor,
        // or from enough distinct vendors (>=3) that limited per-vendor stock is effectively
        // always available somewhere. A single low-stock vendor does NOT count as reliable —
        // that's the "one-off" case the AH should still cover (e.g. Copper Ore has exactly one
        // such vendor on this server; it stays AH-worthy).
        if (QueryResult qr = WorldDatabase.Query(
                "SELECT item, COUNT(DISTINCT entry) AS vendorCount, SUM(maxcount = 0) AS unlimitedCount "
                "FROM npc_vendor WHERE ExtendedCost = 0 GROUP BY item "
                "HAVING unlimitedCount > 0 OR vendorCount >= 3"))
        {
            do
                g_reliableVendorItems.insert(qr->Fetch()[0].Get<uint32>());
            while (qr->NextRow());
        }

        LOG_INFO("mod.dynamicah", "vendor: {} items are reliably gold-vendor-purchasable and excluded from AH postings",
                 g_reliableVendorItems.size());
    }

    bool DynamicAHVendor::IsReliableVendorItem(uint32 itemId)
    {
        EnsureReliableVendorSetBuilt();
        return g_reliableVendorItems.count(itemId) != 0;
    }

    uint8 DynamicAHVendor::VendorStockType(uint32 itemId, ItemTemplate const *tmpl, bool considerBuyPrice)
    {
        auto it = g_vendorCache.find(itemId);
        if (it != g_vendorCache.end())
            return it->second;

        uint8 res = 0;

        if (QueryResult qr = WorldDatabase.Query("SELECT maxcount FROM npc_vendor WHERE item = {} LIMIT 1", itemId))
        {
            int32 maxc = qr->Fetch()[0].Get<int32>(); // 0 = unlimited, >0 = limited
            res = (maxc == 0) ? 2 : 1;
        }
        else if (considerBuyPrice && tmpl && tmpl->BuyPrice > 0)
            res = 2;

        g_vendorCache.emplace(itemId, res);
        return res;
    }

    void DynamicAHVendor::ApplyVendorFloor(ItemTemplate const *tmpl, uint32 &startBid, uint32 &buyout, uint32 minPriceCopper, double vendorMinMarkup)
    {
        if (!tmpl)
            return;

        if (tmpl->BuyPrice == 0)
        {
            startBid = std::max(startBid, minPriceCopper);
            buyout = std::max(buyout, std::max(startBid, minPriceCopper));
            return;
        }

        uint32 floorV = std::max<uint32>(minPriceCopper, uint32(double(tmpl->BuyPrice) * (1.0 + vendorMinMarkup)));
        startBid = std::max(startBid, floorV);
        buyout = std::max(buyout, std::max(startBid, floorV));
    }

} // namespace ModDynamicAH
