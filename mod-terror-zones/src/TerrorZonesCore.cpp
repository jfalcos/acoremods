// Slice 1/8 — core lifecycle: singleton accessor, bot-exclusion helper,
// startup init, the 1Hz OnUpdate/ForceTick tick, and combat/rotation/
// event snapshot publishing for the lock-free hot-path readers.
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

TerrorZonesMgr& TerrorZonesMgr::Instance()
{
    static TerrorZonesMgr inst;
    return inst;
}
Player* TerrorZonesMgr::RealPlayerFromSession(WorldSession* session)
{
    if (!session || session->IsBot())
        return nullptr;
    Player* p = session->GetPlayer();
    if (!p || !p->IsInWorld())
        return nullptr;
    return p;
}

uint64 TerrorZonesMgr::AlignedBoundary(uint64 now, uint64 intervalSec)
{
    if (intervalSec == 0)
        return now;
    return now - (now % intervalSec);
}
// Slice 8 — rebuild the combat kHz-read snapshot from
// `_rotation.slots` + `_eventBossSpawnIndex`. World-thread-only;
// no synchronization needed against other writers.
void TerrorZonesMgr::PublishCombatHot()
{
    auto snap = std::make_shared<CombatHotState>();
    snap->slots.reserve(_rotation.slots.size());
    for (ActiveSlot const& s : _rotation.slots)
        snap->slots.push_back({s.zoneId, s.tier});
    snap->eventBossGuids.reserve(_eventBossSpawnIndex.size());
    for (auto const& kv : _eventBossSpawnIndex)
        snap->eventBossGuids.insert(kv.first);
    snap->eventBossTiers.reserve(_eventBossTierMap.size());
    for (auto const& kv : _eventBossTierMap)
        snap->eventBossTiers.emplace(kv.first, kv.second);
    snap->tickAt = _rotation.tickAt;
    std::atomic_store_explicit(&_combatHot,
                                std::shared_ptr<CombatHotState const>(snap),
                                std::memory_order_release);
}
// Slice 8b cleanup — publish a fresh immutable copy of the rotation
// for the read paths (IsZoneEmpowered, GetActiveRotation,
// SendTickLineTo, etc.).
void TerrorZonesMgr::PublishRotationSnap()
{
    auto snap = std::make_shared<ActiveRotation>(_rotation);
    std::atomic_store_explicit(&_rotationSnap,
                                std::shared_ptr<ActiveRotation const>(snap),
                                std::memory_order_release);
}
// Slice 8b cleanup — publish a fresh immutable copy of the live
// event list for `.zones event list` and the world-boss tracker
// loop. Vector copy is cheap (≤10 events in flight).
void TerrorZonesMgr::PublishEventsSnap()
{
    auto snap = std::make_shared<std::vector<ActiveEvent>>(_activeEvents);
    std::atomic_store_explicit(
        &_eventsSnap,
        std::shared_ptr<std::vector<ActiveEvent> const>(snap),
        std::memory_order_release);
}
// Slice 8b cleanup — publish a fresh immutable copy of the
// `(rawGuid → (tickAt, eventId))` index. Loot path reads the value
// (not just membership), so this is distinct from
// `_combatHot.eventBossGuids` which only carries membership.
void TerrorZonesMgr::PublishEventBossSpawnSnap()
{
    auto snap = std::make_shared<std::unordered_map<uint64,
                                                     std::pair<uint64, uint32>>>(
        _eventBossSpawnIndex);
    std::atomic_store_explicit(
        &_eventBossSpawnSnap,
        std::shared_ptr<std::unordered_map<uint64,
                                            std::pair<uint64, uint32>> const>(
            snap),
        std::memory_order_release);
}
void TerrorZonesMgr::InitializeOnStartup()
{
    if (!_enabled)
    {
        LOG_INFO("module", "mod-terror-zones: disabled by config; skipping init.");
        return;
    }

    LoadPool();
    BuildRarityIndex();
    BuildUniqueDropIndex();
    LoadEventContent();
    LoadEventBossLootPool();
    LoadClassDropIndex();

    uint64 now = static_cast<uint64>(::time(nullptr));
    uint64 currentBoundary = AlignedBoundary(now, _intervalSec);

    // Try to resume from last history row if it matches the current window.
    uint64 lastTickAt = 0;
    QueryResult r = CharacterDatabase.Query(
        "SELECT tick_at FROM terror_zones_history "
        "ORDER BY tick_at DESC LIMIT 1");
    if (r)
        lastTickAt = r->Fetch()[0].Get<uint64>();

    // Resume whenever the persisted rotation is current or newer than our
    // locally-computed boundary, not just on an exact match. A strict
    // `!=` here means any backward clock skew between worldserver restarts
    // (container clock briefly behind the previous instance's wall-clock —
    // observed in practice with frequent dev restarts) makes
    // `currentBoundary` resolve OLDER than `lastTickAt`. That used to fail
    // the equality check and run a brand-new RunRotation for the stale,
    // already-superseded boundary — re-picking zones (including ones that
    // still have a live, unexpired AlwaysSpawn world-boss event from the
    // newer rotation) and spawning a second boss on top of it. Treating
    // `lastTickAt >= currentBoundary` as "still current" closes that gap:
    // we only start a genuinely fresh rotation when wall-clock has moved
    // strictly past the last persisted tick.
    bool needFreshRotation = _startupForceTick
                          || (lastTickAt < currentBoundary);
    if (!_startupForceTick && lastTickAt > currentBoundary)
    {
        LOG_WARN("module",
                 "mod-terror-zones: startup boundary {} is older than the "
                 "latest persisted rotation tick_at={} (clock skew since "
                 "last restart?); resuming the newer rotation instead of "
                 "starting a stale one.",
                 currentBoundary, lastTickAt);
        currentBoundary = lastTickAt;
    }

    if (!needFreshRotation)
    {
        // Resume existing rotation from DB.
        QueryResult rr = CharacterDatabase.Query(
            "SELECT slot_index, zone_id, flavor, tier FROM terror_zones_history "
            "WHERE tick_at = {} ORDER BY slot_index ASC", lastTickAt);
        if (rr)
        {
            _rotation.slots.clear();
            _rotation.tickAt = lastTickAt;
            _rotation.expiresAt = lastTickAt + _intervalSec;
            do
            {
                Field* f = rr->Fetch();
                uint32 slotIndex = f[0].Get<uint32>();
                uint32 zoneId = f[1].Get<uint32>();
                uint8  flavor = f[2].Get<uint8>();
                uint8  tier   = f[3].Get<uint8>();
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
                _rotation.slots.push_back(std::move(s));
            } while (rr->NextRow());
            LOG_INFO("module",
                     "mod-terror-zones: resumed rotation from tick_at={} "
                     "({} slot(s)), expires at {}.",
                     lastTickAt,
                     static_cast<uint32>(_rotation.slots.size()),
                     _rotation.expiresAt);
            // Publish snapshots so the startup rescale walk + live
            // combat hooks see the resumed rotation immediately.
            PublishRotationSnap();
            PublishCombatHot();
        }
        else
        {
            needFreshRotation = true;
        }
    }

    // Re-apply atmosphere override on resumed slots. `sMapMgr->DoForAllMaps`
    // is a no-op if no maps are loaded yet (common at startup), in which
    // case the override lands later when the first player enters via
    // SendZoneDynamicInfo — but if this restart happened with players
    // still connected (`.worldserver restart`), we want to nudge the
    // override back in place immediately.
    if (!needFreshRotation && _flavorsEnabled)
    {
        for (ActiveSlot const& s : _rotation.slots)
            if (s.flavor != FLAVOR_NONE)
                ApplyAtmosphereOverride(s.zoneId, s.flavor);
    }

    // Slice 6 — restore active events that outlived the process exit.
    // Runs after atmosphere re-apply so any zone-scoped announcement
    // FireEvent sends lands on players already reconnected.
    LoadActiveEvents();

    if (needFreshRotation)
    {
        // Count online real-player sessions. During normal OnStartup this
        // is always zero (no one has logged in yet), but the helper keeps
        // the check correct if InitializeOnStartup is ever called after
        // the world is already populated (reload scenarios).
        uint32 realPlayersOnline = 0;
        WorldSessionMgr::SessionMap const& sessions =
            sWorldSessionMgr->GetAllSessions();
        for (auto const& kv : sessions)
        {
            Player* p = RealPlayerFromSession(kv.second);
            // GMs in GM mode are staff, not participants — don't let
            // one trigger the deferred startup rotation.
            if (p && !p->IsGameMaster())
                ++realPlayersOnline;
        }

        if (_startupForceTick || realPlayersOnline > 0)
        {
            RunRotation(currentBoundary, _announceStartupTick);
            _nextTickAt = currentBoundary + _intervalSec;
        }
        else
        {
            // Defer the first rotation until a real player logs in.
            // Rationale: with no online players, SelectZones falls back
            // to flat random over the whole 22-zone pool (targets=0),
            // which defeats the spec §3.1 "weight toward the levels of
            // players currently online" goal. OnPlayerLogin triggers
            // the rotation on the first real (non-bot) login.
            _rotationDeferredForFirstLogin = true;
            _nextTickAt = 0;  // OnUpdate short-circuits on 0
            LOG_INFO("module",
                     "mod-terror-zones: startup rotation deferred — no "
                     "real players online. First real-player login will "
                     "trigger it.");
        }
    }
    else
    {
        _nextTickAt = currentBoundary + _intervalSec;
    }
}
void TerrorZonesMgr::OnUpdate(uint32 diff)
{
    // Drain any resolved async queries (e.g. the RunRotation recency-history
    // lookup) regardless of the enabled/tick gating below.
    _queryProcessor.ProcessReadyCallbacks();

    if (!_enabled || _nextTickAt == 0)
        return;

    _tickAccumMs += diff;
    if (_tickAccumMs < 1000)
        return;
    _tickAccumMs = 0;

    uint64 now = static_cast<uint64>(::time(nullptr));

    // Slice 6 — drive event lifecycle on the same 1Hz cadence as
    // rotation. Runs before the rotation check so PENDING events
    // whose startsAt has passed fire on the same wall-clock second
    // the rotation would tick over.
    if (_eventsEnabled)
        TickEvents(now);

    // Slice 7 — rotation-ending warning. Single-shot per rotation
    // via `_lastRotationEndingWarnTickAt`. The pure helper enforces
    // the slack window + missed-window suppress so a restart-resume
    // doesn't late-fire.
    if (ShouldFireRotationEndingWarning(
            now, _nextTickAt, _rotationEndingLeadSec,
            kAnnounceWindowSec, _lastRotationEndingWarnTickAt))
    {
        SendRotationEndingWarning(_nextTickAt);
        _lastRotationEndingWarnTickAt = _nextTickAt;
    }

    if (now < _nextTickAt)
        return;

    // Collapse any missed boundaries into a single rotation — no catch-up.
    uint64 tickAt = AlignedBoundary(now, _intervalSec);
    RunRotation(tickAt, _announceServerWide);
    _nextTickAt = tickAt + _intervalSec;
}
void TerrorZonesMgr::ForceTick()
{
    uint64 now = static_cast<uint64>(::time(nullptr));
    uint64 tickAt = AlignedBoundary(now, _intervalSec);
    // Advance if the current boundary is already represented, to produce
    // visible rotation churn when the GM pokes the button repeatedly.
    if (tickAt == _rotation.tickAt)
        tickAt += _intervalSec;
    RunRotation(tickAt, true);
    _nextTickAt = tickAt + _intervalSec;
}

}  // namespace mod_terror_zones
