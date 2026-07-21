#include "ParagonMgr.h"

#include "ParagonStrings.h"
#include "PropertyOverrideAddonMsg.h"
#include "PropertyOverrideMgr.h"
#include "RewardDispatcher.h"

#include "Chat.h"
#include "Item.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"

namespace mod_paragon
{

namespace
{
    // Parses comma-separated uint32 lists ("489,566").
    void ParseCsvU32(std::unordered_set<uint32>& out, std::string const& csv)
    {
        out.clear();
        uint32 acc = 0;
        bool any = false;
        for (size_t i = 0; i <= csv.size(); ++i)
        {
            if (i == csv.size() || csv[i] == ',' || csv[i] == ' ')
            {
                if (any && acc)
                    out.insert(acc);
                acc = 0;
                any = false;
                continue;
            }
            if (csv[i] >= '0' && csv[i] <= '9')
            {
                acc = acc * 10 + (csv[i] - '0');
                any = true;
            }
        }
    }
}

ParagonMgr& ParagonMgr::Instance()
{
    static ParagonMgr inst;
    return inst;
}

void ParagonMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("Paragon.Enable",
               sConfigMgr->GetOption<bool>("Paragon.Enabled", true));
    _pxPerLevel = sConfigMgr->GetOption<uint32>("Paragon.PXPerLevel", 1670800);
    _loginSplash = sConfigMgr->GetOption<bool>("Paragon.LoginSplash", true);
    _debug = sConfigMgr->GetOption<bool>("Paragon.Debug", false);
    _processBots = sConfigMgr->GetOption<bool>("Paragon.ProcessBots", false);
    _coinItemId = sConfigMgr->GetOption<uint32>("Paragon.CoinItemId", 37711);
    _qmNpcEntry = sConfigMgr->GetOption<uint32>("Paragon.QuartermasterEntry", 96000);
    _minToggleLevel = sConfigMgr->GetOption<uint32>("Paragon.MinToggleLevel", 30);
    _maxAllocLow = sConfigMgr->GetOption<uint32>("Paragon.MaxXPPercent.Low", 30);
    _maxAllocHigh = sConfigMgr->GetOption<uint32>("Paragon.MaxXPPercent.High", 100);
    _allocCapBreakLevel = sConfigMgr->GetOption<uint32>("Paragon.MaxXPPercent.BreakLevel", 60);
    _xpGainScale = sConfigMgr->GetOption<bool>("Paragon.XPGainScale", false);

    ParseCsvU32(_blockedMaps,
                sConfigMgr->GetOption<std::string>("Paragon.BlockedMaps", "489,566"));
    ParseCsvU32(_milestoneLevels,
                sConfigMgr->GetOption<std::string>("Paragon.Broadcast.Milestones", "10,25,50"));

    _perkCfg.maxRanks = sConfigMgr->GetOption<uint32>("Paragon.Perk.MaxRanks", 20);
    _perkCfg.costStepEvery = sConfigMgr->GetOption<uint32>("Paragon.Perk.CostStepEvery", 5);
    _perkMinLevel = sConfigMgr->GetOption<uint32>("Paragon.Perk.MinLevel", 30);

    _itemUpgradeEnabled = sConfigMgr->GetOption<bool>("Paragon.ItemUpgrade.Enable", true);
    _upgradeCfg.budgetPercent =
        sConfigMgr->GetOption<uint32>("Paragon.ItemUpgrade.BudgetPercent", 30) / 100.0f;
    static constexpr std::array<char const*, perks::PERK_SET.size()> VALUE_KEYS =
    {
        "Paragon.Perk.ValuePerRank.Strength",
        "Paragon.Perk.ValuePerRank.Agility",
        "Paragon.Perk.ValuePerRank.Stamina",
        "Paragon.Perk.ValuePerRank.Intellect",
        "Paragon.Perk.ValuePerRank.Spirit",
        "Paragon.Perk.ValuePerRank.AttackPower",
        "Paragon.Perk.ValuePerRank.SpellPower",
    };
    static constexpr std::array<uint32, perks::PERK_SET.size()> VALUE_DEFAULTS =
        { 5, 5, 5, 5, 5, 10, 6 };
    for (size_t i = 0; i < perks::PERK_SET.size(); ++i)
        _perkCfg.valuePerRank[i] =
            sConfigMgr->GetOption<uint32>(VALUE_KEYS[i], VALUE_DEFAULTS[i]);

