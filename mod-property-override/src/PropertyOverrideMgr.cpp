#include "PropertyOverrideMgr.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>

namespace mod_property_override
{

namespace
{
    constexpr uint32 RECONCILE_INTERVAL_MS = 2000;

    uint64 NowSecs()
    {
        return static_cast<uint64>(GameTime::GetGameTime().count());
    }
}

PropertyOverrideMgr& PropertyOverrideMgr::Instance()
{
    static PropertyOverrideMgr instance;
    return instance;
}

void PropertyOverrideMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("PropertyOverride.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("PropertyOverride.Debug", false);
    LOG_INFO("module", "mod-property-override: config loaded (enable={}, debug={}).",
             _enabled, _debug);
}

void PropertyOverrideMgr::StartupCleanup()
{
    if (!_enabled)
        return;

    // Rows orphaned while the module was disabled or by external deletion.
    CharacterDatabase.Execute(
        "DELETE po FROM item_property_override po "
        "LEFT JOIN item_instance ii ON po.item_guid = ii.guid "
        "WHERE ii.guid IS NULL");
    LOG_INFO("module", "mod-property-override: startup orphan sweep queued.");
}

void PropertyOverrideMgr::LoadPlayer(Player* player)
{
    if (!_enabled || !player)
        return;

    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();

    // Join live ownership: the module's owner_guid column is write-time
    // bookkeeping only, so items traded/mailed to this character are picked
    // up here without any transfer hooks.
    uint64 now = NowSecs();

    ItemOverrideMap loaded;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT o.item_guid, o.property, o.value, o.expiry "
            "FROM item_property_override o "
            "JOIN item_instance i ON i.guid = o.item_guid "
            "WHERE i.owner_guid = {}",
            guidLow))
    {
        do
        {
            Field* f = result->Fetch();
            OverrideRow row;
            ObjectGuid::LowType itemGuid = f[0].Get<uint32>();
            row.property = f[1].Get<uint8>();
            row.value    = f[2].Get<int32>();
            row.expiry   = f[3].Get<uint64>();

            if (!IsValidProperty(row.property))
                continue;
            if (row.IsExpired(now))
            {
                CharacterDatabase.Execute(
                    "DELETE FROM item_property_override WHERE item_guid = {} AND property = {}",
                    itemGuid, row.property);
                continue;
            }
            loaded[itemGuid].push_back(row);
        } while (result->NextRow());
    }

    std::vector<PlayerRow> playerRows;
    if (QueryResult presult = CharacterDatabase.Query(
            "SELECT source, property, value, expiry "
            "FROM player_property_override WHERE player_guid = {}",
            guidLow))
    {
        do
        {
            Field* f = presult->Fetch();
            PlayerRow row;
            row.source   = f[0].Get<std::string>();
            row.property = f[1].Get<uint8>();
            row.value    = f[2].Get<int32>();
            row.expiry   = f[3].Get<uint64>();

            if (!IsValidProperty(row.property) || !IsValidSource(row.source))
                continue;
            if (row.IsExpired(now))
            {
                CharacterDatabase.Execute(
                    "DELETE FROM player_property_override "
                    "WHERE player_guid = {} AND source = '{}' AND property = {}",
                    guidLow, row.source, row.property);
                continue;
            }
            playerRows.push_back(std::move(row));
        } while (presult->NextRow());
    }

    if (loaded.empty() && playerRows.empty())
        return;

    PlayerState& state = _players[guidLow];
    state.overrides = std::move(loaded);
    state.applied.clear();
    state.playerRows = std::move(playerRows);
    state.playerApplied.clear();
    state.playerMinExpiry = 0;
    state.reconcileTimerMs = 0;

    if (!state.playerRows.empty())
        PlayerResync(player, state);

    if (_debug)
        LOG_INFO("module", "mod-property-override: loaded overrides for {} items "
                 "and {} player rows of player guid {}.",
                 state.overrides.size(), state.playerRows.size(), guidLow);
}

void PropertyOverrideMgr::UnloadPlayer(ObjectGuid::LowType playerGuid)
{
    // No unapply needed: the Unit object is being destroyed and all stats
    // are recomputed from scratch at next login.
    _players.erase(playerGuid);
}

