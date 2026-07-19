// Slice 6/8 — event content loading: DB-backed boss/node-surge
// definitions and the event-boss bonus loot pool.
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
    std::vector<uint32> ParseIdCsv(std::string const& s)
    {
        std::vector<uint32> out;
        std::stringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            while (!tok.empty()
                   && std::isspace(static_cast<unsigned char>(tok.front())))
                tok.erase(tok.begin());
            while (!tok.empty()
                   && std::isspace(static_cast<unsigned char>(tok.back())))
                tok.pop_back();
            if (tok.empty())
                continue;
            long v = std::strtol(tok.c_str(), nullptr, 10);
            if (v > 0)
                out.push_back(static_cast<uint32>(v));
        }
        return out;
    }
}
void TerrorZonesMgr::LoadEventContent()
{
    _eventBossDefs.clear();
    _eventNodeSurgeDefs.clear();

    QueryResult bossRows = WorldDatabase.Query(
        "SELECT id, zone_id, level_min, level_max, creature_template_id, "
        "anchor_map, anchor_x, anchor_y, anchor_z, anchor_o, display_name, "
        "weight, enabled, tracker_quest_id FROM terror_zones_event_bosses");
    uint32 bossLoaded = 0, bossSkipped = 0, bossNativeMatched = 0;
    if (bossRows)
    {
        do
        {
            Field* f = bossRows->Fetch();
            EventBossDef d;
            d.id                 = f[0].Get<uint32>();
            d.zoneId             = f[1].Get<uint32>();
            d.levelMin           = f[2].Get<uint8>();
            d.levelMax           = f[3].Get<uint8>();
            d.creatureTemplateId = f[4].Get<uint32>();
            d.anchorMap          = f[5].Get<uint32>();
            d.anchorX            = f[6].Get<float>();
            d.anchorY            = f[7].Get<float>();
            d.anchorZ            = f[8].Get<float>();
            d.anchorO            = f[9].Get<float>();
            d.displayName        = f[10].Get<std::string>();
            d.weight             = f[11].Get<uint32>();
            d.enabled            = f[12].Get<uint8>() != 0;
            d.trackerQuestId     = f[13].Get<uint32>();
            if (!d.enabled)
            {
                ++bossSkipped;
                continue;
            }
            if (!sObjectMgr->GetCreatureTemplate(d.creatureTemplateId))
            {
                LOG_WARN("module",
                         "mod-terror-zones: event boss def id={} references "
                         "missing creature_template entry {}, skipping.",
                         d.id, d.creatureTemplateId);
                ++bossSkipped;
                continue;
            }
            // Anti-duplication fix: these anchors are deliberately curated
            // to sit on top of an already-existing native spawn of the
            // same creature (see the migration's own curation comment) —
            // find that native spawn's DB guid here, once, so
            // SpawnWorldBoss can possess it instead of summoning a
            // duplicate standing next to it. Nearest match within a tight
            // tolerance only; anything farther is treated as "no native
            // counterpart" and falls back to the TempSummon path.
            {
                QueryResult nativeRow = WorldDatabase.Query(
                    "SELECT guid, position_x, position_y, position_z "
                    "FROM creature WHERE id = {} AND map = {} ORDER BY "
                    "POW(position_x-({}),2)+POW(position_y-({}),2)"
                    "+POW(position_z-({}),2) ASC LIMIT 1",
                    d.creatureTemplateId, d.anchorMap,
                    d.anchorX, d.anchorY, d.anchorZ);
                if (nativeRow)
                {
                    Field* nf = nativeRow->Fetch();
                    uint32 guid = nf[0].Get<uint32>();
                    float  nx   = nf[1].Get<float>();
                    float  ny   = nf[2].Get<float>();
                    float  nz   = nf[3].Get<float>();
                    float  dx = nx - d.anchorX, dy = ny - d.anchorY,
                           dz = nz - d.anchorZ;
                    float  dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= 3.0f)
                    {
                        d.nativeSpawnGuid = guid;
                        ++bossNativeMatched;
                        LOG_INFO("module",
                                 "mod-terror-zones: event boss def id={} "
                                 "matched native spawn guid={} (dist={:.2f}) "
                                 "— will possess instead of summoning a "
                                 "duplicate.",
                                 d.id, guid, dist);
                    }
                    else
                        LOG_INFO("module",
                                 "mod-terror-zones: event boss def id={} — "
                                 "nearest native spawn is {:.1f} units away "
                                 "(> 3.0 tolerance); no native match, will "
                                 "summon a temp copy.",
                                 d.id, dist);
                }
                else
                    LOG_INFO("module",
                             "mod-terror-zones: event boss def id={} — no "
                             "native creature spawn found for template {} "
                             "on map {}; will summon a temp copy.",
                             d.id, d.creatureTemplateId, d.anchorMap);
            }
            _eventBossDefs.push_back(std::move(d));
            ++bossLoaded;
        } while (bossRows->NextRow());
    }

    QueryResult nodeRows = WorldDatabase.Query(
        "SELECT id, zone_id, level_min, level_max, anchor_map, anchor_x, "
        "anchor_y, anchor_z, radius, node_gameobject_ids, node_count, "
        "display_name, weight, enabled FROM terror_zones_event_node_surges");
    uint32 nodeLoaded = 0, nodeSkipped = 0;
    if (nodeRows)
    {
        do
        {
            Field* f = nodeRows->Fetch();
            EventNodeSurgeDef d;
            d.id          = f[0].Get<uint32>();
            d.zoneId      = f[1].Get<uint32>();
            d.levelMin    = f[2].Get<uint8>();
            d.levelMax    = f[3].Get<uint8>();
            d.anchorMap   = f[4].Get<uint32>();
            d.anchorX     = f[5].Get<float>();
            d.anchorY     = f[6].Get<float>();
            d.anchorZ     = f[7].Get<float>();
            d.radius      = f[8].Get<float>();
            std::string csv = f[9].Get<std::string>();
            d.nodeCount   = f[10].Get<uint32>();
            d.displayName = f[11].Get<std::string>();
            d.weight      = f[12].Get<uint32>();
            d.enabled     = f[13].Get<uint8>() != 0;
            if (!d.enabled)
            {
                ++nodeSkipped;
                continue;
            }
            std::vector<uint32> ids = ParseIdCsv(csv);
            // Validate each ID against gameobject_template; drop invalid
            // ones with a warn line so curation typos don't go silent.
            for (auto it = ids.begin(); it != ids.end(); )
            {
                if (!sObjectMgr->GetGameObjectTemplate(*it))
                {
                    LOG_WARN("module",
                             "mod-terror-zones: node surge def id={} has "
                             "missing gameobject_template entry {}, skipping.",
                             d.id, *it);
                    it = ids.erase(it);
                }
                else
                    ++it;
            }
            if (ids.empty())
            {
                LOG_WARN("module",
                         "mod-terror-zones: node surge def id={} has no valid "
                         "gameobject entries, skipping row.", d.id);
                ++nodeSkipped;
                continue;
            }
            d.nodeIds = std::move(ids);
            _eventNodeSurgeDefs.push_back(std::move(d));
            ++nodeLoaded;
        } while (nodeRows->NextRow());
    }

    LOG_INFO("module",
             "mod-terror-zones: event content loaded — bosses={} (skipped {}, "
             "{} matched a native spawn to possess, {} will summon a temp "
             "copy), node surges={} (skipped {}).",
             bossLoaded, bossSkipped, bossNativeMatched,
             bossLoaded - bossNativeMatched, nodeLoaded, nodeSkipped);
}
uint32 TerrorZonesMgr::GetEventBossDefCount() const
{
    // Write-once at LoadEventContent; safe to read directly.
    return static_cast<uint32>(_eventBossDefs.size());
}
uint32 TerrorZonesMgr::GetEventNodeSurgeDefCount() const
{
    return static_cast<uint32>(_eventNodeSurgeDefs.size());
}