    LOG_INFO("module",
             "mod-paragon: enable={} pxPerLevel={} coin={} qm={} minLevel={} "
             "allocCaps={}/{} (break {}) scale={} blockedMaps={} milestones={} "
             "perks(maxRanks={} costStep={})",
             _enabled, _pxPerLevel, _coinItemId, _qmNpcEntry, _minToggleLevel,
             _maxAllocLow, _maxAllocHigh, _allocCapBreakLevel, _xpGainScale,
             _blockedMaps.size(), _milestoneLevels.size(),
             _perkCfg.maxRanks, _perkCfg.costStepEvery);
}

bool ParagonMgr::IsMapBlocked(uint32 mapId) const
{
    return _blockedMaps.count(mapId) != 0;
}

uint32 ParagonMgr::MaxAllocPercentFor(uint8 level) const
{
    return level < _allocCapBreakLevel ? _maxAllocLow : _maxAllocHigh;
}

ParagonMgr::AccountState* ParagonMgr::FindAccount(uint32 accountId)
{
    auto it = _accounts.find(accountId);
    return it != _accounts.end() ? &it->second : nullptr;
}

ParagonMgr::AccountState const* ParagonMgr::FindAccount(uint32 accountId) const
{
    auto it = _accounts.find(accountId);
    return it != _accounts.end() ? &it->second : nullptr;
}

void ParagonMgr::LoadPlayer(Player* player)
{
    if (!_enabled || !player || !player->GetSession())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    AccountState& st = _accounts[accountId];
    ++st.chars;
    if (st.chars == 1) // first character of this account: pull from DB
    {
        if (QueryResult qr = CharacterDatabase.Query(
                "SELECT lifetime_px, last_reward_level, paragon_opt_out, "
                "xp_allocation_percent FROM paragon_account_progress "
                "WHERE account = {}", accountId))
        {
            Field* f = qr->Fetch();
            st.lifetimePx = f[0].Get<uint64>();
            st.lastRewardLevel = f[1].Get<uint32>();
            st.optOut = f[2].Get<uint8>() != 0;
            st.allocPercent = f[3].Get<uint32>();
            st.rowExists = true;
        }
    }

    // Per-character perk ranks.
    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    auto& ranks = _perkRanks[guidLow];
    ranks.clear();
    if (QueryResult qr = CharacterDatabase.Query(
            "SELECT property, ranks FROM paragon_perk_ranks WHERE guid = {}", guidLow))
    {
        do
        {
            Field* f = qr->Fetch();
            ranks[f[0].Get<uint8>()] = f[1].Get<uint32>();
        } while (qr->NextRow());
    }
    // The applied stats live in mod-property-override's own rows (source
    // 'paragon'), reloaded by that module - no reapply needed here.
}

void ParagonMgr::UnloadPlayer(Player* player)
{
    if (!player || !player->GetSession())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    if (AccountState* st = FindAccount(accountId))
        if (st->chars == 0 || --st->chars == 0)
            _accounts.erase(accountId);
    _perkRanks.erase(player->GetGUID().GetCounter());
}

uint64 ParagonMgr::GetLifetimePX(uint32 accountId) const
{
    AccountState const* st = FindAccount(accountId);
    return st ? st->lifetimePx : 0;
}

uint32 ParagonMgr::GetAllocPercent(uint32 accountId) const
{
    AccountState const* st = FindAccount(accountId);
    return st ? st->allocPercent : 0;
}

bool ParagonMgr::IsOptedOut(uint32 accountId) const
{
    AccountState const* st = FindAccount(accountId);
    return st && st->optOut;
}

void ParagonMgr::PersistAccount(uint32 accountId, AccountState const& st) const
{
    CharacterDatabase.Execute(
        "INSERT INTO paragon_account_progress "
        "(account, lifetime_px, season_px, last_reward_level, paragon_opt_out, "
        "xp_allocation_percent, season, updated_at, created_at) "
        "VALUES ({}, {}, 0, {}, {}, {}, 0, UNIX_TIMESTAMP(), UNIX_TIMESTAMP()) "
        "ON DUPLICATE KEY UPDATE lifetime_px = VALUES(lifetime_px), "
        "last_reward_level = VALUES(last_reward_level), "
        "paragon_opt_out = VALUES(paragon_opt_out), "
        "xp_allocation_percent = VALUES(xp_allocation_percent), "
        "updated_at = UNIX_TIMESTAMP()",
        accountId, st.lifetimePx, st.lastRewardLevel,
        st.optOut ? 1 : 0, st.allocPercent);
}

void ParagonMgr::SetAllocPercent(uint32 accountId, uint32 percent)
{
    AccountState& st = _accounts[accountId];
    st.allocPercent = percent;
    st.rowExists = true;
    PersistAccount(accountId, st);
}

void ParagonMgr::SetOptOut(uint32 accountId, bool state)
{
    AccountState& st = _accounts[accountId];
    st.optOut = state;
    st.rowExists = true;
    PersistAccount(accountId, st);
}