void PropertyOverrideMgr::Sync(Player* player)
{
    if (!_enabled || !player)
        return;

    auto stateIt = _players.find(player->GetGUID().GetCounter());
    if (stateIt == _players.end())
        return;
    PlayerState& state = stateIt->second;
    if (state.overrides.empty() && state.applied.empty())
        return;

    uint64 now = NowSecs();

    std::vector<ObjectGuid::LowType> equipped;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
            if (!item->IsBroken() && state.overrides.count(itemGuid))
                equipped.push_back(itemGuid);
        }

    SyncActions actions = ComputeSyncActions(state.overrides, state.applied, equipped, now);

    for (ObjectGuid::LowType guid : actions.unapply)
        UnapplyItem(player, state, guid);
    for (ObjectGuid::LowType guid : actions.expired)
        PruneExpiredRows(state, guid, now);
    for (ObjectGuid::LowType guid : actions.apply)
        ApplyItem(player, state, guid, now);
}

void PropertyOverrideMgr::OnPlayerTick(Player* player, uint32 diffMs)
{
    if (!_enabled || !player)
        return;

    auto stateIt = _players.find(player->GetGUID().GetCounter());
    if (stateIt == _players.end())
        return;

    PlayerState& state = stateIt->second;
    state.reconcileTimerMs += diffMs;
    if (state.reconcileTimerMs < RECONCILE_INTERVAL_MS)
        return;
    state.reconcileTimerMs = 0;
    Sync(player);

    // Player-target rows only need attention when one actually expired.
    if (state.playerMinExpiry != 0 && NowSecs() >= state.playerMinExpiry)
    {
        PruneExpiredPlayerRows(player, state, NowSecs());
        PlayerResync(player, state);
    }
}

void PropertyOverrideMgr::PlayerResync(Player* player, PlayerState& state)
{
    for (AppliedRow const& row : state.playerApplied)
        ApplyStat(player, row.property, row.value, false);
    state.playerApplied.clear();
    state.playerMinExpiry = 0;

    uint64 now = NowSecs();
    for (PlayerRow const& row : FilterLivePlayerRows(state.playerRows, now))
    {
        ApplyStat(player, row.property, static_cast<float>(row.value), true);
        state.playerApplied.push_back({ row.property, static_cast<float>(row.value) });
        if (row.expiry != 0 &&
            (state.playerMinExpiry == 0 || row.expiry < state.playerMinExpiry))
            state.playerMinExpiry = row.expiry;
    }

    if (_debug)
        LOG_INFO("module", "mod-property-override: player resync applied {} rows on {}.",
                 state.playerApplied.size(), player->GetName());
}

void PropertyOverrideMgr::PruneExpiredPlayerRows(Player* player, PlayerState& state, uint64 now)
{
    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    for (auto it = state.playerRows.begin(); it != state.playerRows.end();)
    {
        if (it->IsExpired(now))
        {
            CharacterDatabase.Execute(
                "DELETE FROM player_property_override "
                "WHERE player_guid = {} AND source = '{}' AND property = {}",
                guidLow, it->source, it->property);
            it = state.playerRows.erase(it);
        }
        else
            ++it;
    }
}

void PropertyOverrideMgr::HandleItemDestroyed(Player* player, Item* item)
{
    if (!player || !item)
        return;

    auto stateIt = _players.find(player->GetGUID().GetCounter());
    if (stateIt == _players.end())
        return;

    PlayerState& state = stateIt->second;
    ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
    UnapplyItem(player, state, itemGuid);
    state.overrides.erase(itemGuid);
    // DB rows are purged transactionally via GlobalScript::OnItemDelFromDB
    // when the engine deletes the item_instance row.
}

