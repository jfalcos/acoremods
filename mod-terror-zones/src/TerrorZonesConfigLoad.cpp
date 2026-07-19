// Slice 1 — config + zone-pool loading at boot/.reload.
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

void TerrorZonesMgr::LoadConfig()
{
    _enabled   = sConfigMgr->GetOption<bool>("TerrorZones.Enable", true);
    _debug     = sConfigMgr->GetOption<bool>("TerrorZones.Debug", false);
    _intervalSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.RotationIntervalSeconds", 3600);
    if (_intervalSec < 60)
        _intervalSec = 60;
    _slotCount = sConfigMgr->GetOption<uint32>("TerrorZones.SlotCount", 1);
    if (_slotCount == 0)
        _slotCount = 1;
    _onePerContinent = sConfigMgr->GetOption<bool>(
        "TerrorZones.Selection.OnePerContinent", true);
    _innkeeperGossipEnable = sConfigMgr->GetOption<bool>(
        "TerrorZones.Innkeeper.Gossip.Enable", true);
    _recencyWindow = sConfigMgr->GetOption<uint32>(
        "TerrorZones.RecencyWindow", 6);
    _recencyMultiplier = sConfigMgr->GetOption<float>(
        "TerrorZones.RecencyDampenMultiplier", 0.1f);
    if (_recencyMultiplier < 0.0)
        _recencyMultiplier = 0.0;
    _levelWindow = sConfigMgr->GetOption<uint32>(
        "TerrorZones.LevelWindow", 5);
    _weightNear    = sConfigMgr->GetOption<uint32>("TerrorZones.Weight.Near", 100);
    _weightOverlap = sConfigMgr->GetOption<uint32>("TerrorZones.Weight.Overlap", 30);
    _weightBelow   = sConfigMgr->GetOption<uint32>("TerrorZones.Weight.Below", 10);
    _weightAbove   = sConfigMgr->GetOption<uint32>("TerrorZones.Weight.Above", 1);
    // `_announceServerWide` also gates whether RunRotation announces a
    // given tick server-wide (see RunRotationContinued) -- the per-player
    // announcement preference subsystem (default state, per-category
    // masks) is fully owned by TerrorZonesPlayerPrefsMgr, which loads its
    // own copy of this and the related TerrorZones.Announce.* keys.
    _announceServerWide = sConfigMgr->GetOption<bool>(
        "TerrorZones.Announce.ServerWide", true);
    _announceStartupTick = sConfigMgr->GetOption<bool>(
        "TerrorZones.Announce.StartupTick", true);
    _startupForceTick = sConfigMgr->GetOption<bool>(
        "TerrorZones.StartupForceTick", false);
    _rotationEndingLeadSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Announce.RotationEndingLeadSec", 300);
    _eventEndingLeadSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Announce.EventEndingLeadSec", 300);

    // Slice 2 scaling config now loads independently in
    // TerrorZonesCombatMgr::LoadConfig (full decomposition).
    _maxPlayerLevel = static_cast<uint8>(std::clamp<uint32>(
        sConfigMgr->GetOption<uint32>("TerrorZones.MaxPlayerLevel", 80),
        1u, 255u));

    _rewardsEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Rewards.Enable", true);
    _xpMultiplier = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.XpMultiplier", 1.5f);
    if (_xpMultiplier < 0.0f)
        _xpMultiplier = 0.0f;
    _goldMultiplier = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldMultiplier", 1.5f);
    if (_goldMultiplier < 0.0f)
        _goldMultiplier = 0.0f;
    _tierBumpChance = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.TierBumpChance", 0.03f);
    if (_tierBumpChance < 0.0f) _tierBumpChance = 0.0f;
    if (_tierBumpChance > 1.0f) _tierBumpChance = 1.0f;
    _tierBumpLevelTolerance = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Rewards.TierBumpLevelTolerance", 5);
    _maxBumpQuality = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Rewards.MaxBumpQuality", 4);
    if (_maxBumpQuality > ITEM_QUALITY_EPIC)
        _maxBumpQuality = ITEM_QUALITY_EPIC;
    _goldLevelRatioExp = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldLevelRatioExp", 2.0f);
    if (_goldLevelRatioExp < 0.0f)
        _goldLevelRatioExp = 0.0f;

    // Slice 10 — effort-anchored gold floor.
    _goldFloorEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Rewards.GoldFloor.Enable", true);
    _goldFloorPerLevelCopper = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldFloor.PerLevelCopper", 0.5f);
    if (_goldFloorPerLevelCopper < 0.0f)
        _goldFloorPerLevelCopper = 0.0f;
    _goldFloorExp = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldFloor.Exp", 2.0f);
    if (_goldFloorExp < 0.0f)
        _goldFloorExp = 0.0f;
    _goldFloorEffortMin = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldFloor.EffortMin", 1.0f);
    if (_goldFloorEffortMin < 0.0f)
        _goldFloorEffortMin = 0.0f;
    _goldFloorEffortMax = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldFloor.EffortMax", 12.0f);
    if (_goldFloorEffortMax < _goldFloorEffortMin)
        _goldFloorEffortMax = _goldFloorEffortMin;
    _goldFloorRefHpPerLevel = sConfigMgr->GetOption<float>(
        "TerrorZones.Rewards.GoldFloor.RefHpPerLevel", 130.0f);
    if (_goldFloorRefHpPerLevel < 1.0f)
        _goldFloorRefHpPerLevel = 1.0f;
    _goldFloorCapCopper = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Rewards.GoldFloor.CapCopper", 500000);

    // Slice 10 Pass 2 contract config now loads independently in
    // TerrorZonesContractMgr::LoadConfig (full decomposition).

    // Teleport-unlock config now loads independently in
    // TerrorZonesTeleportMgr::LoadConfig (full decomposition).

    // --- Slice 4 — flavors ---
    _flavorsEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Flavor.Enable", true);

    _flavorWeights[FLAVOR_BLOODBATH   - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weight.Bloodbath",   100);
    _flavorWeights[FLAVOR_PROSPECTORS - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weight.Prospectors", 100);
    _flavorWeights[FLAVOR_WARLORDS    - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weight.Warlords",    100);
    _flavorWeights[FLAVOR_ARCANE      - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weight.Arcane",      100);
    _flavorWeights[FLAVOR_MERCHANTS   - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weight.Merchants",   100);

    auto readOverlay = [&](Flavor f, char const* key, float def, float& out, bool clampNonNeg = true)
    {
        out = sConfigMgr->GetOption<float>(key, def);
        if (clampNonNeg && out < 0.0f)
            out = 0.0f;
        (void)f;
    };
    readOverlay(FLAVOR_BLOODBATH,   "TerrorZones.Flavor.Overlay.Bloodbath.XpBoost",      1.50f, _flavorXpBoost[FLAVOR_BLOODBATH - 1]);
    readOverlay(FLAVOR_BLOODBATH,   "TerrorZones.Flavor.Overlay.Bloodbath.GoldBoost",    1.00f, _flavorGoldBoost[FLAVOR_BLOODBATH - 1]);
    readOverlay(FLAVOR_BLOODBATH,   "TerrorZones.Flavor.Overlay.Bloodbath.TierBumpAdd",  0.00f, _flavorTierBumpAdd[FLAVOR_BLOODBATH - 1]);
    readOverlay(FLAVOR_PROSPECTORS, "TerrorZones.Flavor.Overlay.Prospectors.XpBoost",    1.00f, _flavorXpBoost[FLAVOR_PROSPECTORS - 1]);
    readOverlay(FLAVOR_PROSPECTORS, "TerrorZones.Flavor.Overlay.Prospectors.GoldBoost",  1.25f, _flavorGoldBoost[FLAVOR_PROSPECTORS - 1]);
    readOverlay(FLAVOR_PROSPECTORS, "TerrorZones.Flavor.Overlay.Prospectors.TierBumpAdd", 0.00f, _flavorTierBumpAdd[FLAVOR_PROSPECTORS - 1]);
    readOverlay(FLAVOR_WARLORDS,    "TerrorZones.Flavor.Overlay.Warlords.XpBoost",       1.00f, _flavorXpBoost[FLAVOR_WARLORDS - 1]);
    readOverlay(FLAVOR_WARLORDS,    "TerrorZones.Flavor.Overlay.Warlords.GoldBoost",     1.00f, _flavorGoldBoost[FLAVOR_WARLORDS - 1]);
    readOverlay(FLAVOR_WARLORDS,    "TerrorZones.Flavor.Overlay.Warlords.TierBumpAdd",   0.05f, _flavorTierBumpAdd[FLAVOR_WARLORDS - 1]);
    readOverlay(FLAVOR_ARCANE,      "TerrorZones.Flavor.Overlay.Arcane.XpBoost",         1.25f, _flavorXpBoost[FLAVOR_ARCANE - 1]);
    readOverlay(FLAVOR_ARCANE,      "TerrorZones.Flavor.Overlay.Arcane.GoldBoost",       1.00f, _flavorGoldBoost[FLAVOR_ARCANE - 1]);
    readOverlay(FLAVOR_ARCANE,      "TerrorZones.Flavor.Overlay.Arcane.TierBumpAdd",     0.00f, _flavorTierBumpAdd[FLAVOR_ARCANE - 1]);
    readOverlay(FLAVOR_MERCHANTS,   "TerrorZones.Flavor.Overlay.Merchants.XpBoost",      1.00f, _flavorXpBoost[FLAVOR_MERCHANTS - 1]);
    readOverlay(FLAVOR_MERCHANTS,   "TerrorZones.Flavor.Overlay.Merchants.GoldBoost",    2.00f, _flavorGoldBoost[FLAVOR_MERCHANTS - 1]);
    readOverlay(FLAVOR_MERCHANTS,   "TerrorZones.Flavor.Overlay.Merchants.TierBumpAdd",  0.00f, _flavorTierBumpAdd[FLAVOR_MERCHANTS - 1]);

    _flavorWeatherOverride = sConfigMgr->GetOption<bool>("TerrorZones.Flavor.WeatherOverride.Enable", true);
    _flavorWeatherState[FLAVOR_BLOODBATH - 1]   = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weather.Bloodbath.State",   90);
    _flavorWeatherGrade[FLAVOR_BLOODBATH - 1]   = sConfigMgr->GetOption<float>( "TerrorZones.Flavor.Weather.Bloodbath.Grade",   0.75f);
    _flavorWeatherState[FLAVOR_PROSPECTORS - 1] = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weather.Prospectors.State", 1);
    _flavorWeatherGrade[FLAVOR_PROSPECTORS - 1] = sConfigMgr->GetOption<float>( "TerrorZones.Flavor.Weather.Prospectors.Grade", 0.40f);
    _flavorWeatherState[FLAVOR_WARLORDS - 1]    = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weather.Warlords.State",    86);
    _flavorWeatherGrade[FLAVOR_WARLORDS - 1]    = sConfigMgr->GetOption<float>( "TerrorZones.Flavor.Weather.Warlords.Grade",    0.85f);
    _flavorWeatherState[FLAVOR_ARCANE - 1]      = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weather.Arcane.State",      1);
    _flavorWeatherGrade[FLAVOR_ARCANE - 1]      = sConfigMgr->GetOption<float>( "TerrorZones.Flavor.Weather.Arcane.Grade",      0.70f);
    _flavorWeatherState[FLAVOR_MERCHANTS - 1]   = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Weather.Merchants.State",   3);
    _flavorWeatherGrade[FLAVOR_MERCHANTS - 1]   = sConfigMgr->GetOption<float>( "TerrorZones.Flavor.Weather.Merchants.Grade",   0.30f);
    for (uint32 i = 0; i < FLAVOR_MAX; ++i)
    {
        if (_flavorWeatherGrade[i] < 0.0f) _flavorWeatherGrade[i] = 0.0f;
        if (_flavorWeatherGrade[i] > 1.0f) _flavorWeatherGrade[i] = 1.0f;
    }

    _flavorGatheringYieldMult = sConfigMgr->GetOption<float>("TerrorZones.Flavor.Gathering.YieldMultiplier", 2.0f);
    if (_flavorGatheringYieldMult < 1.0f)
        _flavorGatheringYieldMult = 1.0f;
    _flavorGatheringBonusChance = sConfigMgr->GetOption<float>("TerrorZones.Flavor.Gathering.BonusRollChance", 0.50f);
    if (_flavorGatheringBonusChance < 0.0f) _flavorGatheringBonusChance = 0.0f;
    if (_flavorGatheringBonusChance > 1.0f) _flavorGatheringBonusChance = 1.0f;

    _flavorUniquesEnabled  = sConfigMgr->GetOption<bool>( "TerrorZones.Flavor.Uniques.Enable", true);
    _flavorUniquesBaseChance = sConfigMgr->GetOption<float>("TerrorZones.Flavor.Uniques.BaseChance", 0.02f);
    if (_flavorUniquesBaseChance < 0.0f) _flavorUniquesBaseChance = 0.0f;
    if (_flavorUniquesBaseChance > 1.0f) _flavorUniquesBaseChance = 1.0f;
    _flavorUniquesMinMobLevel = sConfigMgr->GetOption<uint32>("TerrorZones.Flavor.Uniques.MinMobLevel", 0);

    // Slice 5 tier config (enable, rarity weights, axis-roll table, bias
    // bumps, axis caps) now loads independently in
    // TerrorZonesTierMgr::LoadConfig (full decomposition).

    // --- Slice 6 — dynamic events ---
    _eventsEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.Enable", true);
    _eventBossAlwaysSpawn = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.WorldBoss.AlwaysSpawn", true);

    _eventCfg.fireChance = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.FireChance", 0.60f);
    if (_eventCfg.fireChance < 0.0f) _eventCfg.fireChance = 0.0f;
    if (_eventCfg.fireChance > 1.0f) _eventCfg.fireChance = 1.0f;
    _eventCfg.secondChance = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.SecondChance", 0.20f);
    if (_eventCfg.secondChance < 0.0f) _eventCfg.secondChance = 0.0f;
    if (_eventCfg.secondChance > 1.0f) _eventCfg.secondChance = 1.0f;
    _eventCfg.durationSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.DurationSec", 1200);
    if (_eventCfg.durationSec < 60)
        _eventCfg.durationSec = 60;
    _eventCfg.firstOffsetSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.FirstOffsetSec", 300);
    _eventCfg.secondOffsetSec = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.SecondOffsetSec", 1800);
    _eventRetentionHours = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.RetentionHours", 24);

    for (uint32 i = 0; i <= EVENT_TYPE_MAX; ++i)
    {
        _eventCfg.typeWeights[i] = 0;
        _eventCfg.typeEnabled[i] = false;
    }
    _eventCfg.typeWeights[EVENT_WORLD_BOSS] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.Type.WorldBoss.Weight", 100);
    _eventCfg.typeEnabled[EVENT_WORLD_BOSS] = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.Type.WorldBoss.Enable", true);
    _eventCfg.typeWeights[EVENT_RARE_NODE_SURGE] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.Type.NodeSurge.Weight", 100);
    _eventCfg.typeEnabled[EVENT_RARE_NODE_SURGE] = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.Type.NodeSurge.Enable", true);
    _eventCfg.typeWeights[EVENT_TREASURE_CARAVAN] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.Type.Caravan.Weight", 0);
    _eventCfg.typeEnabled[EVENT_TREASURE_CARAVAN] = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.Type.Caravan.Enable", false);
    _eventCfg.typeWeights[EVENT_CHAMPION_GROUNDS] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.Type.Champion.Weight", 0);
    _eventCfg.typeEnabled[EVENT_CHAMPION_GROUNDS] = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.Type.Champion.Enable", false);

    _eventBossLootBaseChance = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.WorldBoss.LootBaseChance", 1.0f);
    if (_eventBossLootBaseChance < 0.0f) _eventBossLootBaseChance = 0.0f;
    if (_eventBossLootBaseChance > 1.0f) _eventBossLootBaseChance = 1.0f;
    _eventBossScaleMult = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.WorldBoss.ScaleMult", 5.0f);
    if (_eventBossScaleMult < 0.1f) _eventBossScaleMult = 0.1f;
    if (_eventBossScaleMult > 10.0f) _eventBossScaleMult = 10.0f;
    _eventBossBeaconGoId = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.WorldBoss.BeaconGameObjectId", 191763);
    _eventBossTrackerSpellId = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.WorldBoss.TrackerSpellId", 1130);
    _eventNodeSurgeDefaultRadius = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.NodeSurge.DefaultRadius", 40.0f);
    if (_eventNodeSurgeDefaultRadius < 1.0f)
        _eventNodeSurgeDefaultRadius = 1.0f;
    _eventNodeSurgeDefaultCount = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.NodeSurge.DefaultNodeCount", 8);
    if (_eventNodeSurgeDefaultCount == 0)
        _eventNodeSurgeDefaultCount = 1;
    _eventNodeSurgeZIgnore = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.NodeSurge.ZIgnore", 10.0f);
    _eventNodeSurgeBeaconGoId = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Events.NodeSurge.BeaconGameObjectId", 177015);

    // Slice 8 combat-difficulty config (HP/damage mults, tier bonuses,
    // elite density, group HP scaling) now loads independently in
    // TerrorZonesCombatMgr::LoadConfig (full decomposition).

    _eventBossLootPoolEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Events.WorldBoss.LootPool.Enable", true);
    _eventBossLootPurpleMultiplier = sConfigMgr->GetOption<float>(
        "TerrorZones.Events.WorldBoss.LootPool.PurpleMultiplier", 1.0f);
    if (_eventBossLootPurpleMultiplier < 0.0f)
        _eventBossLootPurpleMultiplier = 0.0f;

    // --- Slice 9 Pass 1 — class-drop chance per tier ---
    auto loadDropChance = [&](char const* key, float def) -> float {
        float raw = sConfigMgr->GetOption<float>(key, def);
        if (raw < 0.0f) return 0.0f;
        if (raw > 1.0f) return 1.0f;
        return raw;
    };
    _classDropChance[TIER_1] = loadDropChance(
        "TerrorZones.Items.DropChance.T1", 0.05f);
    _classDropChance[TIER_2] = loadDropChance(
        "TerrorZones.Items.DropChance.T2", 0.10f);
    _classDropChance[TIER_3] = loadDropChance(
        "TerrorZones.Items.DropChance.T3", 0.20f);
    _classDropChance[TIER_4] = loadDropChance(
        "TerrorZones.Items.DropChance.T4", 0.35f);
    _classDropChance[TIER_5] = loadDropChance(
        "TerrorZones.Items.DropChance.T5", 0.60f);

    // Slice 8b elite density + Slice 10 Pass 3 group HP scaling config
    // now load independently in TerrorZonesCombatMgr::LoadConfig too.

    LOG_INFO("module",
             "mod-terror-zones: enabled={}, debug={}, interval={}s, slots={}, "
             "recency(window={}, mult={:.3f}), levelWindow={}, "
             "weights(near={}/overlap={}/below={}/above={}), "
             "announce(server={}, startup={}), startupForce={}",
             _enabled, _debug, _intervalSec, _slotCount,
             _recencyWindow, _recencyMultiplier,
             _levelWindow,
             _weightNear, _weightOverlap, _weightBelow, _weightAbove,
             _announceServerWide, _announceStartupTick,
             _startupForceTick);
    // Scaling boot-log line now lives in TerrorZonesCombatMgr::LoadConfig.
    LOG_INFO("module",
             "mod-terror-zones: rewards enable={} xp={:.2f} gold={:.2f} "
             "tier_bump_chance={:.3f} tier_bump_level_tolerance={} "
             "max_bump_quality={} gold_level_ratio_exp={:.2f}",
             _rewardsEnabled, _xpMultiplier, _goldMultiplier,
             _tierBumpChance, _tierBumpLevelTolerance, _maxBumpQuality,
             _goldLevelRatioExp);
    LOG_INFO("module",
             "mod-terror-zones: flavor enable={} weather={} uniques={} "
             "weights=[bb={},pr={},wa={},ar={},me={}] "
             "overlays=[bb_xp={:.2f},pr_gold={:.2f},wa_tier_add={:.3f},"
             "ar_xp={:.2f},me_gold={:.2f}] "
             "gathering=[yield_mult={:.2f},bonus_chance={:.2f}] "
             "uniques=[base_chance={:.3f},min_mob_level={}]",
             _flavorsEnabled, _flavorWeatherOverride, _flavorUniquesEnabled,
             _flavorWeights[0], _flavorWeights[1], _flavorWeights[2],
             _flavorWeights[3], _flavorWeights[4],
             _flavorXpBoost[FLAVOR_BLOODBATH - 1],
             _flavorGoldBoost[FLAVOR_PROSPECTORS - 1],
             _flavorTierBumpAdd[FLAVOR_WARLORDS - 1],
             _flavorXpBoost[FLAVOR_ARCANE - 1],
             _flavorGoldBoost[FLAVOR_MERCHANTS - 1],
             _flavorGatheringYieldMult, _flavorGatheringBonusChance,
             _flavorUniquesBaseChance, _flavorUniquesMinMobLevel);
    // Tier boot-log line now lives in TerrorZonesTierMgr::LoadConfig.
    LOG_INFO("module",
             "mod-terror-zones: events enable={} fire_chance={:.2f} "
             "second_chance={:.2f} duration={}s offsets=[first={},second={}] "
             "retention={}h types=[boss(w={},en={}), nodes(w={},en={}), "
             "caravan(w={},en={}), champion(w={},en={})] "
             "world_boss=[scale={:.2f}x,beacon_go={},tracker_spell={}] "
             "node_surge=[default_radius={:.1f},default_count={},z_ignore={:.1f},"
             "beacon_go={}]",
             _eventsEnabled,
             _eventCfg.fireChance, _eventCfg.secondChance,
             _eventCfg.durationSec,
             _eventCfg.firstOffsetSec, _eventCfg.secondOffsetSec,
             _eventRetentionHours,
             _eventCfg.typeWeights[EVENT_WORLD_BOSS],
             _eventCfg.typeEnabled[EVENT_WORLD_BOSS],
             _eventCfg.typeWeights[EVENT_RARE_NODE_SURGE],
             _eventCfg.typeEnabled[EVENT_RARE_NODE_SURGE],
             _eventCfg.typeWeights[EVENT_TREASURE_CARAVAN],
             _eventCfg.typeEnabled[EVENT_TREASURE_CARAVAN],
             _eventCfg.typeWeights[EVENT_CHAMPION_GROUNDS],
             _eventCfg.typeEnabled[EVENT_CHAMPION_GROUNDS],
             _eventBossScaleMult, _eventBossBeaconGoId,
             _eventBossTrackerSpellId,
             _eventNodeSurgeDefaultRadius,
             _eventNodeSurgeDefaultCount,
             _eventNodeSurgeZIgnore,
             _eventNodeSurgeBeaconGoId);
    // Combat-difficulty boot-log line now lives in
    // TerrorZonesCombatMgr::LoadConfig; loot-pool config is still core.
    LOG_INFO("module",
             "mod-terror-zones: event-boss loot_pool enable={} purple_mult={:.2f}",
             _eventBossLootPoolEnabled, _eventBossLootPurpleMultiplier);
    uint8 announceCategoryGlobal =
        TerrorZonesPlayerPrefsMgr::Instance().GetGlobalAnnounceCategoryMask();
    auto bit = [&](AnnounceCategory c) {
        return (announceCategoryGlobal & AnnounceCategoryBit(c)) ? 1 : 0;
    };
    LOG_INFO("module",
             "mod-terror-zones: announce categories=[tick={} r-ending={} "
             "r-end={} z-entry={} z-leave={} e-start={} e-ending={} "
             "e-end={}] leads=[r-ending={}s e-ending={}s]",
             bit(ANNOUNCE_ROTATION_TICK), bit(ANNOUNCE_ROTATION_ENDING),
             bit(ANNOUNCE_ROTATION_END), bit(ANNOUNCE_ZONE_ENTRY),
             bit(ANNOUNCE_ZONE_LEAVE), bit(ANNOUNCE_EVENT_START),
             bit(ANNOUNCE_EVENT_ENDING), bit(ANNOUNCE_EVENT_END),
             _rotationEndingLeadSec, _eventEndingLeadSec);
}
void TerrorZonesMgr::LoadPool()
{
    _pool.clear();
    _poolIndex.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT zone_id, display_name, level_min, level_max, enabled, "
        "tp_map, tp_x, tp_y, tp_z, tp_o "
        "FROM terror_zones_pool");

    if (!result)
    {
        LOG_WARN("module",
                 "mod-terror-zones: terror_zones_pool is empty or missing. "
                 "Slice 1 migration must be applied before this module is useful.");
        return;
    }

    do
    {
        Field* f = result->Fetch();
        PoolEntry e;
        e.zoneId      = f[0].Get<uint32>();
        e.displayName = f[1].Get<std::string>();
        e.levelMin    = f[2].Get<uint16>();
        e.levelMax    = f[3].Get<uint16>();
        e.enabled     = f[4].Get<uint8>() != 0;
        e.tpMap       = f[5].Get<int32>();
        e.tpX         = f[6].Get<float>();
        e.tpY         = f[7].Get<float>();
        e.tpZ         = f[8].Get<float>();
        e.tpO         = f[9].Get<float>();
        // Resolve the zone's continent (map id) from the core DBC so
        // SelectZonesPerContinent can empower one zone per continent.
        if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(e.zoneId))
            e.continent = area->mapid;
        else
        {
            e.continent = 0;
            LOG_WARN("module",
                     "mod-terror-zones: pool zone {} ({}) has no AreaTable "
                     "entry; defaulting continent to 0 (Eastern Kingdoms).",
                     e.zoneId, e.displayName);
        }
        _poolIndex[e.zoneId] = _pool.size();
        _pool.push_back(std::move(e));
    } while (result->NextRow());

    size_t enabled = 0;
    for (auto const& e : _pool) if (e.enabled) ++enabled;
    LOG_INFO("module",
             "mod-terror-zones: loaded {} zones from terror_zones_pool "
             "({} enabled).", _pool.size(), enabled);
}

}  // namespace mod_terror_zones