void ParagonMgr::AddPX(Player* player, uint64 px)
{
    if (!_enabled || !player || !px || !player->GetSession())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    AccountState& st = _accounts[accountId];
    uint32 prevLevel = ComputePL(st.lifetimePx);
    st.lifetimePx += px;
    st.rowExists = true;
    uint32 newLevel = ComputePL(st.lifetimePx);

    if (newLevel > st.lastRewardLevel)
    {
        uint32 prevReward = st.lastRewardLevel;
        st.lastRewardLevel = newLevel;
        PersistAccount(accountId, st);
        HandleLevelCrossings(player, prevReward, newLevel);
    }
    else
        PersistAccount(accountId, st);

    if (_debug)
        LOG_INFO("module", "mod-paragon: +{} PX account {} (lifetime {} PL {}->{})",
                 px, accountId, st.lifetimePx, prevLevel, newLevel);
}

void ParagonMgr::SetLifetimePX(Player* player, uint64 px, uint32 rewardLevel)
{
    if (!player || !player->GetSession())
        return;
    uint32 accountId = player->GetSession()->GetAccountId();
    AccountState& st = _accounts[accountId];
    st.lifetimePx = px;
    st.lastRewardLevel = rewardLevel;
    st.rowExists = true;
    PersistAccount(accountId, st);
}

void ParagonMgr::HandleLevelCrossings(Player* player, uint32 prevLevel, uint32 newLevel)
{
    for (uint32 lv = prevLevel + 1; lv <= newLevel; ++lv)
    {
        if (_milestoneLevels.count(lv))
            for (auto const& p : ObjectAccessor::GetPlayers())
                if (Player* onlinePlayer = p.second)
                    if (WorldSession* sess = onlinePlayer->GetSession())
                        ChatHandler(sess).PSendSysMessage(
                            LANG_PARAGON_BROADCAST, player->GetName(), lv);

        RewardDispatcher::DeliverLevelTo(player, lv);
        player->SendPlaySpellVisual(70); // level-up sparkle
    }
}

uint32 ParagonMgr::ComputePL(uint64 lifetimePx) const
{
    return _pxPerLevel ? static_cast<uint32>(lifetimePx / _pxPerLevel) : 0;
}

uint64 ParagonMgr::ComputeProgressInLevel(uint64 lifetimePx) const
{
    return _pxPerLevel ? (lifetimePx % _pxPerLevel) : 0;
}

uint32 ParagonMgr::GetPerkRanks(ObjectGuid::LowType guid, perks::Property prop) const
{
    auto it = _perkRanks.find(guid);
    if (it == _perkRanks.end())
        return 0;
    auto rankIt = it->second.find(static_cast<uint8>(prop));
    return rankIt != it->second.end() ? rankIt->second : 0;
}

void ParagonMgr::ApplyPerkOverride(Player* player, perks::Property prop, uint32 ranks)
{
    auto idx = perks::PerkIndex(prop);
    if (!idx)
        return;
    uint32 total = perks::TotalValue(_perkCfg, *idx, ranks);
    auto& props = mod_property_override::PropertyOverrideMgr::Instance();
    if (!props.SetPlayerOverride(player, "paragon", prop,
                                 static_cast<int32>(total), 0))
        LOG_WARN("module", "mod-paragon: SetPlayerOverride failed for guid {}; "
                 "is mod-property-override enabled?",
                 player->GetGUID().GetCounter());
}

bool ParagonMgr::TryPurchasePerk(Player* player, perks::Property prop)
{
    if (!_enabled || !player)
        return false;

    ChatHandler ch(player->GetSession());
    auto idx = perks::PerkIndex(prop);
    if (!idx)
        return false;

    // Never take payment the stat engine can't honor (degenerate config:
    // mod-property-override disabled while paragon stays on).
    if (!mod_property_override::PropertyOverrideMgr::Instance().IsEnabled())
    {
        ch.SendSysMessage("|cffffd100[Paragon]|r The stat engine "
                          "(mod-property-override) is disabled - purchases are off.");
        return false;
    }

    // Spend gate: keeps funded low-level alts from buying bracket-breaking
    // stats (the earn side is gated by MinToggleLevel already).
    if (player->GetLevel() < _perkMinLevel)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r Perks unlock at level {}.", _perkMinLevel);
        return false;
    }

    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    uint32 ranks = GetPerkRanks(guidLow, prop);
    uint32 cost = perks::CostForNextRank(_perkCfg, ranks);
    if (!cost)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r {} is already at max rank ({}).",
                           PropertyName(prop), _perkCfg.maxRanks);
        return false;
    }

    if (player->GetItemCount(_coinItemId, false) < cost)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r You need {} Paragon Coin(s) "
                           "for {} rank {}.", cost, PropertyName(prop), ranks + 1);
        return false;
    }

    player->DestroyItemCount(_coinItemId, cost, true);
    ++ranks;
    _perkRanks[guidLow][static_cast<uint8>(prop)] = ranks;
    CharacterDatabase.Execute(
        "INSERT INTO paragon_perk_ranks (guid, property, ranks) VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE ranks = VALUES(ranks)",
        guidLow, static_cast<uint8>(prop), ranks);
    ApplyPerkOverride(player, prop, ranks);

    ch.PSendSysMessage("|cffffd100[Paragon]|r {} rank {}/{} - total |cff40ff40+{}|r.",
                       PropertyName(prop), ranks, _perkCfg.maxRanks,
                       perks::TotalValue(_perkCfg, *idx, ranks));
    return true;
}