bool PropertyOverrideMgr::AddOverride(Player* owner, Item* item, Property prop,
                                      int32 value, uint32 durationSecs)
{
    if (!_enabled || !owner || !item || !IsValidProperty(static_cast<uint8>(prop)))
        return false;

    uint64 now = NowSecs();
    uint64 expiry = durationSecs ? now + durationSecs : 0;
    ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
    ObjectGuid::LowType ownerGuid = owner->GetGUID().GetCounter();
    uint8 rawProp = static_cast<uint8>(prop);

    PlayerState& state = _players[ownerGuid];

    // Re-apply from fresh rows: take the old snapshot off first, then let
    // Sync put the updated set back on (if the item is equipped).
    UnapplyItem(owner, state, itemGuid);

    auto& rows = state.overrides[itemGuid];
    auto rowIt = std::find_if(rows.begin(), rows.end(),
                              [rawProp](OverrideRow const& r) { return r.property == rawProp; });
    if (rowIt != rows.end())
    {
        rowIt->value = value;
        rowIt->expiry = expiry;
    }
    else
        rows.push_back({ rawProp, value, expiry });

    CharacterDatabase.Execute(
        "INSERT INTO item_property_override (item_guid, owner_guid, property, value, expiry) "
        "VALUES ({}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value), expiry = VALUES(expiry), "
        "owner_guid = VALUES(owner_guid)",
        itemGuid, ownerGuid, rawProp, value, expiry);

    Sync(owner);

    if (_debug)
        LOG_INFO("module", "mod-property-override: set item {} {}={} (expiry {}) "
                 "for player guid {}.", itemGuid, PropertyName(prop), value, expiry, ownerGuid);
    return true;
}

bool PropertyOverrideMgr::ClearOverrides(Player* owner, Item* item)
{
    if (!_enabled || !owner || !item)
        return false;

    ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
    ObjectGuid::LowType ownerGuid = owner->GetGUID().GetCounter();

    bool hadAny = false;
    auto stateIt = _players.find(ownerGuid);
    if (stateIt != _players.end())
    {
        PlayerState& state = stateIt->second;
        UnapplyItem(owner, state, itemGuid);
        hadAny = state.overrides.erase(itemGuid) > 0;
        if (state.overrides.empty() && state.applied.empty())
            _players.erase(stateIt);
    }

    CharacterDatabase.Execute(
        "DELETE FROM item_property_override WHERE item_guid = {}", itemGuid);
    return hadAny;
}

bool PropertyOverrideMgr::SetPlayerOverride(Player* player, std::string_view source,
                                            Property prop, int32 value, uint32 durationSecs)
{
    if (!_enabled || !player || !IsValidSource(source) ||
        !IsValidProperty(static_cast<uint8>(prop)))
        return false;

    uint64 now = NowSecs();
    uint64 expiry = durationSecs ? now + durationSecs : 0;
    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    uint8 rawProp = static_cast<uint8>(prop);
    std::string src(source);

    PlayerState& state = _players[guidLow];

    auto rowIt = std::find_if(state.playerRows.begin(), state.playerRows.end(),
                              [&](PlayerRow const& r)
                              { return r.source == src && r.property == rawProp; });
    if (rowIt != state.playerRows.end())
    {
        rowIt->value = value;
        rowIt->expiry = expiry;
    }
    else
        state.playerRows.push_back({ src, rawProp, value, expiry });

    CharacterDatabase.Execute(
        "INSERT INTO player_property_override (player_guid, source, property, value, expiry) "
        "VALUES ({}, '{}', {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value), expiry = VALUES(expiry)",
        guidLow, src, rawProp, value, expiry);

    PlayerResync(player, state);

    if (_debug)
        LOG_INFO("module", "mod-property-override: set player {} [{}] {}={} (expiry {}).",
                 guidLow, src, PropertyName(prop), value, expiry);
    return true;
}

bool PropertyOverrideMgr::ClearPlayerOverrides(Player* player, std::string_view source)
{
    if (!_enabled || !player || !IsValidSource(source))
        return false;

    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    std::string src(source);

    bool hadAny = false;
    auto stateIt = _players.find(guidLow);
    if (stateIt != _players.end())
    {
        PlayerState& state = stateIt->second;
        size_t before = state.playerRows.size();
        state.playerRows.erase(
            std::remove_if(state.playerRows.begin(), state.playerRows.end(),
                           [&](PlayerRow const& r) { return r.source == src; }),
            state.playerRows.end());
        hadAny = state.playerRows.size() != before;
        PlayerResync(player, state);
        if (state.overrides.empty() && state.applied.empty() &&
            state.playerRows.empty() && state.playerApplied.empty())
            _players.erase(stateIt);
    }

    CharacterDatabase.Execute(
        "DELETE FROM player_property_override WHERE player_guid = {} AND source = '{}'",
        guidLow, src);
    return hadAny;
}

