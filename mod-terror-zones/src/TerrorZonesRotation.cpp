// Slice 1 — rotation lifecycle: zone/tier/flavor selection at each
// rotation tick and the server-wide tick announcement.
#include "TerrorZonesCombatMgr.h"
#include "TerrorZonesContractMgr.h"
#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"
#include "TerrorZonesTierMgr.h"

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

void TerrorZonesMgr::RunRotation(uint64 tickAt, bool announce)
{
    // Gather targets (lowest-level-in-group, deduped across groups).
    std::vector<uint8> targets;
    {
        std::unordered_set<uint64> seenGroups;
        WorldSessionMgr::SessionMap const& sessions =
            sWorldSessionMgr->GetAllSessions();
        for (auto const& kv : sessions)
        {
            Player* p = RealPlayerFromSession(kv.second);
            if (!p)
                continue;
            // GMs in GM mode don't bias which zone is chosen.
            if (p->IsGameMaster())
                continue;
            Group* g = p->GetGroup();
            if (g)
            {
                uint64 key = static_cast<uint64>(
                    reinterpret_cast<uintptr_t>(g));
                if (!seenGroups.insert(key).second)
                    continue;
                uint8 minLevel = p->GetLevel();
                for (GroupReference* itr = g->GetFirstMember();
                     itr != nullptr; itr = itr->next())
                {
                    Player* m = itr->GetSource();
                    if (m && m->GetLevel() < minLevel)
                        minLevel = m->GetLevel();
                }
                targets.push_back(minLevel);
            }
            else
            {
                targets.push_back(p->GetLevel());
            }
        }
    }

    // Recent zone IDs: last slotCount*recencyWindow rows. In
    // per-continent mode the effective slot count is the number of
    // continents (bounded by 4 in 3.3.5a), so use that to keep the
    // dampening window covering a few full rotations. Fetched async so
    // the world thread never blocks on this DB round-trip; the rest of
    // RunRotation resumes in RunRotationContinued once it resolves.
    uint32 effectiveSlots = _onePerContinent ? 4u : _slotCount;
    if (_recencyWindow > 0 && effectiveSlots > 0)
    {
        uint32 limit = _recencyWindow * effectiveSlots;
        std::string sql = Acore::StringFormat(
            "SELECT zone_id FROM terror_zones_history "
            "ORDER BY tick_at DESC, slot_index ASC LIMIT {}", limit);
        _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(sql)
            .WithCallback([this, tickAt, announce, targets](QueryResult r)
        {
            std::vector<uint32> recent;
            if (r)
            {
                do
                {
                    recent.push_back(r->Fetch()[0].Get<uint32>());
                } while (r->NextRow());
            }
            RunRotationContinued(tickAt, announce, targets, std::move(recent));
        }));
        return;
    }

    RunRotationContinued(tickAt, announce, targets, {});
}
void TerrorZonesMgr::RunRotationContinued(uint64 tickAt, bool announce,
                                           std::vector<uint8> targets,
                                           std::vector<uint32> recent)
{
    // `_pool` is write-once at LoadPool, so reading it directly on the
    // world thread is safe.
    SelectionConfig cfg {
        _levelWindow, _weightNear, _weightOverlap,
        _weightBelow, _weightAbove, _recencyMultiplier, _slotCount
    };

    uint64 rngSeed = static_cast<uint64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    rngSeed ^= tickAt * 0x9E3779B97F4A7C15ULL;
    StdRng rng(rngSeed);

    std::vector<uint32> picks = _onePerContinent
        ? SelectZonesPerContinent(_pool, targets, recent, cfg, rng)
        : SelectZones(_pool, targets, recent, cfg, rng);

    // Roll one flavor per slot, independent of zone selection (plan §2.2).
    std::vector<Flavor> pickFlavors;
    pickFlavors.reserve(picks.size());
    if (_flavorsEnabled)
    {
        for (size_t i = 0; i < picks.size(); ++i)
        {
            Flavor f = SelectFlavor(_flavorWeights, rng);
            if (f == FLAVOR_NONE)
            {
                LOG_WARN("module",
                         "mod-terror-zones: all flavor weights zero, "
                         "falling back to Bloodbath for slot {}.",
                         static_cast<uint32>(i));
                f = FLAVOR_BLOODBATH;
            }
            pickFlavors.push_back(f);
        }
    }
    else
    {
        pickFlavors.assign(picks.size(), FLAVOR_NONE);
    }

    // Slice 5 — roll one tier per slot, independent of zone + flavor.
    std::vector<Tier> pickTiers;
    pickTiers.reserve(picks.size());
    if (TerrorZonesTierMgr::Instance().IsTierEnabled())
    {
        for (size_t i = 0; i < picks.size(); ++i)
        {
            Tier t = TerrorZonesTierMgr::Instance().RollTierWeighted(rng);
            if (t == TIER_NONE)
            {
                LOG_WARN("module",
                         "mod-terror-zones: all tier rarity weights zero, "
                         "falling back to Tier 1 for slot {}.",
                         static_cast<uint32>(i));
                t = TIER_1;
            }
            pickTiers.push_back(t);
        }
    }
    else
    {
        // Slice 4 fallback — tier system disabled, every slot reads as
        // Tier 1 for any .zones display but Apply*/Try* use the flat
        // Slice 4 overlays instead of tier rolls.
        pickTiers.assign(picks.size(), TIER_NONE);
    }

    if (picks.empty())
    {
        uint32 enabledCount = 0;
        for (PoolEntry const& z : _pool)
            if (z.enabled) ++enabledCount;
        LOG_WARN("module",
                 "mod-terror-zones: SelectZones returned no picks at tick_at={} "
                 "(pool size={}, enabled={}).",
                 tickAt, static_cast<uint32>(_pool.size()),
                 enabledCount);
        return;
    }

    // Write history synchronously.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (size_t i = 0; i < picks.size(); ++i)
    {
        trans->Append(
            "INSERT INTO terror_zones_history (tick_at, slot_index, zone_id, flavor, tier) "
            "VALUES ({}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE zone_id = VALUES(zone_id), "
            "flavor = VALUES(flavor), tier = VALUES(tier)",
            tickAt, static_cast<uint32>(i), picks[i],
            static_cast<uint32>(pickFlavors[i]),
            static_cast<uint32>(pickTiers[i]));
    }
    CharacterDatabase.CommitTransaction(trans);

    // Update live rotation. Capture the previous slot set for
    // edge-diff (tick-on / tick-off walks). World-thread-only — no
    // synchronization against other writers.
    std::vector<uint32> prevZones;
    std::vector<std::pair<uint32, std::string>> prevZoneNames;
    prevZones.reserve(_rotation.slots.size());
    prevZoneNames.reserve(_rotation.slots.size());
    for (ActiveSlot const& s : _rotation.slots)
    {
        prevZones.push_back(s.zoneId);
        prevZoneNames.emplace_back(s.zoneId, s.displayName);
    }
    ActiveRotation rot;
    rot.tickAt = tickAt;
    rot.expiresAt = tickAt + _intervalSec;
    for (size_t i = 0; i < picks.size(); ++i)
    {
        uint32 zoneId = picks[i];
        ActiveSlot s;
        s.zoneId = zoneId;
        s.slotIndex = static_cast<uint32>(i);
        auto it = _poolIndex.find(zoneId);
        s.displayName = (it != _poolIndex.end())
                      ? _pool[it->second].displayName
                      : std::to_string(zoneId);
        s.flavor = pickFlavors[i];
        s.tier   = pickTiers[i];
        rot.slots.push_back(std::move(s));
    }
    _rotation = rot;
    // Publish snapshots so the rescale walk below + the combat hot
    // paths see the new rotation without any synchronization.
    PublishRotationSnap();
    PublishCombatHot();

    // Edge-diff walk the creature maps. Must happen AFTER _rotation is
    // updated so ComputeTargetLevel inside OnBeforeCreatureSelectLevel
    // observes the new state. Tick-off first so a slot that left gets
    // baselined before tick-on for a new pick could conflict (can't
    // happen with dedup, but ordering is cheap).
    std::unordered_set<uint32> newSet(picks.begin(), picks.end());
    std::unordered_set<uint32> prevSet(prevZones.begin(), prevZones.end());
    for (auto const& zn : prevZoneNames)
    {
        uint32 z = zn.first;
        if (newSet.count(z))
            continue;
        // Slice 6 — end any active/pending events in a zone that
        // is leaving the rotation. Keeps event lifetime bounded
        // by the zone's empowerment window: if the rotation
        // rotates off, the world boss / node surge despawn with
        // it rather than lingering until their duration expires.
        EndActiveEventsInZone(z);
        TerrorZonesCombatMgr::Instance().WalkZoneRescale(z, /*edgeOn*/ false);
        RestoreAtmosphere(z);
        // Slice 7 — zone-scoped fade line, gated per-player by the
        // RotationEnd category. Fires after the rescale + atmosphere
        // restore so any straggling tick-driven message is the last
        // thing the player sees from this rotation.
        SendRotationEndLineFor(z, zn.second);
    }

    // Slice 10 Pass 2 — settle + mail the bounties of the rotation(s)
    // that just ended: every unmailed contract row with tick_at < this
    // new tick. Offline-safe + restart-recovering (a missed rotation's
    // rows are mailed on the next tick). Runs after the tick-off rescale
    // so it's the last thing tied to the ending rotation.
    TerrorZonesContractMgr::Instance().MailContractRewards(tickAt);
    // New rotation begins — reset the best-effort progress-message cache
    // and the per-rotation group-scale tracking set.
    TerrorZonesContractMgr::Instance().ClearMsgCreditCache();
    TerrorZonesCombatMgr::Instance().ClearGroupScaledGuids();

    for (size_t i = 0; i < picks.size(); ++i)
    {
        uint32 z = picks[i];
        if (!prevSet.count(z))
        {
            TerrorZonesCombatMgr::Instance().WalkZoneRescale(z, /*edgeOn*/ true);
            ApplyAtmosphereOverride(z, pickFlavors[i]);
        }
    }

    // Slice 6 — schedule dynamic events for the new rotation's slots.
    // Runs after atmosphere so the event's zone already has its new
    // weather if a late-firing FireEvent sends announcements first.
    ScheduleEvents(tickAt, rot.slots);

    {
        std::string slotList;
        for (size_t i = 0; i < rot.slots.size(); ++i)
        {
            if (i) slotList += ", ";
            slotList += rot.slots[i].displayName;
            slotList += "/";
            slotList += TierDisplayName(rot.slots[i].tier);
            slotList += "/";
            slotList += FlavorDisplayName(rot.slots[i].flavor);
        }
        LOG_INFO("module",
                 "mod-terror-zones: rotation tick_at={} slots=[{}] "
                 "targets={} recent={}",
                 tickAt, slotList,
                 static_cast<uint32>(targets.size()),
                 static_cast<uint32>(recent.size()));
    }

    if (announce)
        AnnounceRotation(rot);
}
void TerrorZonesMgr::AnnounceRotation(ActiveRotation const& rot)
{
    uint32 remaining = _intervalSec;  // full window at tick time

    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;
        if (!TerrorZonesPlayerPrefsMgr::Instance().IsCategoryEnabledFor(p, ANNOUNCE_ROTATION_TICK))
            continue;
        for (ActiveSlot const& s : rot.slots)
            SendTickLineTo(p, s.displayName, remaining);
    }
}

}  // namespace mod_terror_zones
