#include "PropertyOverrideMgr.h"

#include <algorithm>
#include <unordered_set>

// Pure diff-sync computation, kept free of game-engine includes so the
// lifecycle unit tests can exercise it in isolation. The executor contract
// (unapply first, then prune expired rows, then apply) lives in
// PropertyOverrideMgr::Sync.

namespace mod_property_override
{

SyncActions ComputeSyncActions(ItemOverrideMap const& overrides,
                               AppliedMap const& applied,
                               std::vector<ObjectGuid::LowType> const& equipped,
                               uint64 now)
{
    SyncActions actions;

    std::unordered_set<ObjectGuid::LowType> expiredSet;
    for (auto const& [guid, item] : applied)
        if (item.minExpiry != 0 && now >= item.minExpiry)
        {
            expiredSet.insert(guid);
            actions.expired.push_back(guid);
        }

    std::unordered_set<ObjectGuid::LowType> desired;
    for (ObjectGuid::LowType guid : equipped)
    {
        auto it = overrides.find(guid);
        if (it == overrides.end())
            continue;
        bool hasLive = std::any_of(it->second.begin(), it->second.end(),
                                   [now](OverrideRow const& row) { return !row.IsExpired(now); });
        if (hasLive)
            desired.insert(guid);
    }

    for (auto const& [guid, item] : applied)
        if (!desired.count(guid) || expiredSet.count(guid))
            actions.unapply.push_back(guid);

    for (ObjectGuid::LowType guid : desired)
        if (!applied.count(guid) || expiredSet.count(guid))
            actions.apply.push_back(guid);

    return actions;
}

} // namespace mod_property_override
