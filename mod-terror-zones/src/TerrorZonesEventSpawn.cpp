// Slice 6 — event entity spawn/despawn: world boss + node surge
// creature/gameobject summoning and cleanup.
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

void TerrorZonesMgr::FireEvent(ActiveEvent& evt)
{
    // World-thread-only — `_eventBossDefs` and `_eventNodeSurgeDefs`
    // are write-once at LoadEventContent; safe to read directly.
    switch (evt.type)
    {
        case EVENT_WORLD_BOSS:
        {
            // Defensive guard: ScheduleEvents already excludes
            // EVENT_WORLD_BOSS from the second-event roll once the
            // guaranteed boss is scheduled, but the GM `.zones event fire`
            // path (FireEventNow) bypasses ScheduleEvents entirely and
            // isn't covered by that fix. Skip spawning a duplicate boss
            // (without ever going ACTIVE) if another one is already live
            // in the same zone.
            bool duplicateBoss = false;
            for (ActiveEvent const& other : _activeEvents)
            {
                if (&other == &evt)
                    continue;
                if (other.type == EVENT_WORLD_BOSS
                    && other.zoneId == evt.zoneId
                    && other.state == EVENT_STATE_ACTIVE)
                {
                    duplicateBoss = true;
                    break;
                }
            }
            if (duplicateBoss)
            {
                LOG_WARN("module",
                         "mod-terror-zones: skipped duplicate world boss "
                         "fire — zone={} already has an active world boss "
                         "(def={}, tick_at={}, slot={})",
                         evt.zoneId, evt.definitionId, evt.tickAt,
                         evt.slotIndex);
                evt.state = EVENT_STATE_EXPIRED;
                PersistEventState(evt);
                return;
            }

            EventBossDef const* def = nullptr;
            for (EventBossDef const& d : _eventBossDefs)
                if (d.id == evt.definitionId)
                {
                    def = &d;
                    break;
                }
            if (def)
                SpawnWorldBoss(evt, *def);
            break;
        }
        case EVENT_RARE_NODE_SURGE:
        {
            EventNodeSurgeDef const* def = nullptr;
            for (EventNodeSurgeDef const& d : _eventNodeSurgeDefs)
                if (d.id == evt.definitionId)
                {
                    def = &d;
                    break;
                }
            if (def)
                SpawnNodeSurge(evt, *def);
            break;
        }
        default:
            break;
    }

    evt.state = EVENT_STATE_ACTIVE;
    PersistEventState(evt);

    // Zone-scoped announcement (§6.4).
    std::string line;
    {
        char buf[256];
        uint32 remainingSec =
            (evt.endsAt > evt.startsAt)
                ? static_cast<uint32>(evt.endsAt - evt.startsAt)
                : _eventCfg.durationSec;
        switch (evt.type)
        {
            case EVENT_WORLD_BOSS:
                std::snprintf(buf, sizeof(buf),
                    "|cffff8040A dreadful presence stirs — %s has appeared "
                    "near (%.0f, %.0f). (~%u minutes.)|r",
                    evt.displayName.c_str(),
                    evt.anchorX, evt.anchorY, remainingSec / 60);
                break;
            case EVENT_RARE_NODE_SURGE:
                std::snprintf(buf, sizeof(buf),
                    "|cffff8040A rich vein swells through %s at "
                    "(%.0f, %.0f) — gatherers, head out. (~%u minutes.)|r",
                    evt.displayName.c_str(),
                    evt.anchorX, evt.anchorY, remainingSec / 60);
                break;
            default:
                std::snprintf(buf, sizeof(buf),
                    "|cffff8040A %s event has begun. (~%u minutes.)|r",
                    EventTypeDisplayName(evt.type), remainingSec / 60);
                break;
        }
        line = buf;
    }
    uint32 zoneId = evt.zoneId;
    BroadcastZoneLineGated(zoneId, line, ANNOUNCE_EVENT_START);

    LOG_INFO("module",
             "mod-terror-zones: event fired type={} def={} zone={} "
             "spawns=[creatures={},objects={}] ends_at={}",
             EventTypeDisplayName(evt.type), evt.definitionId, evt.zoneId,
             static_cast<uint32>(evt.spawnedCreatures.size()),
             static_cast<uint32>(evt.spawnedGameObjects.size()),
             evt.endsAt);
}
void TerrorZonesMgr::EndEvent(ActiveEvent& evt)
{
    DespawnEventCreatures(evt);
    DespawnEventGameObjects(evt);

    evt.state = EVENT_STATE_EXPIRED;
    PersistEventState(evt);

    std::string line;
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "|cffff8040%s's presence fades — the rotation's event has ended.|r",
            evt.displayName.c_str());
        line = buf;
    }
    uint32 zoneId = evt.zoneId;
    BroadcastZoneLineGated(zoneId, line, ANNOUNCE_EVENT_END);

    LOG_INFO("module",
             "mod-terror-zones: event ended type={} def={} zone={}",
             EventTypeDisplayName(evt.type), evt.definitionId, evt.zoneId);
}
uint8 TerrorZonesMgr::ComputeEventBossApex(EventBossDef const& def) const
{
    // Server-wide apex across real-player sessions (bots excluded).
    // Mirrors ComputeTargetLevel's player walk, but is callable
    // regardless of rotation state. Event bosses always scale to the
    // apex so a GM force-fire in a non-empowered zone still produces
    // a credible fight.
    uint8 highest = 0;
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

    // `_pool` / `_poolIndex` are write-once at LoadPool.
    uint16 poolLevelMax = 0;
    auto it = _poolIndex.find(def.zoneId);
    if (it != _poolIndex.end())
        poolLevelMax = _pool[it->second].levelMax;

    uint8 apex = std::max<uint8>(
        {static_cast<uint8>(poolLevelMax), highest, def.levelMax});
    if (apex < def.levelMin)
        apex = def.levelMin;
    // CreatureBaseStats ships data up to level 83; anything above
    // that reads zero stats. Clamp defensively.
    if (apex > 83)
        apex = 83;
    return apex;
}
void TerrorZonesMgr::SpawnWorldBoss(ActiveEvent& evt,
                                    EventBossDef const& def)
{
    Map* map = sMapMgr->FindBaseNonInstanceMap(def.anchorMap);
    if (!map)
    {
        LOG_WARN("module",
                 "mod-terror-zones: event boss spawn — base map {} not loaded; "
                 "boss def {} deferred (will retry on restart-resume).",
                 def.anchorMap, def.id);
        return;
    }
    Position pos{ def.anchorX, def.anchorY, def.anchorZ, def.anchorO };
    // Summon for the event's actual remaining window rather than the
    // global DurationSec: the AlwaysSpawn boss runs the FULL rotation
    // (endsAt = tickAt + intervalSec), which is longer than DurationSec.
    // For normal events (endsAt = startsAt + DurationSec, fired at
    // ~startsAt) this resolves to ~DurationSec, so behavior is unchanged.
    uint64 nowSec = static_cast<uint64>(::time(nullptr));
    uint32 windowSec = (evt.endsAt > nowSec)
        ? static_cast<uint32>(evt.endsAt - nowSec)
        : _eventCfg.durationSec;
    uint32 durMs = windowSec * 1000;

    // Force the scaling override before SummonCreature. The hook
    // fires synchronously inside Create → SelectLevel on this same
    // thread, so the override is live for exactly the one
    // SelectLevel call we care about. Cleared in a guard below.
    uint8 apex = ComputeEventBossApex(def);

    // Resolve the source rotation slot's tier so HP scaling can
    // pin to it (decoupled from the boss's spawn zone). GM-forced
    // events use slotIndex 0xFFFF — fall back to TIER_5.
    Tier eventTier = TIER_5;
    if (evt.slotIndex < _rotation.slots.size())
        eventTier = _rotation.slots[evt.slotIndex].tier;
    if (eventTier == TIER_NONE)
        eventTier = TIER_5;

    // Anti-duplication fix: if a native creature was matched to this
    // anchor at LoadEventContent AND it's currently alive, possess it
    // in place (force-relevel it to apex via the same override the
    // TempSummon path below uses) instead of summoning a second copy
    // that would stand right next to it. Falls through to the
    // TempSummon path below when there's no match or the native is
    // currently dead/not loaded — exactly today's behavior.
    if (def.nativeSpawnGuid != 0)
    {
        ObjectGuid nativeGuid = ObjectGuid::Create<HighGuid::Unit>(
            def.creatureTemplateId, def.nativeSpawnGuid);
        Creature* native = map->GetCreature(nativeGuid);
        if (native && native->IsAlive())
        {
            _eventBossScaleOverride.store(apex, std::memory_order_relaxed);
            _eventBossSpawnPending.store(true, std::memory_order_relaxed);
            _eventBossTierOverride.store(static_cast<uint8>(eventTier),
                                          std::memory_order_relaxed);

            native->SelectLevel(true);

            _eventBossScaleOverride.store(0, std::memory_order_relaxed);
            _eventBossSpawnPending.store(false, std::memory_order_relaxed);
            _eventBossTierOverride.store(0, std::memory_order_relaxed);

            if (_debug)
                LOG_INFO("module",
                         "mod-terror-zones: event boss possessed native "
                         "guid={} entry={} level={} (apex={}) zone={} "
                         "pos=({:.1f},{:.1f},{:.1f})",
                         def.nativeSpawnGuid, def.creatureTemplateId,
                         static_cast<uint32>(native->GetLevel()),
                         static_cast<uint32>(apex), evt.zoneId,
                         def.anchorX, def.anchorY, def.anchorZ);

            if (_eventBossScaleMult > 0.0f && _eventBossScaleMult != 1.0f)
                native->SetObjectScale(
                    native->GetObjectScale() * _eventBossScaleMult);
            evt.spawnedCreatures.push_back(native->GetGUID());
            evt.bossIsNative = true;

            if (_eventBossBeaconGoId != 0)
            {
                GameObject* beacon = map->SummonGameObject(
                    _eventBossBeaconGoId, def.anchorX, def.anchorY,
                    def.anchorZ, def.anchorO, 0.0f, 0.0f, 0.0f, 0.0f,
                    windowSec, /*checkTransport*/ false);
                if (beacon)
                    evt.spawnedGameObjects.push_back(beacon->GetGUID());
                else if (_debug)
                    LOG_INFO("module",
                             "mod-terror-zones: event boss beacon spawn "
                             "failed — go_entry={} map={} (template "
                             "missing?)",
                             _eventBossBeaconGoId, def.anchorMap);
            }
            return;
        }
    }

    _eventBossScaleOverride.store(apex, std::memory_order_relaxed);
    _eventBossSpawnPending.store(true, std::memory_order_relaxed);
    _eventBossTierOverride.store(static_cast<uint8>(eventTier),
                                  std::memory_order_relaxed);

    TempSummon* boss = map->SummonCreature(
        def.creatureTemplateId, pos, nullptr, durMs, nullptr);

    // Always clear the override flags immediately after the summon,
    // whether it succeeded or not. Subsequent summons in this call
    // (beacon, future markers) MUST NOT pick up either flag.
    _eventBossScaleOverride.store(0, std::memory_order_relaxed);
    _eventBossSpawnPending.store(false, std::memory_order_relaxed);
    _eventBossTierOverride.store(0, std::memory_order_relaxed);

    if (!boss)
    {
        LOG_WARN("module",
                 "mod-terror-zones: event boss SummonCreature failed — "
                 "template={} map={} pos=({:.1f},{:.1f},{:.1f})",
                 def.creatureTemplateId, def.anchorMap,
                 def.anchorX, def.anchorY, def.anchorZ);
        return;
    }

    if (_debug)
        LOG_INFO("module",
                 "mod-terror-zones: event boss spawned entry={} level={} "
                 "(apex={}) zone={} pos=({:.1f},{:.1f},{:.1f})",
                 def.creatureTemplateId,
                 static_cast<uint32>(boss->GetLevel()),
                 static_cast<uint32>(apex), evt.zoneId,
                 def.anchorX, def.anchorY, def.anchorZ);

    // Scale up so the boss reads as dramatic-sized. Runtime-only — we
    // don't mutate creature_template. Clamp guard applies in LoadConfig.
    if (_eventBossScaleMult > 0.0f && _eventBossScaleMult != 1.0f)
        boss->SetObjectScale(boss->GetObjectScale() * _eventBossScaleMult);
    ObjectGuid bossGuid = boss->GetGUID();
    evt.spawnedCreatures.push_back(bossGuid);
    // NOTE: `_eventBossSpawnIndex` commit + snapshot publish happens
    // in TickEvents / FireEventNow once FireEvent returns. We don't
    // touch the index here so callers stay in control of the
    // commit-then-publish ordering.

    // Beacon GO at the anchor so players can spot the event from
    // across the zone. Any gameobject entry works — default is a
    // vertical Energy Column (191763). Admins can swap via
    // TerrorZones.Events.WorldBoss.BeaconGameObjectId=0 to disable.
    if (_eventBossBeaconGoId != 0)
    {
        GameObject* beacon = map->SummonGameObject(
            _eventBossBeaconGoId, def.anchorX, def.anchorY, def.anchorZ,
            def.anchorO, 0.0f, 0.0f, 0.0f, 0.0f,
            windowSec, /*checkTransport*/ false);
        if (beacon)
            evt.spawnedGameObjects.push_back(beacon->GetGUID());
        else if (_debug)
            LOG_INFO("module",
                     "mod-terror-zones: event boss beacon spawn failed — "
                     "go_entry={} map={} (template missing?)",
                     _eventBossBeaconGoId, def.anchorMap);
    }
}
void TerrorZonesMgr::SpawnNodeSurge(ActiveEvent& evt,
                                    EventNodeSurgeDef const& def)
{
    Map* map = sMapMgr->FindBaseNonInstanceMap(def.anchorMap);
    if (!map)
    {
        LOG_WARN("module",
                 "mod-terror-zones: node surge spawn — base map {} not loaded; "
                 "def {} deferred.", def.anchorMap, def.id);
        return;
    }
    float radius = def.radius > 0.0f
                 ? def.radius : _eventNodeSurgeDefaultRadius;
    uint32 nodeCount = def.nodeCount > 0
                     ? def.nodeCount : _eventNodeSurgeDefaultCount;
    if (def.nodeIds.empty())
        return;

    // Seed uniquely per event so two node surges in the same tick have
    // different placements.
    uint64 seed = static_cast<uint64>(evt.tickAt)
                ^ (static_cast<uint64>(evt.eventId) << 32)
                ^ 0xA5A5A5A5A5A5A5A5ULL;
    StdRng rng(seed);

    // Cluster-center beacon so players can find the surge from distance.
    // Goes in first so it's at the exact anchor Z (not height-recovered).
    if (_eventNodeSurgeBeaconGoId != 0)
    {
        GameObject* beacon = map->SummonGameObject(
            _eventNodeSurgeBeaconGoId,
            def.anchorX, def.anchorY, def.anchorZ,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            _eventCfg.durationSec, /*checkTransport*/ false);
        if (beacon)
            evt.spawnedGameObjects.push_back(beacon->GetGUID());
    }

    uint32 spawned = 0;
    for (uint32 i = 0; i < nodeCount; ++i)
    {
        uint32 goEntry =
            def.nodeIds[rng.NextUInt(
                static_cast<uint32>(def.nodeIds.size()))];
        float nx, ny;
        PickSubregionAnchor(def.anchorX, def.anchorY, radius,
                             rng, nx, ny);
        // Recover a valid Z via the map's height lookup from a point
        // above the anchor's Z. If that fails, fall back to the
        // anchor Z — worst case the node floats 2m off the ground.
        float probeZ = def.anchorZ + _eventNodeSurgeZIgnore;
        float nz = map->GetHeight(nx, ny, probeZ, true);
        // Map::GetHeight returns INVALID_HEIGHT (~-100000) when no
        // terrain data is loaded at (x,y). Fall back to the anchor Z
        // so worst case the node floats a few meters off the ground
        // rather than through the world.
        if (nz < -10000.0f)
            nz = def.anchorZ;

        // respawnTime=_eventCfg.durationSec marks the GO as temporary
        // (SpellId=1 per Map::SummonGameObject), and SpawnedByDefault
        // is set to false so it doesn't auto-respawn after gather.
        // We despawn anything still alive at EndEvent.
        GameObject* go = map->SummonGameObject(
            goEntry, nx, ny, nz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            _eventCfg.durationSec, /*checkTransport*/ false);
        if (go)
        {
            evt.spawnedGameObjects.push_back(go->GetGUID());
            ++spawned;
        }
    }

    if (_debug)
        LOG_INFO("module",
                 "mod-terror-zones: node surge spawn def={} zone={} "
                 "anchor=({:.1f},{:.1f},{:.1f}) radius={:.1f} spawned={}/{}",
                 def.id, def.zoneId,
                 def.anchorX, def.anchorY, def.anchorZ, radius,
                 spawned, nodeCount);
}
void TerrorZonesMgr::DespawnEventCreatures(ActiveEvent& evt)
{
    // `_eventBossSpawnIndex` erase + snapshot publish happens in the
    // caller (TickEvents / EndActiveEventsInZone) once EndEvent
    // returns — stays in control of the commit-then-publish ordering.
    if (evt.spawnedCreatures.empty())
        return;
    Map* map = sMapMgr->FindBaseNonInstanceMap(evt.mapId);
    for (ObjectGuid const& guid : evt.spawnedCreatures)
    {
        if (!map)
            continue;
        Creature* c = map->GetCreature(guid);
        if (!c)
            continue;  // already auto-despawned by TempSummon timer, or
                       // (native case) already dead/corpse-removed —
                       // nothing to revert either way
        if (evt.bossIsNative)
        {
            // Anti-duplication fix: this GUID is a possessed native
            // creature, not a module-owned TempSummon — despawning it
            // would remove real world content. The caller already
            // erased it from `_eventBossSpawnIndex` before calling
            // EndEvent, so re-running SelectLevel here (no override set)
            // falls through to normal Slice-2 zone scaling / native
            // level roll — the same in-place revert WalkZoneRescale uses
            // when a zone de-empowers. Object scale isn't touched by
            // SelectLevel, so it needs its own explicit revert back to
            // the template's native scale (SpawnWorldBoss only bumps it
            // for the possess path, never resets it itself).
            c->SelectLevel(true);
            c->SetObjectScale(c->GetNativeObjectScale());
        }
        else
            c->DespawnOrUnsummon();
    }
    evt.spawnedCreatures.clear();
}
void TerrorZonesMgr::DespawnEventGameObjects(ActiveEvent& evt)
{
    if (evt.spawnedGameObjects.empty())
        return;
    Map* map = sMapMgr->FindBaseNonInstanceMap(evt.mapId);
    for (ObjectGuid const& guid : evt.spawnedGameObjects)
    {
        if (!map)
            continue;
        GameObject* go = map->GetGameObject(guid);
        if (!go)
            continue;  // already gathered / cleaned up
        go->Delete();
    }
    evt.spawnedGameObjects.clear();
}

}  // namespace mod_terror_zones