bool ParagonMgr::TryPurchaseItemUpgrade(Player* player, Item* item, upgrades::Property prop)
{
    if (!_enabled || !_itemUpgradeEnabled || !player || !item)
        return false;

    ChatHandler ch(player->GetSession());
    if (!mod_property_override::PropertyOverrideMgr::Instance().IsEnabled())
    {
        ch.SendSysMessage("|cffffd100[Paragon]|r The stat engine "
                          "(mod-property-override) is disabled - purchases are off.");
        return false;
    }
    if (player->GetLevel() < _perkMinLevel)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r Item upgrades unlock at level {}.",
                           _perkMinLevel);
        return false;
    }

    upgrades::PropertyDef const* def = upgrades::FindDef(prop);
    ItemTemplate const* proto = item->GetTemplate();
    if (!def || !proto)
        return false;

    auto& props = mod_property_override::PropertyOverrideMgr::Instance();
    ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
    auto rows = props.GetActiveOverrides(player, itemGuid);

    float budget = upgrades::UpgradeBudget(_upgradeCfg, proto->Quality, proto->ItemLevel);
    float spent = mod_property_override::BudgetSpent(rows, "paragon");
    float chunkCost = mod_property_override::PropertyWeight(prop) *
                      static_cast<float>(def->chunk);
    if (spent + chunkCost > budget + 0.001f)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r {} has no room for that upgrade "
                           "(budget {:.0f}/{:.0f}).", proto->Name1, spent, budget);
        return false;
    }

    uint32 coins = upgrades::CostForNextChunk(budget > 0.f ? spent / budget : 1.f);
    if (player->GetItemCount(_coinItemId, false) < coins)
    {
        ch.PSendSysMessage("|cffffd100[Paragon]|r You need {} Paragon Coin(s) for that upgrade.",
                           coins);
        return false;
    }

    int32 current = 0;
    for (auto const& row : rows)
        if (row.source == "paragon" && row.property == static_cast<uint8>(prop))
        {
            current = row.value;
            break;
        }

    player->DestroyItemCount(_coinItemId, coins, true);
    if (!props.AddOverride(player, item, "paragon", prop,
                           current + static_cast<int32>(def->chunk), 0))
    {
        LOG_WARN("module", "mod-paragon: AddOverride failed for item {} (player {})",
                 itemGuid, player->GetGUID().GetCounter());
        return false;
    }
    props.SendAddonMessage(player, mod_property_override::addon::BuildInvalidate());

    ch.PSendSysMessage("|cffffd100[Paragon]|r {}: {} |cff40ff40+{} -> +{}|r "
                       "(budget {:.0f}/{:.0f}, paid {} coin(s)).",
                       proto->Name1, def->label, current,
                       current + static_cast<int32>(def->chunk),
                       spent + chunkCost, budget, coins);
    return true;
}

void ParagonMgr::ResetPerks(Player* player)
{
    if (!player)
        return;
    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();
    _perkRanks.erase(guidLow);
    CharacterDatabase.Execute("DELETE FROM paragon_perk_ranks WHERE guid = {}", guidLow);
    mod_property_override::PropertyOverrideMgr::Instance()
        .ClearPlayerOverrides(player, "paragon");
}

void ParagonMgr::OnLogin(Player* player)
{
    if (!_enabled || !player || !player->GetSession())
        return;

    RewardDispatcher::DeliverPendingFor(player);

    if (!_loginSplash)
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    uint64 lifetimePx = GetLifetimePX(accountId);
    if (!lifetimePx && !GetAllocPercent(accountId))
        return; // never touched the system; stay quiet

    ChatHandler(player->GetSession()).PSendSysMessage(
        LANG_PARAGON_LOGIN_SPLASH,
        ComputePL(lifetimePx),
        static_cast<unsigned long long>(ComputeProgressInLevel(lifetimePx)),
        _pxPerLevel);
}

} // namespace mod_paragon