std::vector<PlayerRow> PropertyOverrideMgr::GetPlayerOverrides(Player* player) const
{
    if (!_enabled || !player)
        return {};

    auto stateIt = _players.find(player->GetGUID().GetCounter());
    if (stateIt == _players.end())
        return {};
    return FilterLivePlayerRows(stateIt->second.playerRows, NowSecs());
}

std::vector<OverrideRow> PropertyOverrideMgr::GetActiveOverrides(
    Player* owner, ObjectGuid::LowType itemGuid) const
{
    std::vector<OverrideRow> out;
    if (!_enabled || !owner)
        return out;

    auto stateIt = _players.find(owner->GetGUID().GetCounter());
    if (stateIt == _players.end())
        return out;

    auto itemIt = stateIt->second.overrides.find(itemGuid);
    if (itemIt == stateIt->second.overrides.end())
        return out;

    uint64 now = NowSecs();
    for (OverrideRow const& row : itemIt->second)
        if (!row.IsExpired(now))
            out.push_back(row);
    return out;
}

void PropertyOverrideMgr::SendAddonMessage(Player* player, std::string const& payload) const
{
    if (!player || !player->GetSession())
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&data);
}

void PropertyOverrideMgr::ApplyItem(Player* player, PlayerState& state,
                                    ObjectGuid::LowType itemGuid, uint64 now)
{
    if (state.applied.count(itemGuid))
        return; // exactly-once guard

    auto itemIt = state.overrides.find(itemGuid);
    if (itemIt == state.overrides.end())
        return;

    AppliedItem snapshot;
    for (OverrideRow const& row : itemIt->second)
    {
        if (row.IsExpired(now))
            continue;
        ApplyStat(player, row.property, static_cast<float>(row.value), true);
        snapshot.rows.push_back({ row.property, static_cast<float>(row.value) });
        if (row.expiry != 0 && (snapshot.minExpiry == 0 || row.expiry < snapshot.minExpiry))
            snapshot.minExpiry = row.expiry;
    }

    if (snapshot.rows.empty())
        return;

    if (_debug)
        LOG_INFO("module", "mod-property-override: applied {} rows for item {} on {}.",
                 snapshot.rows.size(), itemGuid, player->GetName());
    state.applied.emplace(itemGuid, std::move(snapshot));
}

void PropertyOverrideMgr::UnapplyItem(Player* player, PlayerState& state,
                                      ObjectGuid::LowType itemGuid)
{
    auto appliedIt = state.applied.find(itemGuid);
    if (appliedIt == state.applied.end())
        return;

    for (AppliedRow const& row : appliedIt->second.rows)
        ApplyStat(player, row.property, row.value, false);

    if (_debug)
        LOG_INFO("module", "mod-property-override: unapplied {} rows for item {} on {}.",
                 appliedIt->second.rows.size(), itemGuid, player->GetName());
    state.applied.erase(appliedIt);
}

void PropertyOverrideMgr::PruneExpiredRows(PlayerState& state,
                                           ObjectGuid::LowType itemGuid, uint64 now)
{
    auto itemIt = state.overrides.find(itemGuid);
    if (itemIt == state.overrides.end())
        return;

    auto& rows = itemIt->second;
    for (auto rowIt = rows.begin(); rowIt != rows.end();)
    {
        if (rowIt->IsExpired(now))
        {
            CharacterDatabase.Execute(
                "DELETE FROM item_property_override WHERE item_guid = {} AND property = {}",
                itemGuid, rowIt->property);
            rowIt = rows.erase(rowIt);
        }
        else
            ++rowIt;
    }

    if (rows.empty())
        state.overrides.erase(itemIt);
}

