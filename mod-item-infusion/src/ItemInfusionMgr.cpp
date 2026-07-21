#include "ItemInfusionMgr.h"

#include "ParagonMgr.h"
#include "PropertyOverrideAddonMsg.h"
#include "PropertyOverrideMgr.h"

#include "Chat.h"
#include "Config.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

namespace mod_item_infusion
{

namespace
{
    // Parses "itemId:percent,itemId:percent" substance lists into
    // (itemId, fraction) pairs. Malformed entries are skipped.
    std::vector<std::pair<uint32, float>> ParseSubstances(std::string const& csv)
    {
        std::vector<std::pair<uint32, float>> out;
        uint32 id = 0, pct = 0;
        bool inPct = false, any = false;
        auto flush = [&]()
        {
            if (any && inPct && id && pct)
                out.emplace_back(id, static_cast<float>(pct) / 100.f);
            id = pct = 0;
            inPct = any = false;
        };
        for (char c : csv)
        {
            if (c == ',') { flush(); continue; }
            if (c == ':') { inPct = true; continue; }
            if (c < '0' || c > '9') continue;
            (inPct ? pct : id) = (inPct ? pct : id) * 10 + (c - '0');
            any = true;
        }
        flush();
        return out;
    }
}

ItemInfusionMgr& ItemInfusionMgr::Instance()
{
    static ItemInfusionMgr inst;
    return inst;
}

void ItemInfusionMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("ItemInfusion.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("ItemInfusion.Debug", false);
    _minLevel = sConfigMgr->GetOption<uint32>("ItemInfusion.MinLevel", 10);
    _alchemistEntry = sConfigMgr->GetOption<uint32>("ItemInfusion.AlchemistEntry", 96010);
    _cfg.masteryGrace = sConfigMgr->GetOption<uint32>("ItemInfusion.Mastery.GraceLevels", 10);
    _cfg.masteryPenaltyPerLevel =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Mastery.PenaltyPerLevelPercent", 2) / 100.0f;
    _cfg.masteryPenaltyMax =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Mastery.MaxPenaltyPercent", 30) / 100.0f;
    _masteredAtLevel = sConfigMgr->GetOption<uint32>("ItemInfusion.Mastery.MasteredAtLevel", 80);
    _cfg.substanceGrace = sConfigMgr->GetOption<uint32>("ItemInfusion.SubstanceGraceLevels", 15);
    _cfg.efficiency =
        sConfigMgr->GetOption<uint32>("ItemInfusion.EfficiencyPercent", 35) / 100.0f;
    _cfg.riskBase =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Risk.BasePercent", 5) / 100.0f;
    _cfg.riskSlope =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Risk.SlopePercent", 45) / 100.0f;
    _cfg.riskPivot =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Risk.PivotPercent", 30) / 100.0f;
    _cfg.riskExp = sConfigMgr->GetOption<float>("ItemInfusion.Risk.Exponent", 1.6f);
    _cfg.riskMax =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Risk.MaxPercent", 90) / 100.0f;
    _cfg.riskFloor =
        sConfigMgr->GetOption<uint32>("ItemInfusion.Risk.FloorPercent", 2) / 100.0f;
    _cfg.coinReduction =
        sConfigMgr->GetOption<uint32>("ItemInfusion.CoinReductionPercent", 5) / 100.0f;
    _substances = ParseSubstances(sConfigMgr->GetOption<std::string>(
        "ItemInfusion.Substances",
        "118:5,858:5,3928:5,33447:5,33448:5,36906:8,35625:10"));

    LOG_INFO("module",
             "mod-item-infusion: enable={} minLevel={} alchemist={} eff={:.2f} "
             "risk(base={:.2f} slope={:.2f} pivot={:.2f} exp={:.1f} max={:.2f} "
             "floor={:.2f}) coinRed={:.2f} mastery(grace={} perLvl={:.2f} "
             "max={:.2f} masteredAt={}) substances={}",
             _enabled, _minLevel, _alchemistEntry, _cfg.efficiency,
             _cfg.riskBase, _cfg.riskSlope, _cfg.riskPivot, _cfg.riskExp,
             _cfg.riskMax, _cfg.riskFloor, _cfg.coinReduction,
             _cfg.masteryGrace, _cfg.masteryPenaltyPerLevel,
             _cfg.masteryPenaltyMax, _masteredAtLevel, _substances.size());
}

uint32 ItemInfusionMgr::CoinItemId() const
{
    return mod_paragon::ParagonMgr::Instance().CoinItemId();
}

std::vector<DonorStat> ItemInfusionMgr::CollectDonorStats(ItemTemplate const* proto)
{
    std::vector<DonorStat> stats;
    if (!proto)
        return stats;
    for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        stats.push_back({ proto->ItemStat[i].ItemStatType,
                          proto->ItemStat[i].ItemStatValue });
    auto push = [&](uint32 id, int32 v)
    {
        if (v > 0)
            stats.push_back({ id, v });
    };
    push(100, static_cast<int32>(proto->Armor));
    push(101, proto->HolyRes);
    push(102, proto->FireRes);
    push(103, proto->NatureRes);
    push(104, proto->FrostRes);
    push(105, proto->ShadowRes);
    push(106, proto->ArcaneRes);
    return stats;
}

float ItemInfusionMgr::MasteryPenaltyFor(Player* player, ItemTemplate const* target,
                                         ItemTemplate const* donor) const
{
    if (!player || !target)
        return 0.f;
    uint32 level = player->GetLevel();
    if (_masteredAtLevel && level >= _masteredAtLevel)
        return 0.f;
    uint32 reqLevel = target->RequiredLevel;
    if (donor)
        reqLevel = std::max(reqLevel, donor->RequiredLevel);
    return MasteryPenalty(_cfg, level, reqLevel);
}

ItemInfusionMgr::InfuseResult ItemInfusionMgr::TryInfuse(
    Player* player, Item* target, Item* donor,
    uint32 coins, std::vector<uint32> const& substanceIds)
{
    if (!_enabled || !player || !player->GetSession() || !target || !donor)
        return InfuseResult::Rejected;

    ChatHandler ch(player->GetSession());
    // Never take the donor/materials if the stat engine can't honor the
    // transfer (degenerate config: mod-property-override disabled).
    if (!mod_property_override::PropertyOverrideMgr::Instance().IsEnabled())
    {
        ch.SendSysMessage("|cffffd100[Infusion]|r The stat engine "
                          "(mod-property-override) is disabled - infusions are off.");
        return InfuseResult::Rejected;
    }
    if (player->GetLevel() < _minLevel)
    {
        ch.PSendSysMessage("|cffffd100[Infusion]|r Infusions unlock at level {}.", _minLevel);
        return InfuseResult::Rejected;
    }
    if (target->GetGUID() == donor->GetGUID())
    {
        ch.SendSysMessage("|cffffd100[Infusion]|r An item cannot be infused with itself.");
        return InfuseResult::Rejected;
    }

    ItemTemplate const* tproto = target->GetTemplate();
    ItemTemplate const* dproto = donor->GetTemplate();
    if (!tproto || !dproto)
        return InfuseResult::Rejected;

    if (mod_property_override::NativeBudget(tproto->Quality, tproto->ItemLevel) <= 0.f)
    {
        ch.PSendSysMessage("|cffffd100[Infusion]|r {} is too basic to hold an infusion.",
                           tproto->Name1);
        return InfuseResult::Rejected;
    }

    auto yield = DonorYield(CollectDonorStats(dproto), _cfg.efficiency);
    if (yield.empty())
    {
        ch.PSendSysMessage("|cffffd100[Infusion]|r {} has no essence worth transferring.",
                           dproto->Name1);
        return InfuseResult::Rejected;
    }

    if (coins && player->GetItemCount(CoinItemId(), false) < coins)
    {
        ch.PSendSysMessage("|cffffd100[Infusion]|r You do not have {} Paragon Coin(s).",
                           coins);
        return InfuseResult::Rejected;
    }
    uint32 gearReqLevel = std::max(tproto->RequiredLevel, dproto->RequiredLevel);
    std::vector<float> reductions;
    for (uint32 id : substanceIds)
    {
        auto it = std::find_if(_substances.begin(), _substances.end(),
                               [&](auto const& p) { return p.first == id; });
        if (it == _substances.end())
        {
            ch.PSendSysMessage("|cffffd100[Infusion]|r Item {} is not a known substance.", id);
            return InfuseResult::Rejected;
        }
        ItemTemplate const* stmpl = sObjectMgr->GetItemTemplate(id);
        if (!stmpl || !player->GetItemCount(id, false))
        {
            ch.PSendSysMessage("|cffffd100[Infusion]|r You have no {} left.",
                               stmpl ? stmpl->Name1 : "substance");
            return InfuseResult::Rejected;
        }
        if (!SubstanceEffective(_cfg, stmpl->ItemLevel, gearReqLevel))
        {
            ch.PSendSysMessage("|cffffd100[Infusion]|r {} is too weak to stabilize "
                               "this gear.", stmpl->Name1);
            return InfuseResult::Rejected;
        }
        reductions.push_back(it->second);
    }

    auto& props = mod_property_override::PropertyOverrideMgr::Instance();
    auto rows = props.GetActiveOverrides(player, target->GetGUID().GetCounter());
    float f = MixFraction(rows, tproto->Quality, tproto->ItemLevel);
    float penalty = MasteryPenaltyFor(player, tproto, dproto);
    float risk = MitigatedRisk(_cfg, RiskFor(_cfg, f, penalty), coins, reductions);
    if (_riskOverridePct >= 0)
        risk = std::min(static_cast<float>(_riskOverridePct) / 100.f, 1.f);
    else if (penalty > 0.f)
        ch.PSendSysMessage("|cffffd100[Infusion]|r This gear is beyond your mastery: "
                           "|cffff2020+{:.0f}%|r risk.", penalty * 100.f);

    // The price is paid win or lose: mitigation materials and the donor.
    if (coins)
        player->DestroyItemCount(CoinItemId(), coins, true);
    for (uint32 id : substanceIds)
        player->DestroyItemCount(id, 1, true);
    std::string donorName = dproto->Name1;
    player->DestroyItem(donor->GetBagSlot(), donor->GetSlot(), true);

    if (frand(0.f, 1.f) < risk)
    {
        std::string targetName = tproto->Name1;
        player->DestroyItem(target->GetBagSlot(), target->GetSlot(), true);
        ch.PSendSysMessage("|cffff2020[Infusion]|r The infusion destabilizes... "
                           "{} is DESTROYED along with {}.", targetName, donorName);
        if (_debug)
            LOG_INFO("module", "mod-item-infusion: {} destroyed target (risk {:.2f}).",
                     player->GetName(), risk);
        return InfuseResult::Destroyed;
    }

    std::string gains;
    for (auto const& e : yield)
    {
        int32 current = 0;
        for (auto const& row : rows)
            if (row.source == "mix" && row.property == static_cast<uint8>(e.prop))
            {
                current = row.value;
                break;
            }
        props.AddOverride(player, target, "mix", e.prop, current + e.amount, 0);
        if (!gains.empty())
            gains += ", ";
        gains += fmt::format("|cff1eff00+{}|r {}",
                             e.amount,
                             mod_property_override::PropertyName(e.prop));
    }
    props.SendAddonMessage(player, mod_property_override::addon::BuildInvalidate());

    auto newRows = props.GetActiveOverrides(player, target->GetGUID().GetCounter());
    float newF = MixFraction(newRows, tproto->Quality, tproto->ItemLevel);
    ch.PSendSysMessage("|cffffd100[Infusion]|r {} absorbs {}: {}. "
                       "Next infusion risk: {:.0f}%.",
                       tproto->Name1, donorName, gains,
                       RiskFor(_cfg, newF, MasteryPenaltyFor(player, tproto)) * 100.f);
    return InfuseResult::Survived;
}

} // namespace mod_item_infusion
