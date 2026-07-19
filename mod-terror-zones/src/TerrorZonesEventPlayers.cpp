// Slice 6/7 — player-facing event surfaces: world-boss tracker
// aura/quest sweep and zone-scoped chat broadcasts.
#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"

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
    // Reserved quest ID range carrying Slice 6 worldmap POIs. See
    // migration rev_1777084800000000004.sql — IDs 90100-90120 are
    // one-per-zone dummy quests whose quest_poi rows point at each
    // boss's anchor. Kept in sync with the migration; if the range
    // grows, bump the max.
    constexpr uint32 TRACKER_QUEST_ID_MIN = 90100;
    constexpr uint32 TRACKER_QUEST_ID_MAX = 90120;

    bool IsTrackerQuestId(uint32 qid)
    {
        return qid >= TRACKER_QUEST_ID_MIN && qid <= TRACKER_QUEST_ID_MAX;
    }

    // Clean-revoke a tracker quest from a player's log. Mirrors the
    // CMSG_QUESTLOG_REMOVE_QUEST handler flow minus the achievement
    // credit + side-effects — our dummy quest has no items, timers,
    // or PVP flags, so the happy path is: clear the slot + erase
    // m_QuestStatus + push the update. Safe even if the quest isn't
    // actually in the log (early-out on slot lookup).
    void RevokeTrackerQuest(Player* p, uint32 questId)
    {
        if (!p) return;
        uint16 slot = p->FindQuestSlot(questId);
        if (slot >= MAX_QUEST_LOG_SIZE)
            return;
        p->RemoveActiveQuest(questId, /*update*/ true);
        p->SetQuestSlot(slot, 0);
    }
}

