// Slice 1/7 — login/zone-change hooks. Per-player announcement
// preference storage itself lives in TerrorZonesPlayerPrefsMgr (full
// decomposition split) -- this file only orchestrates it alongside the
// rotation/scaling side effects a zone crossing triggers.
#include "TerrorZonesCombatMgr.h"
#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"

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

void TerrorZonesMgr::OnPlayerLogin(Player* player)
{
    if (!_enabled || !player)
        return;

    // Trigger the deferred startup rotation on the first real-player
    // login. Bots (Playerbot-populated sessions) don't count — we want
    // the rotation to weight toward an actual human's level.
    if (_rotationDeferredForFirstLogin)
    {
        WorldSession* session = player->GetSession();
        if (session && !session->IsBot() && player->IsInWorld())
        {
            _rotationDeferredForFirstLogin = false;
            uint64 now = static_cast<uint64>(::time(nullptr));
            uint64 boundary = AlignedBoundary(now, _intervalSec);
            LOG_INFO("module",
                     "mod-terror-zones: first real-player login, running "
                     "deferred startup rotation at tick_at={}.", boundary);
            RunRotation(boundary, _announceStartupTick);
            _nextTickAt = boundary + _intervalSec;
        }
    }

    TerrorZonesPlayerPrefsMgr& prefs = TerrorZonesPlayerPrefsMgr::Instance();
    prefs.LoadPlayerPref(player);
    uint32 zone = player->GetZoneId();
    std::string name;
    uint32 remaining = 0;
    bool empowered = IsZoneEmpowered(zone, &name, &remaining);
    if (empowered)
    {
        // Seed the last-empowered cache so a subsequent zone-leave
        // can fire its line. Done regardless of category gate state
        // — the cache is module-internal bookkeeping.
        uint32 guidLow = player->GetGUID().GetCounter();
        prefs.SetLastEmpoweredZone(guidLow, zone, name);
    }
    if (!prefs.IsCategoryEnabledFor(player, ANNOUNCE_ZONE_ENTRY))
        return;
    if (empowered)
        SendEntryLineTo(player, name, remaining);
}
void TerrorZonesMgr::OnPlayerUpdateZone(Player* player, uint32 newZone)
{
    if (!_enabled || !player)
        return;

    // Snapshot the player's last-empowered zone (if any), then
    // update the cache for the new zone. Per-player prefs are
    // world-thread-only; no synchronization needed.
    TerrorZonesPlayerPrefsMgr& prefs = TerrorZonesPlayerPrefsMgr::Instance();
    uint32 guidLow = player->GetGUID().GetCounter();
    TerrorZonesPlayerPrefsMgr::LastEmpoweredZone last =
        prefs.GetLastEmpoweredZone(guidLow);
    uint32 oldZone = last.zoneId;
    std::string oldName = last.zoneName;

    std::string newName;
    uint32 remaining = 0;
    bool newEmpowered = IsZoneEmpowered(newZone, &newName, &remaining);

    // Update the cache to reflect the new zone state. Stays empty
    // when newZone isn't empowered so the next entry crossing fires
    // the entry line cleanly.
    if (newEmpowered)
        prefs.SetLastEmpoweredZone(guidLow, newZone, newName);
    else
        prefs.SetLastEmpoweredZone(guidLow, 0, "");

    // Leave-line: fired when the player crosses out of an empowered
    // zone (cached) into a different zone, regardless of whether the
    // new zone is empowered. Skip the cross between two empowered
    // slots in the same rotation — the entry line already handles
    // that with full info.
    if (oldZone != 0 && oldZone != newZone)
    {
        if (prefs.IsCategoryEnabledFor(player, ANNOUNCE_ZONE_LEAVE))
            SendZoneLeaveLineTo(player, oldName);

        // Core gap: Map::SendZoneDynamicInfo (called earlier in the same
        // Player::UpdateZone, just before this hook fires) silently no-ops
        // for newZone if that zone has never had a _zoneDynamicInfo entry
        // created — which is exactly the case for any zone with no
        // game_weather rows. That left a flavor's weather override (e.g.
        // Bloodbath's BLACKRAIN) rendering forever on the client once a
        // player walked out of the empowered zone into one of those.
        // Mirror the zone-data lookup ourselves: if the destination zone
        // genuinely has weather data, GetOrGenerateZoneDefaultWeather is a
        // harmless idempotent re-check (core already created/sent it);
        // if it doesn't, force an explicit FINE push so the override can't
        // linger.
        if (Map* map = player->GetMap())
        {
            if (Weather* w = map->GetOrGenerateZoneDefaultWeather(newZone))
                w->SendWeatherUpdateToPlayer(player);
            else
                Weather::SendFineWeatherUpdateToPlayer(player);
        }
    }

    // Entry-line: fired when the player crosses into an empowered
    // zone they weren't already in.
    if (newEmpowered && oldZone != newZone)
    {
        if (prefs.IsCategoryEnabledFor(player, ANNOUNCE_ZONE_ENTRY))
            SendEntryLineTo(player, newName, remaining);
    }

    // Recompute mob levels against the real players now present.
    // ComputeTargetLevel is zone-scoped, so a player entering an
    // empowered zone (or leaving one) changes the aggregate that
    // already-spawned mobs were scaled to. Without this re-walk the
    // levels stay frozen at whatever they were when the zone last
    // ticked — the exact reason a max-level player could find an
    // empowered low zone still full of native-level mobs.
    //
    // Gated to real (non-bot) players for two reasons: bots are
    // excluded from the level aggregate (so their crossings can't
    // change the target), and with ~hundreds of bots a full-zone
    // SelectLevel walk on every bot border-cross would be a serious
    // perf cost. force=true bypasses the RescaleOnTick gate — a
    // player physically entering is an explicit trigger.
    //
    // Debounced per zone (WalkZoneRescaleDebounced): a player riding a
    // transport whose flight path clips a zone's boundary (e.g. the
    // Skybreaker patrolling in and out of Icecrown) can flip GetZoneId()
    // back and forth many times a minute, and each flip reaches this
    // hook as a genuine zone crossing. Without a rate limit that turns
    // into a repeated whole-zone SelectLevel walk (thousands of
    // creatures) firing back-to-back, which stalls the world thread
    // often enough to visibly pop nearby NPCs in and out of existence
    // for anyone aboard. The debounce still lets the first crossing
    // through immediately; only the rapid repeats within the window
    // get skipped.
    TerrorZonesCombatMgr& combat = TerrorZonesCombatMgr::Instance();
    if (combat.IsScalingEnabled() && oldZone != newZone)
    {
        WorldSession* s = player->GetSession();
        if (s && !s->IsBot())
        {
            if (newEmpowered)
                combat.WalkZoneRescaleDebounced(newZone, /*edgeOn*/ true,
                                                 /*force*/ true);
            // Leaving an empowered zone: re-walk it so its mobs reflect
            // whoever remains (or fall back to native once empty).
            if (oldZone != 0)
                combat.WalkZoneRescaleDebounced(oldZone, /*edgeOn*/ true,
                                                 /*force*/ true);
        }
    }
}

}  // namespace mod_terror_zones
