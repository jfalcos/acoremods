// Slice 1/8 — read-only accessors for rotation/pool/history state
// used by GM commands and other read paths.
#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Group.h"
#include "GroupReference.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "Weather.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>

namespace mod_terror_zones
{

ActiveRotation TerrorZonesMgr::GetActiveRotation() const
{
    auto rot = std::atomic_load_explicit(&_rotationSnap,
                                          std::memory_order_acquire);
    return rot ? *rot : ActiveRotation{};
}
uint64 TerrorZonesMgr::GetNextTickAt() const
{
    return _nextTickAt;
}
std::vector<PoolEntry> TerrorZonesMgr::GetPool() const
{
    // `_pool` is write-once at startup; safe to copy directly.
    return _pool;
}
std::vector<HistoryTick> TerrorZonesMgr::GetHistory(uint32 maxTicks) const
{
    std::vector<HistoryTick> out;
    if (maxTicks == 0)
        return out;

    uint32 limit = maxTicks * std::max<uint32>(_slotCount, 1);
    QueryResult r = CharacterDatabase.Query(
        "SELECT tick_at, slot_index, zone_id, flavor, tier FROM terror_zones_history "
        "ORDER BY tick_at DESC, slot_index ASC LIMIT {}", limit);
    if (!r)
        return out;

    // `_pool` / `_poolIndex` are write-once at startup; safe to read
    // directly while iterating the DB rows.
    uint64 currentTickAt = 0;
    HistoryTick* current = nullptr;
    do
    {
        Field* f = r->Fetch();
        uint64 tickAt = f[0].Get<uint64>();
        uint32 slotIndex = f[1].Get<uint32>();
        uint32 zoneId = f[2].Get<uint32>();
        uint8  flavor = f[3].Get<uint8>();
        uint8  tier   = f[4].Get<uint8>();
        if (tickAt != currentTickAt || !current)
        {
            if (out.size() >= maxTicks)
                break;
            out.push_back({tickAt, {}});
            current = &out.back();
            currentTickAt = tickAt;
        }
        ActiveSlot s;
        s.zoneId = zoneId;
        s.slotIndex = slotIndex;
        auto it = _poolIndex.find(zoneId);
        s.displayName = (it != _poolIndex.end())
                      ? _pool[it->second].displayName
                      : std::to_string(zoneId);
        s.flavor = (flavor <= FLAVOR_MAX)
                 ? static_cast<Flavor>(flavor)
                 : FLAVOR_NONE;
        s.tier = (tier <= TIER_MAX)
               ? static_cast<Tier>(tier)
               : TIER_NONE;
        current->slots.push_back(std::move(s));
    } while (r->NextRow());

    return out;
}
bool TerrorZonesMgr::IsZoneEmpowered(uint32 zoneId,
                                     std::string* outName,
                                     uint32* outRemainingSec) const
{
    auto rot = std::atomic_load_explicit(&_rotationSnap,
                                          std::memory_order_acquire);
    if (!rot)
        return false;
    for (ActiveSlot const& s : rot->slots)
    {
        if (s.zoneId == zoneId)
        {
            if (outName) *outName = s.displayName;
            if (outRemainingSec)
            {
                uint64 now = static_cast<uint64>(::time(nullptr));
                *outRemainingSec = (rot->expiresAt > now)
                    ? static_cast<uint32>(rot->expiresAt - now) : 0;
            }
            return true;
        }
    }
    return false;
}

}  // namespace mod_terror_zones
