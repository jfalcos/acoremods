// Slice 8 — event-boss bonus loot pool rolls + gold uplift.
#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace mod_terror_zones
{

bool TerrorZonesMgr::TryEventBossDrop(Player const* player, Loot& loot)
{
    if (!player || !_enabled || !_eventsEnabled
        || !_eventBossLootPoolEnabled)
        return false;

    uint64 rawGuid = loot.sourceWorldObjectGUID.GetRawValue();
    if (rawGuid == 0)
        return false;

    // World-thread-only. Event-boss spawn index is the writer-side
    // mutable copy; loot path runs on the same world thread that
    // mutates it (TickEvents / FireEventNow / EndActiveEventsInZone).
    if (!_eventBossSpawnIndex.count(rawGuid))
        return false;  // not an event boss

    // Per-bundle dedup so TryEventBossDrop fires at most once per
    // Loot* bundle, matching the Slice 4 TryUniqueDrop shape. Without
    // this guard, OnBeforeDropAddItem would re-fire the injection per
    // template item the boss rolls — we'd get N copies of the
    // guaranteed blue. Uses its own set (distinct from
    // _uniqueRolledBundles) so unique drops + event-boss drops can
    // both fire on the same bundle.
    uint64 bundleKey = reinterpret_cast<uint64>(&loot);
    uint64 now = static_cast<uint64>(::time(nullptr));
    if (now - _eventBossLootRolledBundlesClearedAt > 30
        || _eventBossLootRolledBundles.size() > 10000)
    {
        _eventBossLootRolledBundles.clear();
        _eventBossLootRolledBundlesClearedAt = now;
    }
    if (!_eventBossLootRolledBundles.insert(bundleKey).second)
        return false;  // already injected for this bundle

    // Compute scaled level. Event bosses can outlive their rotation's
    // empowerment window (Slice 6 §2.9), so fall back to the player's
    // level when the zone isn't currently in rotation.
    uint8 highest = 0;
    {
        WorldSessionMgr::SessionMap const& sessions =
            sWorldSessionMgr->GetAllSessions();
        for (auto const& kv : sessions)
        {
            Player* p = RealPlayerFromSession(kv.second);
            if (!p)
                continue;
            if (p->IsGameMaster())
                continue;
            uint8 lvl = p->GetLevel();
            if (lvl > highest)
                highest = lvl;
        }
    }
    uint32 playerZone = player->GetZoneId();
    bool inRotation = false;
    Tier zoneTier = TIER_NONE;
    for (ActiveSlot const& s : _rotation.slots)
    {
        if (s.zoneId == playerZone)
        {
            inRotation = true;
            zoneTier = s.tier;
            break;
        }
    }
    uint8 zoneMin = 0;
    auto poolIt = _poolIndex.find(playerZone);
    if (poolIt != _poolIndex.end() && poolIt->second < _pool.size())
        zoneMin = static_cast<uint8>(_pool[poolIt->second].levelMin);
    uint8 tierVal = (zoneTier >= TIER_1 && zoneTier <= TIER_5)
        ? static_cast<uint8>(zoneTier) : 0;
    uint8 scaled = ComputeTargetLevelPure(inRotation, highest,
                                           zoneMin, tierVal,
                                           _maxPlayerLevel);
    if (scaled == 0)
        scaled = player->GetLevel();

    LootPoolEntry const* band = FindEventBossLootBand(scaled);
    if (!band)
        return false;

    uint32 blueId       = band->guaranteedBlueId;
    uint32 purpleId     = band->purpleItemId;
    float  purpleChance = band->purpleChance * _eventBossLootPurpleMultiplier;
    if (purpleChance < 0.0f) purpleChance = 0.0f;
    if (purpleChance > 1.0f) purpleChance = 1.0f;

    bool injected = false;
    if (blueId != 0)
    {
        LootStoreItem blue(blueId, 0, 100.0f, false,
                            LOOT_MODE_DEFAULT, 0, 1, 1);
        loot.AddItem(blue);
        injected = true;
    }
    if (purpleId != 0 && roll_chance_f(purpleChance * 100.0f))
    {
        LootStoreItem purple(purpleId, 0, 100.0f, false,
                              LOOT_MODE_DEFAULT, 0, 1, 1);
        loot.AddItem(purple);
        injected = true;
    }
    // Gold target. We pre-seed loot.gold here so the gold pile is
    // visible when the loot window opens (creatures with native
    // mingold/maxgold = 0 wouldn't otherwise have a gold pile to
    // click). For creatures with non-zero native gold,
    // generateMoneyLoot (Unit.cpp:14084, fires AFTER FillLoot) will
    // overwrite our value — the OnPlayerBeforeLootMoney hook
    // restores via ApplyEventBossGoldUplift using
    // _eventBossGoldTargets.
    if (band->goldMaxCopper >= band->goldMinCopper
        && band->goldMaxCopper > 0)
    {
        uint32 goldAdd = urand(band->goldMinCopper, band->goldMaxCopper);
        if (goldAdd > 0)
        {
            uint64 bundleKey = reinterpret_cast<uint64>(&loot);
            _eventBossGoldTargets[bundleKey] = goldAdd;
            loot.gold = std::max(loot.gold, goldAdd);
            injected = true;
        }
    }

    if (injected && _debug)
        LOG_INFO("module",
                 "mod-terror-zones: event-boss loot injected — player_guid={} "
                 "scaled={} blue={} purple={} (chance={:.2f}) "
                 "[bundle readback: items={} gold={} unlooted={}]",
                 player->GetGUID().GetCounter(),
                 static_cast<uint32>(scaled),
                 blueId, purpleId, purpleChance,
                 static_cast<uint32>(loot.items.size()),
                 loot.gold,
                 static_cast<uint32>(loot.unlootedCount));

    // Slice 9 Pass 1 — class-targeted drop, additive on top of the
    // band-pool injection above. Has its own dedup, T1 skip, drop
    // chance, and cell-populated guard.
    TryClassDrop(player, loot);

    return injected;
}
void TerrorZonesMgr::ApplyEventBossGoldUplift(Loot& loot,
                                                Player const* player)
{
    if (!player || !_enabled || !_eventsEnabled
        || !_eventBossLootPoolEnabled)
        return;

    uint64 rawGuid = loot.sourceWorldObjectGUID.GetRawValue();
    if (rawGuid == 0)
        return;
    if (!_eventBossSpawnIndex.count(rawGuid))
        return;

    // Restore the band-pool gold target captured during
    // TryEventBossDrop. generateMoneyLoot may have clobbered
    // loot.gold to the creature's native (often tiny) value.
    // max() is idempotent — duplicate clicks are no-ops.
    uint64 bundleKey = reinterpret_cast<uint64>(&loot);
    auto it = _eventBossGoldTargets.find(bundleKey);
    if (it == _eventBossGoldTargets.end())
        return;
    uint32 target = it->second;
    uint32 prevGold = loot.gold;
    if (loot.gold < target)
        loot.gold = target;

    if (_debug)
        LOG_INFO("module",
                 "mod-terror-zones: event-boss gold uplift — "
                 "player_guid={} prev={} target={} final={}",
                 player->GetGUID().GetCounter(),
                 prevGold, target, loot.gold);
}

}  // namespace mod_terror_zones