void TerrorZonesMgr::LoadEventBossLootPool()
{
    _eventBossLootPool.clear();

    QueryResult rows = WorldDatabase.Query(
        "SELECT id, level_min, level_max, guaranteed_blue_item_id, "
        "purple_item_id, purple_chance, gold_min_copper, gold_max_copper "
        "FROM terror_zones_event_bosses_loot_pool WHERE enabled = 1 "
        "ORDER BY level_min ASC");
    uint32 loaded = 0, skipped = 0;
    if (rows)
    {
        do
        {
            Field* f = rows->Fetch();
            LootPoolEntry e;
            e.id               = f[0].Get<uint32>();
            e.levelMin         = f[1].Get<uint8>();
            e.levelMax         = f[2].Get<uint8>();
            e.guaranteedBlueId = f[3].Get<uint32>();
            e.purpleItemId     = f[4].Get<uint32>();
            e.purpleChance     = f[5].Get<float>();
            e.goldMinCopper    = f[6].Get<uint32>();
            e.goldMaxCopper    = f[7].Get<uint32>();
            if (e.guaranteedBlueId != 0
                && !sObjectMgr->GetItemTemplate(e.guaranteedBlueId))
            {
                LOG_WARN("module",
                         "mod-terror-zones: loot pool id={} band=[{}-{}] "
                         "guaranteed_blue_item_id={} missing from "
                         "item_template, skipping row.",
                         e.id, static_cast<uint32>(e.levelMin),
                         static_cast<uint32>(e.levelMax),
                         e.guaranteedBlueId);
                ++skipped;
                continue;
            }
            if (e.purpleItemId != 0
                && !sObjectMgr->GetItemTemplate(e.purpleItemId))
            {
                LOG_WARN("module",
                         "mod-terror-zones: loot pool id={} band=[{}-{}] "
                         "purple_item_id={} missing; will skip purple roll.",
                         e.id, static_cast<uint32>(e.levelMin),
                         static_cast<uint32>(e.levelMax),
                         e.purpleItemId);
                e.purpleItemId = 0;
            }
            _eventBossLootPool.push_back(e);
            ++loaded;
        } while (rows->NextRow());
    }
    LOG_INFO("module",
             "mod-terror-zones: event-boss loot pool loaded — bands={} "
             "(skipped {}).",
             loaded, skipped);
}
TerrorZonesMgr::LootPoolEntry const* TerrorZonesMgr::FindEventBossLootBand(
    uint8 scaledLevel) const
{
    // `_eventBossLootPool` is write-once at LoadEventBossLootPool.
    // First-match wins; Pass-1 curation has no overlaps, so linear
    // scan is O(bands) ≤ ~10.
    for (LootPoolEntry const& e : _eventBossLootPool)
        if (scaledLevel >= e.levelMin && scaledLevel <= e.levelMax)
            return &e;
    return nullptr;
}

}  // namespace mod_terror_zones
