// Slice 6 — event scheduling + lifecycle tick: ScheduleEvents writes
// PENDING rows at rotation tick, TickEvents drives PENDING/ACTIVE/EXPIRED
// transitions every 1Hz OnUpdate, plus GM-triggered fire/end and DB persistence.
#include "TerrorZonesCombatMgr.h"
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

namespace
{
    // Deterministic seed for the event RNG at rotation tick. Same
    // (tickAt, slotIndex) always produces the same event choice, which
    // makes the restart-resume path read identical events even if the
    // DB rows ever go missing.
    uint64 EventRotationSeed(uint64 tickAt, uint32 slotIndex)
    {
        uint64 h = tickAt ^ 0x9E3779B97F4A7C15ULL;
        h ^= (static_cast<uint64>(slotIndex) + 1) * 0xBF58476D1CE4E5B9ULL;
        h ^= (h >> 27) * 0x94D049BB133111EBULL;
        return h ^ (h >> 31);
    }
}

void TerrorZonesMgr::ScheduleEvents(uint64 tickAt,
                                    std::vector<ActiveSlot> const& slots)
{
    if (!_eventsEnabled || slots.empty())
        return;

    // World-thread-only state — read `_eventCfg`, `_eventBossDefs`,
    // and `_eventNodeSurgeDefs` directly. `ComputeTargetLevel` reads
    // the rotation snapshot, so it's safe to call inline.
    EventScheduleConfig const& cfg = _eventCfg;
    std::vector<EventBossDef> const& bossDefs = _eventBossDefs;
    std::vector<EventNodeSurgeDef> const& nodeDefs = _eventNodeSurgeDefs;

    std::vector<uint8> slotScaledLevel(slots.size(), 0);
    for (size_t i = 0; i < slots.size(); ++i)
    {
        uint8 lvl = TerrorZonesCombatMgr::Instance().ComputeTargetLevel(slots[i].zoneId);
        slotScaledLevel[i] = (lvl == 0) ? 1 : lvl;
    }

    // A GM-forced tick advances to the *next* aligned boundary, so tickAt
    // can be in the future relative to wall-clock. The guaranteed boss is
    // meant to be up the moment the rotation goes live, so clamp its start
    // to "now" — a normal boundary tick has now ≈ tickAt, so this is a
    // no-op there.
    uint64 nowSec = static_cast<uint64>(::time(nullptr));

    std::vector<ActiveEvent> newEvents;

    for (size_t i = 0; i < slots.size(); ++i)
    {
        ActiveSlot const& slot = slots[i];
        uint8 scaledLevel = slotScaledLevel[i];
        StdRng rng(EventRotationSeed(tickAt,
                                     static_cast<uint32>(slot.slotIndex)));

        auto pickDef = [&](EventType type, uint32 zoneId,
                           uint32& outDefId, uint32& outMapId,
                           float& outX, float& outY, float& outZ,
                           std::string& outName) -> bool
        {
            if (type == EVENT_WORLD_BOSS)
            {
                std::vector<size_t> cands;
                uint64 total = 0;
                for (size_t k = 0; k < bossDefs.size(); ++k)
                {
                    EventBossDef const& d = bossDefs[k];
                    if (d.zoneId != zoneId) continue;
                    if (scaledLevel < d.levelMin) continue;
                    if (d.levelMax > 0 && scaledLevel > d.levelMax) continue;
                    if (d.weight == 0) continue;
                    cands.push_back(k);
                    total += d.weight;
                }
                // AlwaysSpawn fallback: an empty zone at tick time resolves
                // scaledLevel to 1 (no real players present), and a zone
                // whose curated boss sits outside that band would otherwise
                // veto the guaranteed boss. When AlwaysSpawn is on, re-collect
                // the zone's enabled bosses ignoring the level window — the
                // boss still scales to the right level at spawn via
                // ComputeEventBossApex. Non-AlwaysSpawn behavior is unchanged.
                if ((cands.empty() || total == 0) && _eventBossAlwaysSpawn)
                {
                    total = 0;
                    for (size_t k = 0; k < bossDefs.size(); ++k)
                    {
                        EventBossDef const& d = bossDefs[k];
                        if (d.zoneId != zoneId || d.weight == 0)
                            continue;
                        cands.push_back(k);
                        total += d.weight;
                    }
                }
                if (cands.empty() || total == 0)
                    return false;
                uint32 roll = rng.NextUInt(
                    static_cast<uint32>(std::min<uint64>(total, 1'000'000)));
                uint64 acc = 0;
                size_t picked = cands.front();
                for (size_t c : cands)
                {
                    acc += bossDefs[c].weight;
                    if (roll < acc)
                    {
                        picked = c;
                        break;
                    }
                }
                EventBossDef const& d = bossDefs[picked];
                outDefId = d.id;
                outMapId = d.anchorMap;
                outX = d.anchorX;
                outY = d.anchorY;
                outZ = d.anchorZ;
                outName = d.displayName.empty()
                    ? std::string("World Boss") : d.displayName;
                return true;
            }
            if (type == EVENT_RARE_NODE_SURGE)
            {
                std::vector<size_t> cands;
                uint64 total = 0;
                for (size_t k = 0; k < nodeDefs.size(); ++k)
                {
                    EventNodeSurgeDef const& d = nodeDefs[k];
                    if (d.zoneId != zoneId) continue;
                    if (scaledLevel < d.levelMin) continue;
                    if (d.levelMax > 0 && scaledLevel > d.levelMax) continue;
                    if (d.weight == 0) continue;
                    cands.push_back(k);
                    total += d.weight;
                }
                if (cands.empty() || total == 0)
                    return false;
                uint32 roll = rng.NextUInt(
                    static_cast<uint32>(std::min<uint64>(total, 1'000'000)));
                uint64 acc = 0;
                size_t picked = cands.front();
                for (size_t c : cands)
                {
                    acc += nodeDefs[c].weight;
                    if (roll < acc)
                    {
                        picked = c;
                        break;
                    }
                }
                EventNodeSurgeDef const& d = nodeDefs[picked];
                outDefId = d.id;
                outMapId = d.anchorMap;
                outX = d.anchorX;
                outY = d.anchorY;
                outZ = d.anchorZ;
                outName = d.displayName.empty()
                    ? std::string("a rare surge") : d.displayName;
                return true;
            }
            return false;
        };

        uint32 offsets[2] = { cfg.firstOffsetSec, cfg.secondOffsetSec };
        uint32 slotEventId = 0;

        // Guaranteed full-rotation world boss
        // (TerrorZones.Events.WorldBoss.AlwaysSpawn). Bypasses the
        // FireChance gate and the SelectEventType weighted draw: every
        // empowered slot gets a world boss that is up at rotation start
        // and lasts the whole rotation window. The optional second
        // event still rolls below (for non-boss variety — see the
        // EVENT_WORLD_BOSS exclusion at the roll site). When the zone has
        // no curated boss content, we log and fall back to the normal
        // probabilistic
        // path so the slot can still get a node surge.
        bool bossScheduled = false;
        if (_eventBossAlwaysSpawn)
        {
            uint32 bDefId = 0, bMapId = 0;
            float bx = 0.0f, by = 0.0f, bz = 0.0f;
            std::string bName;
            if (pickDef(EVENT_WORLD_BOSS, slot.zoneId, bDefId, bMapId,
                        bx, by, bz, bName))
            {
                uint32 trackerQuestId = 0;
                for (EventBossDef const& d : bossDefs)
                    if (d.id == bDefId)
                    {
                        trackerQuestId = d.trackerQuestId;
                        break;
                    }

                ActiveEvent evt{};
                evt.tickAt         = tickAt;
                evt.slotIndex      = slot.slotIndex;
                evt.eventId        = slotEventId++;
                evt.type           = EVENT_WORLD_BOSS;
                evt.state          = EVENT_STATE_PENDING;
                evt.startsAt       = std::min<uint64>(tickAt, nowSec);  // up now
                evt.endsAt         = tickAt + _intervalSec;  // full rotation window
                evt.zoneId         = slot.zoneId;
                evt.mapId          = bMapId;
                evt.definitionId   = bDefId;
                evt.anchorX        = bx;
                evt.anchorY        = by;
                evt.anchorZ        = bz;
                evt.displayName    = std::move(bName);
                evt.trackerQuestId = trackerQuestId;
                newEvents.push_back(std::move(evt));
                bossScheduled = true;
            }
            else
            {
                LOG_WARN("module",
                         "mod-terror-zones: AlwaysSpawn world boss — no "
                         "curated boss content for zone={} (scaledLevel={}, "
                         "tick_at={}, slot={}); no boss this rotation.",
                         slot.zoneId, scaledLevel, tickAt, slot.slotIndex);
            }
        }

        // First-event coin flip — skipped when the guaranteed boss
        // already filled the first event slot (the optional second
        // event still rolls below).
        if (!bossScheduled && !ShouldFireSecondEvent(cfg.fireChance, rng))
            continue;

        uint32 startEvent = bossScheduled ? 1u : 0u;
        for (uint32 e = startEvent; e < 2; ++e)
        {
            if (e == 1 && !ShouldFireSecondEvent(cfg.secondChance, rng))
                break;

            // The guaranteed boss above already covers EVENT_WORLD_BOSS for
            // this slot — exclude it from this roll's candidate pool so the
            // second event can't independently re-pick it and spawn a
            // duplicate boss in the same zone. Other types (e.g. node
            // surge) still roll normally.
            EventScheduleConfig rollCfg = cfg;
            if (bossScheduled)
                rollCfg.typeEnabled[EVENT_WORLD_BOSS] = false;

            EventType type = SelectEventType(rollCfg, rng);
            if (type == EVENT_NONE)
                break;

            uint32 defId = 0;
            uint32 mapId = 0;
            float ax = 0.0f, ay = 0.0f, az = 0.0f;
            std::string name;
            if (!pickDef(type, slot.zoneId, defId, mapId, ax, ay, az, name))
            {
                if (_debug)
                    LOG_INFO("module",
                             "mod-terror-zones: event schedule skipped — "
                             "no {} content for zone={} (tick_at={}, slot={})",
                             EventTypeDisplayName(type),
                             slot.zoneId, tickAt, slot.slotIndex);
                continue;
            }

            uint32 trackerQuestId = 0;
            if (type == EVENT_WORLD_BOSS)
                for (EventBossDef const& d : bossDefs)
                    if (d.id == defId) { trackerQuestId = d.trackerQuestId; break; }

            ActiveEvent evt{};
            evt.tickAt         = tickAt;
            evt.slotIndex      = slot.slotIndex;
            evt.eventId        = slotEventId++;
            evt.type           = type;
            evt.state          = EVENT_STATE_PENDING;
            evt.startsAt       = tickAt + offsets[e];
            evt.endsAt         = evt.startsAt + cfg.durationSec;
            evt.zoneId         = slot.zoneId;
            evt.mapId          = mapId;
            evt.definitionId   = defId;
            evt.anchorX        = ax;
            evt.anchorY        = ay;
            evt.anchorZ        = az;
            evt.displayName    = std::move(name);
            evt.trackerQuestId = trackerQuestId;
            newEvents.push_back(std::move(evt));
        }
    }

    if (newEvents.empty())
        return;

    // Persist + register in-memory together.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (ActiveEvent const& e : newEvents)
    {
        trans->Append(
            "INSERT INTO terror_zones_events (tick_at, slot_index, event_id, "
            "event_type, state, starts_at, ends_at, zone_id, map_id, "
            "definition_id, anchor_x, anchor_y, anchor_z, countdown_fired) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, 0) "
            "ON DUPLICATE KEY UPDATE event_type=VALUES(event_type), "
            "state=VALUES(state), starts_at=VALUES(starts_at), "
            "ends_at=VALUES(ends_at), zone_id=VALUES(zone_id), "
            "map_id=VALUES(map_id), definition_id=VALUES(definition_id), "
            "anchor_x=VALUES(anchor_x), anchor_y=VALUES(anchor_y), "
            "anchor_z=VALUES(anchor_z), countdown_fired=0",
            e.tickAt, e.slotIndex, e.eventId,
            static_cast<uint32>(e.type), static_cast<uint32>(e.state),
            e.startsAt, e.endsAt, e.zoneId, e.mapId, e.definitionId,
            e.anchorX, e.anchorY, e.anchorZ);
    }
    CharacterDatabase.CommitTransaction(trans);

    for (ActiveEvent& e : newEvents)
        _activeEvents.push_back(std::move(e));
    PublishEventsSnap();

    LOG_INFO("module",
             "mod-terror-zones: scheduled {} event(s) for tick_at={} "
             "across {} slot(s).",
             static_cast<uint32>(newEvents.size()), tickAt,
             static_cast<uint32>(slots.size()));

    // Incidental housekeeping — drop expired rows past retention.
    PruneExpiredEventDbRows();
}
void TerrorZonesMgr::LoadActiveEvents()
{
    if (!_eventsEnabled)
        return;

    uint64 now = static_cast<uint64>(::time(nullptr));
    QueryResult r = CharacterDatabase.Query(
        "SELECT tick_at, slot_index, event_id, event_type, state, "
        "starts_at, ends_at, zone_id, map_id, definition_id, "
        "anchor_x, anchor_y, anchor_z, countdown_fired "
        "FROM terror_zones_events "
        "WHERE ends_at > {} ORDER BY starts_at ASC", now);
    if (!r)
        return;

    std::vector<ActiveEvent> resumed;
    do
    {
        Field* f = r->Fetch();
        ActiveEvent e{};
        e.tickAt          = f[0].Get<uint64>();
        e.slotIndex       = f[1].Get<uint32>();
        e.eventId         = f[2].Get<uint32>();
        e.type            = static_cast<EventType>(f[3].Get<uint8>());
        e.state           = EVENT_STATE_PENDING;  // re-fire from scratch
        e.startsAt        = f[5].Get<uint64>();
        e.endsAt          = f[6].Get<uint64>();
        e.zoneId          = f[7].Get<uint32>();
        e.mapId           = f[8].Get<uint32>();
        e.definitionId    = f[9].Get<uint32>();
        e.anchorX         = f[10].Get<float>();
        e.anchorY         = f[11].Get<float>();
        e.anchorZ         = f[12].Get<float>();
        e.countdownFired  = f[13].Get<uint8>() != 0;

        // Regenerate display name + trackerQuestId from the content def
        // when possible; falls through to the type label if the def row
        // has been dropped since.
        std::string displayName;
        if (e.type == EVENT_WORLD_BOSS)
        {
            for (EventBossDef const& d : _eventBossDefs)
                if (d.id == e.definitionId)
                {
                    displayName       = d.displayName;
                    e.trackerQuestId  = d.trackerQuestId;
                    break;
                }
        }
        else if (e.type == EVENT_RARE_NODE_SURGE)
        {
            for (EventNodeSurgeDef const& d : _eventNodeSurgeDefs)
                if (d.id == e.definitionId)
                {
                    displayName = d.displayName;
                    break;
                }
        }
        if (displayName.empty())
            displayName = EventTypeDisplayName(e.type);
        e.displayName = std::move(displayName);

        resumed.push_back(std::move(e));
    } while (r->NextRow());

    for (ActiveEvent& e : resumed)
        _activeEvents.push_back(std::move(e));
    PublishEventsSnap();

    LOG_INFO("module",
             "mod-terror-zones: event resume loaded {} active/pending event(s).",
             static_cast<uint32>(resumed.size()));

    // Do NOT fire events inline at InitializeOnStartup time — at this
    // point the continent maps aren't loaded yet, so
    // `sMapMgr->FindBaseNonInstanceMap` returns null, and the
    // `ChatHandler` path inside `BroadcastZoneLine` has been observed
    // to wedge the main loop's session handshake setup (log goes
    // silent after LogsDir config-read). The 1Hz OnUpdate cadence
    // picks up due events within a second of boot completing — well
    // before a player actually reaches the world — so inline firing
    // is unnecessary. See ENGINEERING_NOTES 2026-04-23 entry.
}
void TerrorZonesMgr::TickEvents(uint64 now)
{
    // World-thread-only — `_activeEvents` and `_eventBossSpawnIndex`
    // are touched only from this method, ScheduleEvents,
    // LoadActiveEvents, FireEventNow, and EndActiveEventsInZone, all
    // of which run on the world thread. Read paths (combat hooks,
    // GetEventsSnapshot, TryEventBossDrop) go through the published
    // snapshots, so we can mutate the underlying state freely here.
    // Indices into `_activeEvents`. Keyed by index, not (tickAt, eventId):
    // eventId is reset per slot, so multiple slots share eventId 0 — a
    // key-based match would update the wrong event and leave the real one
    // PENDING to re-fire every tick. FireEvent / EndEvent below don't
    // resize `_activeEvents`, so these indices stay valid until the prune.
    std::vector<size_t> toFire;
    std::vector<size_t> toEnd;
    std::vector<size_t> toPrune;
    std::vector<ActiveEvent> toCountdown;

    for (size_t i = 0; i < _activeEvents.size(); ++i)
    {
        ActiveEvent& e = _activeEvents[i];
        if (e.state == EVENT_STATE_PENDING && now >= e.startsAt)
            toFire.push_back(i);
        else if (e.state == EVENT_STATE_ACTIVE && now >= e.endsAt)
            toEnd.push_back(i);
        else if (e.state == EVENT_STATE_EXPIRED
                 && now >= e.endsAt + 60)
            toPrune.push_back(i);
        if (e.state == EVENT_STATE_ACTIVE
            && ShouldFireEventEndingCountdown(
                now, e.endsAt, _eventEndingLeadSec,
                kAnnounceWindowSec, e.countdownFired))
        {
            e.countdownFired = true;
            toCountdown.push_back(e);
        }
    }

    // Slice 7 — fire any pending event-ending countdown lines.
    // Persist `countdown_fired` so the state survives a restart.
    for (ActiveEvent const& evt : toCountdown)
    {
        uint32 remaining = (evt.endsAt > static_cast<uint64>(::time(nullptr)))
            ? static_cast<uint32>(evt.endsAt
                                  - static_cast<uint64>(::time(nullptr)))
            : 0;
        SendEventEndingCountdown(evt, remaining);
        PersistEventCountdownFired(evt);
    }

    bool spawnIndexChanged = false;
    bool eventsChanged = !toCountdown.empty();

    for (size_t idx : toFire)
    {
        ActiveEvent& e = _activeEvents[idx];
        if (e.state != EVENT_STATE_PENDING)
            continue;  // defensive — state changed since collection
        // FireEvent mutates `e` in place: spawns the boss / surge, sets
        // e.state = ACTIVE, and persists. Each event is its own index, so
        // it fires exactly once and can't re-fire.
        FireEvent(e);
        Tier slotTier = TIER_5;
        if (e.slotIndex < _rotation.slots.size())
            slotTier = _rotation.slots[e.slotIndex].tier;
        if (slotTier == TIER_NONE)
            slotTier = TIER_5;
        for (ObjectGuid const& g : e.spawnedCreatures)
        {
            _eventBossSpawnIndex[g.GetRawValue()] = {e.tickAt, e.eventId};
            _eventBossTierMap[g.GetRawValue()] = slotTier;
            spawnIndexChanged = true;
        }
        eventsChanged = true;
    }
    for (size_t idx : toEnd)
    {
        ActiveEvent& e = _activeEvents[idx];
        if (e.state != EVENT_STATE_ACTIVE)
            continue;  // defensive
        // Erase the spawn-index entries while the guids are still on `e`,
        // then EndEvent despawns + sets e.state = EXPIRED + persists.
        for (ObjectGuid const& g : e.spawnedCreatures)
        {
            _eventBossSpawnIndex.erase(g.GetRawValue());
            _eventBossTierMap.erase(g.GetRawValue());
            spawnIndexChanged = true;
        }
        EndEvent(e);
        e.spawnedCreatures.clear();
        e.spawnedGameObjects.clear();
        eventsChanged = true;
    }
    if (!toPrune.empty())
    {
        // Erase high-to-low so indices stay valid.
        std::sort(toPrune.begin(), toPrune.end(), std::greater<size_t>());
        for (size_t idx : toPrune)
            if (idx < _activeEvents.size())
                _activeEvents.erase(_activeEvents.begin()
                                    + static_cast<std::ptrdiff_t>(idx));
        eventsChanged = true;
    }

    if (spawnIndexChanged)
    {
        PublishEventBossSpawnSnap();
        PublishCombatHot();
    }
    if (eventsChanged)
        PublishEventsSnap();

    // Refresh the per-player stalker aura on every ACTIVE world boss so
    // the boss shows on each viewer's minimap. Cheap at 1Hz — the aura
    // only applies if the player doesn't already have one cast on the
    // boss (duration re-check is free).
    MarkWorldBossForPlayers();
}