void TerrorZonesMgr::MarkWorldBossForPlayers()
{
    if (!_eventsEnabled)
        return;

    // World-thread-only — `_activeEvents` is the writer-side mutable
    // copy; we're already on the same thread that mutates it, so a
    // direct read is safe.
    struct BossRef {
        uint32 zoneId;
        uint32 mapId;
        ObjectGuid guid;
        uint32 trackerQuestId;
    };
    std::vector<BossRef> bosses;
    for (ActiveEvent const& e : _activeEvents)
    {
        if (e.state != EVENT_STATE_ACTIVE) continue;
        if (e.type != EVENT_WORLD_BOSS) continue;
        if (e.spawnedCreatures.empty()) continue;
        bosses.push_back({e.zoneId, e.mapId,
                          e.spawnedCreatures[0],
                          e.trackerQuestId});
    }

    // Build (zoneId → trackerQuestId) map of zones with a live
    // event. Used below for the revoke sweep — a player holding a
    // tracker quest whose zone isn't in this map should have it
    // removed (either they left the zone, or the event ended).
    std::unordered_map<uint32, uint32> activeZoneTracker;
    activeZoneTracker.reserve(bosses.size());
    for (BossRef const& b : bosses)
        if (b.trackerQuestId != 0)
            activeZoneTracker[b.zoneId] = b.trackerQuestId;

    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();

    // Pass 1: aura refresh + quest grant. Only for players in a
    // zone with a live world-boss event.
    for (BossRef const& b : bosses)
    {
        Map* map = sMapMgr->FindBaseNonInstanceMap(b.mapId);
        if (!map) continue;
        Creature* boss = map->GetCreature(b.guid);
        if (!boss || !boss->IsAlive() || !boss->IsInWorld())
            continue;

        Quest const* trackerQuest = (b.trackerQuestId != 0)
            ? sObjectMgr->GetQuestTemplate(b.trackerQuestId)
            : nullptr;

        for (auto const& kv : sessions)
        {
            Player* p = RealPlayerFromSession(kv.second);
            if (!p)
                continue;
            if (p->GetZoneId() != b.zoneId)
                continue;

            // Hunter's-Mark-style stalker aura → minimap dot for
            // this specific player.
            if (_eventBossTrackerSpellId != 0
                && !boss->HasAuraTypeWithCaster(
                        SPELL_AURA_MOD_STALKED, p->GetGUID()))
            {
                p->AddAura(_eventBossTrackerSpellId, boss);
            }

            // Worldmap / minimap POI → grant the tracker quest if
            // the player doesn't have it already AND has a free
            // quest-log slot. We deliberately bypass
            // `CanTakeQuest` prerequisite checks — the quest is a
            // hidden helper, not a real quest.
            if (trackerQuest
                && p->FindQuestSlot(b.trackerQuestId)
                       >= MAX_QUEST_LOG_SIZE
                && p->SatisfyQuestLog(/*msg*/ false))
            {
                p->AddQuest(trackerQuest, nullptr);
                // Without this the client never receives the quest's text and
                // shows "Missing Header" in the quest log for this entry. The
                // quest LOG window (L) is populated by SMSG_QUEST_QUERY_RESPONSE
                // (SendQuestQueryResponse) -- the same packet
                // HandleQuestQueryOpcode sends in response to CMSG_QUEST_QUERY --
                // NOT SMSG_QUESTGIVER_QUEST_DETAILS (SendQuestGiverQuestDetails),
                // which is the live NPC accept/decline offer dialog and doesn't
                // touch the log's cached quest data.
                p->PlayerTalkClass->SendQuestQueryResponse(trackerQuest);
            }
        }
    }

    // Pass 2: revoke-sweep for every online player. A tracker
    // quest in the log without a matching active event in the
    // player's current zone gets removed. Handles event-end,
    // zone-leave, and logins-into-a-stale-quest in one loop.
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;

        uint32 playerZone = p->GetZoneId();
        auto it = activeZoneTracker.find(playerZone);
        uint32 keepQuestId = (it != activeZoneTracker.end())
                               ? it->second : 0;

        for (uint32 qid = TRACKER_QUEST_ID_MIN;
             qid <= TRACKER_QUEST_ID_MAX; ++qid)
        {
            if (qid == keepQuestId)
                continue;
            RevokeTrackerQuest(p, qid);
        }
    }
}
void TerrorZonesMgr::BroadcastZoneLineGated(uint32 zoneId,
                                              std::string const& line,
                                              AnnounceCategory cat)
{
    if (zoneId == 0 || line.empty())
        return;
    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;
        if (p->GetZoneId() != zoneId)
            continue;
        if (!TerrorZonesPlayerPrefsMgr::Instance().IsCategoryEnabledFor(p, cat))
            continue;
        ChatHandler(p->GetSession()).PSendSysMessage("{}", line);
    }
}
void TerrorZonesMgr::SendEventEndingCountdown(ActiveEvent const& evt,
                                                uint32 remainingSec)
{
    if (evt.zoneId == 0)
        return;
    char buf[256];
    if (remainingSec >= 60)
    {
        uint32 mins = (remainingSec + 30) / 60;
        std::snprintf(buf, sizeof(buf),
            "|cffff8040%s prepares to withdraw — %u minute%s until "
            "the event ends.|r",
            evt.displayName.c_str(), mins, (mins == 1 ? "" : "s"));
    }
    else
    {
        std::snprintf(buf, sizeof(buf),
            "|cffff8040%s prepares to withdraw — %u seconds until "
            "the event ends.|r",
            evt.displayName.c_str(), remainingSec);
    }
    BroadcastZoneLineGated(evt.zoneId, buf, ANNOUNCE_EVENT_ENDING);
}
void TerrorZonesMgr::BroadcastZoneLine(uint32 zoneId,
                                       std::string const& line)
{
    if (zoneId == 0 || line.empty())
        return;
    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;
        if (p->GetZoneId() != zoneId)
            continue;
        ChatHandler(p->GetSession()).PSendSysMessage("{}", line);
    }
}

}  // namespace mod_terror_zones