void PropertyOverrideMgr::ApplyStat(Player* player, uint8 property, float value, bool apply)
{
    // Mirrors Player::_ApplyItemBonuses' ITEM_MOD_* switch, except primary
    // stats and armor/resistances go through TOTAL_VALUE like enchantments
    // (bonus semantics: green numbers, kept out of the create-stat base).
    auto stat = [&](Stats s)
    {
        player->HandleStatFlatModifier(
            static_cast<UnitMods>(UNIT_MOD_STAT_START + s), TOTAL_VALUE, value, apply);
        player->UpdateStatBuffMod(s);
    };
    int32 val = static_cast<int32>(value);

    switch (static_cast<Property>(property))
    {
        case Property::Mana:
            player->HandleStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, value, apply);
            break;
        case Property::Health:
            player->HandleStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, value, apply);
            break;
        case Property::Agility:   stat(STAT_AGILITY);   break;
        case Property::Strength:  stat(STAT_STRENGTH);  break;
        case Property::Intellect: stat(STAT_INTELLECT); break;
        case Property::Spirit:    stat(STAT_SPIRIT);    break;
        case Property::Stamina:   stat(STAT_STAMINA);   break;
        case Property::DefenseRating:     player->ApplyRatingMod(CR_DEFENSE_SKILL, val, apply); break;
        case Property::DodgeRating:       player->ApplyRatingMod(CR_DODGE, val, apply); break;
        case Property::ParryRating:       player->ApplyRatingMod(CR_PARRY, val, apply); break;
        case Property::BlockRating:       player->ApplyRatingMod(CR_BLOCK, val, apply); break;
        case Property::HitMeleeRating:    player->ApplyRatingMod(CR_HIT_MELEE, val, apply); break;
        case Property::HitRangedRating:   player->ApplyRatingMod(CR_HIT_RANGED, val, apply); break;
        case Property::HitSpellRating:    player->ApplyRatingMod(CR_HIT_SPELL, val, apply); break;
        case Property::CritMeleeRating:   player->ApplyRatingMod(CR_CRIT_MELEE, val, apply); break;
        case Property::CritRangedRating:  player->ApplyRatingMod(CR_CRIT_RANGED, val, apply); break;
        case Property::CritSpellRating:   player->ApplyRatingMod(CR_CRIT_SPELL, val, apply); break;
        case Property::HasteMeleeRating:  player->ApplyRatingMod(CR_HASTE_MELEE, val, apply); break;
        case Property::HasteRangedRating: player->ApplyRatingMod(CR_HASTE_RANGED, val, apply); break;
        case Property::HasteSpellRating:  player->ApplyRatingMod(CR_HASTE_SPELL, val, apply); break;
        case Property::HitRating:
            player->ApplyRatingMod(CR_HIT_MELEE, val, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, val, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, val, apply);
            break;
        case Property::CritRating:
            player->ApplyRatingMod(CR_CRIT_MELEE, val, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, val, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, val, apply);
            break;
        case Property::ResilienceRating:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, val, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, val, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, val, apply);
            break;
        case Property::HasteRating:
            player->ApplyRatingMod(CR_HASTE_MELEE, val, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, val, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, val, apply);
            break;
        case Property::ExpertiseRating:   player->ApplyRatingMod(CR_EXPERTISE, val, apply); break;
        case Property::AttackPower:
            player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, value, apply);
            player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, value, apply);
            break;
        case Property::RangedAttackPower:
            player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, value, apply);
            break;
        case Property::ManaRegen:              player->ApplyManaRegenBonus(val, apply); break;
        case Property::ArmorPenetrationRating: player->ApplyRatingMod(CR_ARMOR_PENETRATION, val, apply); break;
        case Property::SpellPower:             player->ApplySpellPowerBonus(val, apply); break;
        case Property::HealthRegen:            player->ApplyHealthRegenBonus(val, apply); break;
        case Property::SpellPenetration:       player->ApplySpellPenetrationBonus(val, apply); break;
        case Property::BlockValue:
            player->HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, value, apply);
            break;
        case Property::Armor:
            player->HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, value, apply);
            break;
        case Property::HolyResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY, TOTAL_VALUE, value, apply);
            break;
        case Property::FireResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE, TOTAL_VALUE, value, apply);
            break;
        case Property::NatureResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, TOTAL_VALUE, value, apply);
            break;
        case Property::FrostResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FROST, TOTAL_VALUE, value, apply);
            break;
        case Property::ShadowResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, TOTAL_VALUE, value, apply);
            break;
        case Property::ArcaneResistance:
            player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, TOTAL_VALUE, value, apply);
            break;
        default:
            break; // unknown ids are filtered at load/parse time
    }
}

} // namespace mod_property_override