void TerrorZonesMgr::PersistEventState(ActiveEvent const& evt)
{
    CharacterDatabase.Execute(
        "UPDATE terror_zones_events SET state = {} "
        "WHERE tick_at = {} AND slot_index = {} AND event_id = {}",
        static_cast<uint32>(evt.state),
        evt.tickAt, evt.slotIndex, evt.eventId);
}
void TerrorZonesMgr::PersistEventCountdownFired(ActiveEvent const& evt)
{
    CharacterDatabase.Execute(
        "UPDATE terror_zones_events SET countdown_fired = 1 "
        "WHERE tick_at = {} AND slot_index = {} AND event_id = {}",
        evt.tickAt, evt.slotIndex, evt.eventId);
}
void TerrorZonesMgr::PruneExpiredEventDbRows()
{
    if (_eventRetentionHours == 0)
        return;
    uint64 now = static_cast<uint64>(::time(nullptr));
    uint64 cutoff = now > static_cast<uint64>(_eventRetentionHours) * 3600
        ? now - static_cast<uint64>(_eventRetentionHours) * 3600
        : 0;
    if (cutoff == 0)
        return;
    CharacterDatabase.Execute(
        "DELETE FROM terror_zones_events WHERE ends_at < {}", cutoff);
}
uint32 TerrorZonesMgr::FireEventNow(Player* gm, EventType type)
{
    if (!gm || type == EVENT_NONE || !_eventsEnabled)
        return 0;

    // Event bosses are tied to the current TZ rotation. If GM is in
    // a rotation zone, use that zone. Otherwise pick the first
    // rotation slot's zone — keeps event bosses inside empowered
    // zones so class drops + tier scaling apply correctly.
    uint32 zoneId = gm->GetZoneId();
    bool gmZoneInRotation = false;
    for (ActiveSlot const& s : _rotation.slots)
    {
        if (s.zoneId == zoneId)
        {
            gmZoneInRotation = true;
            break;
        }
    }
    if (!gmZoneInRotation)
    {
        if (_rotation.slots.empty())
        {
            LOG_WARN("module",
                     "mod-terror-zones: .zones event fire — no active "
                     "rotation; can't tie event boss to a TZ zone.");
            return 0;
        }
        zoneId = _rotation.slots.front().zoneId;
        LOG_INFO("module",
                 "mod-terror-zones: .zones event fire — GM zone {} "
                 "not in rotation; redirecting event to rotation zone {}.",
                 gm->GetZoneId(), zoneId);
    }
    if (zoneId == 0)
        return 0;

    // Resolve a content def for the zone. GM forcing IGNORES the level
    // band — the band filter is for auto-scheduling (pick something
    // sensibly-scaled for the group level). When a GM types .zones
    // event fire, they want the curated definition for this zone
    // regardless of their character level. Zone match + enabled is the
    // only filter here.
    // Content defs are write-once at LoadEventContent.
    uint32 defId = 0, mapId = 0, trackerQuestId = 0;
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    std::string name;
    if (type == EVENT_WORLD_BOSS)
    {
        for (EventBossDef const& d : _eventBossDefs)
            if (d.zoneId == zoneId)
            {
                defId = d.id; mapId = d.anchorMap;
                ax = d.anchorX; ay = d.anchorY; az = d.anchorZ;
                name = d.displayName.empty()
                    ? std::string("World Boss") : d.displayName;
                trackerQuestId = d.trackerQuestId;
                break;
            }
    }
    else if (type == EVENT_RARE_NODE_SURGE)
    {
        for (EventNodeSurgeDef const& d : _eventNodeSurgeDefs)
            if (d.zoneId == zoneId)
            {
                defId = d.id; mapId = d.anchorMap;
                ax = d.anchorX; ay = d.anchorY; az = d.anchorZ;
                name = d.displayName.empty()
                    ? std::string("a rare surge") : d.displayName;
                break;
            }
    }
    if (defId == 0)
    {
        LOG_WARN("module",
                 "mod-terror-zones: .zones event fire — no {} content "
                 "curated for zone {}.",
                 EventTypeDisplayName(type), zoneId);
        return 0;
    }

    uint64 now = static_cast<uint64>(::time(nullptr));
    ActiveEvent evt{};
    // Use current rotation's tickAt when one exists; otherwise the
    // wall-clock boundary. The tuple (tickAt, slotIndex, eventId) just
    // has to be unique — for GM-forced events, (now, 0, auto) is fine.
    evt.tickAt         = now;
    evt.slotIndex      = 0xFFFF;        // distinct band, GM-forced
    evt.eventId        = 0;
    evt.type           = type;
    evt.state          = EVENT_STATE_PENDING;
    evt.startsAt       = now;
    evt.endsAt         = now + _eventCfg.durationSec;
    evt.zoneId         = zoneId;
    evt.mapId          = mapId;
    evt.definitionId   = defId;
    evt.anchorX        = ax;
    evt.anchorY        = ay;
    evt.anchorZ        = az;
    evt.displayName    = std::move(name);
    evt.trackerQuestId = trackerQuestId;

    // Dedupe: if a GM already has an event at tickAt=now / slot=0xFFFF,
    // bump eventId.
    {
        uint32 maxId = 0;
        bool hasMax = false;
        for (ActiveEvent const& e : _activeEvents)
            if (e.tickAt == evt.tickAt && e.slotIndex == evt.slotIndex)
            {
                hasMax = true;
                if (e.eventId >= maxId) maxId = e.eventId + 1;
            }
        if (hasMax)
            evt.eventId = maxId;
        _activeEvents.push_back(evt);
    }
    PublishEventsSnap();

    CharacterDatabase.Execute(
        "INSERT INTO terror_zones_events (tick_at, slot_index, event_id, "
        "event_type, state, starts_at, ends_at, zone_id, map_id, "
        "definition_id, anchor_x, anchor_y, anchor_z, countdown_fired) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, 0) "
        "ON DUPLICATE KEY UPDATE state=VALUES(state), countdown_fired=0",
        evt.tickAt, evt.slotIndex, evt.eventId,
        static_cast<uint32>(evt.type), static_cast<uint32>(evt.state),
        evt.startsAt, evt.endsAt, evt.zoneId, evt.mapId, evt.definitionId,
        evt.anchorX, evt.anchorY, evt.anchorZ);

    // Fire immediately — it's a GM command, instant feedback matters.
    FireEvent(evt);
    bool spawnIndexChanged = false;
    for (ActiveEvent& e : _activeEvents)
        if (e.tickAt == evt.tickAt && e.slotIndex == evt.slotIndex
            && e.eventId == evt.eventId)
        {
            e.state              = evt.state;
            e.spawnedCreatures   = evt.spawnedCreatures;
            e.spawnedGameObjects = evt.spawnedGameObjects;
            // GM-forced events have slotIndex 0xFFFF — fall back to
            // T5 since slot lookup won't resolve. For non-GM-forced
            // events that hit this path, use the slot's tier.
            Tier slotTier = TIER_5;
            if (e.slotIndex < _rotation.slots.size())
                slotTier = _rotation.slots[e.slotIndex].tier;
            if (slotTier == TIER_NONE)
                slotTier = TIER_5;
            for (ObjectGuid const& g : evt.spawnedCreatures)
            {
                _eventBossSpawnIndex[g.GetRawValue()] =
                    {e.tickAt, e.eventId};
                _eventBossTierMap[g.GetRawValue()] = slotTier;
                spawnIndexChanged = true;
            }
            break;
        }
    PublishEventsSnap();
    if (spawnIndexChanged)
    {
        PublishEventBossSpawnSnap();
        PublishCombatHot();
    }

    return evt.eventId + 1;  // 1-based ID in command output
}
uint32 TerrorZonesMgr::EndActiveEventsInZone(uint32 zoneId)
{
    if (zoneId == 0)
        return 0;
    // Keyed by (tickAt, eventId) is NOT enough: eventId resets per
    // slot/zone, so multiple zones in the same tick can share
    // eventId 0 (see TickEvents' index-based rewrite for the same
    // class of bug). Carry zoneId through both downstream matches
    // below so we never grab/mutate a different zone's event.
    std::vector<std::pair<uint64, uint32>> toEnd;
    for (ActiveEvent const& e : _activeEvents)
    {
        if (e.zoneId != zoneId)
            continue;
        if (e.state == EVENT_STATE_EXPIRED)
            continue;
        toEnd.push_back({e.tickAt, e.eventId});
    }
    bool spawnIndexChanged = false;
    for (auto const& key : toEnd)
    {
        ActiveEvent snapshot;
        bool haveEvt = false;
        bool wasPending = false;
        for (ActiveEvent const& e : _activeEvents)
            if (e.tickAt == key.first && e.eventId == key.second
                && e.zoneId == zoneId
                && e.state != EVENT_STATE_EXPIRED)
            {
                snapshot = e;
                wasPending = (e.state == EVENT_STATE_PENDING);
                haveEvt = true;
                break;
            }
        if (!haveEvt)
            continue;
        if (wasPending)
        {
            // PENDING events never spawned — just mark expired.
            snapshot.state = EVENT_STATE_EXPIRED;
            PersistEventState(snapshot);
        }
        else
        {
            EndEvent(snapshot);   // despawns entities
        }
        for (ActiveEvent& e : _activeEvents)
            if (e.tickAt == key.first && e.eventId == key.second
                && e.zoneId == zoneId)
            {
                e.state = snapshot.state;
                for (ObjectGuid const& g : e.spawnedCreatures)
                {
                    _eventBossSpawnIndex.erase(g.GetRawValue());
                    _eventBossTierMap.erase(g.GetRawValue());
                    spawnIndexChanged = true;
                }
                e.spawnedCreatures.clear();
                e.spawnedGameObjects.clear();
                break;
            }
    }
    if (!toEnd.empty())
        PublishEventsSnap();
    if (spawnIndexChanged)
    {
        PublishEventBossSpawnSnap();
        PublishCombatHot();
    }
    return static_cast<uint32>(toEnd.size());
}
std::vector<ActiveEvent> TerrorZonesMgr::GetEventsSnapshot() const
{
    auto snap = std::atomic_load_explicit(&_eventsSnap,
                                           std::memory_order_acquire);
    return snap ? *snap : std::vector<ActiveEvent>{};
}

}  // namespace mod_terror_zones
