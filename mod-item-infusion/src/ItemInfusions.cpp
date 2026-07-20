#include "ItemInfusions.h"

#include <algorithm>
#include <cmath>

namespace mod_item_infusion
{

namespace po = mod_property_override;

std::vector<YieldEntry> DonorYield(std::vector<DonorStat> const& stats, float efficiency)
{
    // Sum raw values per property first so duplicate template slots merge
    // before the single ceil-scale (two +5 slots yield ceil(10e), not 2*ceil(5e)).
    std::vector<YieldEntry> raw;
    for (DonorStat const& stat : stats)
    {
        auto prop = static_cast<Property>(stat.itemModId);
        if (!po::IsValidProperty(static_cast<uint8>(stat.itemModId)))
            continue;
        if (po::PropertyWeight(prop) <= 0.f)
            continue;
        auto it = std::find_if(raw.begin(), raw.end(),
                               [&](YieldEntry const& e) { return e.prop == prop; });
        if (it != raw.end())
            it->amount += stat.value;
        else
            raw.push_back({ prop, stat.value });
    }

    std::vector<YieldEntry> yield;
    for (YieldEntry const& e : raw)
    {
        int32 amount = static_cast<int32>(
            std::ceil(static_cast<float>(e.amount) * efficiency));
        if (amount > 0)
            yield.push_back({ e.prop, amount });
    }
    return yield;
}

float YieldPoints(std::vector<YieldEntry> const& yield)
{
    float points = 0.f;
    for (YieldEntry const& e : yield)
        points += po::PropertyWeight(e.prop) * static_cast<float>(e.amount);
    return points;
}

float MixFraction(std::vector<OverrideRow> const& rows, uint32 quality, uint32 itemLevel)
{
    float native = po::NativeBudget(quality, itemLevel);
    if (native <= 0.f)
        return 0.f;
    return po::BudgetSpent(rows, "mix") / native;
}

float MasteryPenalty(InfusionConfig const& cfg, uint32 charLevel, uint32 requiredLevel)
{
    uint32 masteredAt = requiredLevel + cfg.masteryGrace;
    if (charLevel >= masteredAt)
        return 0.f;
    float penalty = cfg.masteryPenaltyPerLevel * static_cast<float>(masteredAt - charLevel);
    return std::min(penalty, cfg.masteryPenaltyMax);
}

float RiskFor(InfusionConfig const& cfg, float f, float masteryPenalty)
{
    if (f < 0.f)
        f = 0.f;
    float pivot = cfg.riskPivot > 0.f ? cfg.riskPivot : 0.30f;
    float risk = cfg.riskBase + cfg.riskSlope * std::pow(f / pivot, cfg.riskExp) +
                 masteryPenalty;
    return std::clamp(risk, cfg.riskBase, cfg.riskMax);
}

bool SubstanceEffective(InfusionConfig const& cfg, uint32 substanceItemLevel,
                        uint32 gearRequiredLevel)
{
    return gearRequiredLevel <= substanceItemLevel + cfg.substanceGrace;
}

float MitigatedRisk(InfusionConfig const& cfg, float risk, uint32 coins,
                    std::vector<float> const& substanceReductions)
{
    float reduced = risk - static_cast<float>(coins) * cfg.coinReduction;
    for (float r : substanceReductions)
        reduced -= r;
    return std::clamp(reduced, cfg.riskFloor, cfg.riskMax);
}

} // namespace mod_item_infusion
