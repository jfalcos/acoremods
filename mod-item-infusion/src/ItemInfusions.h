#ifndef MOD_ITEM_INFUSION_INFUSIONS_H
#define MOD_ITEM_INFUSION_INFUSIONS_H

#include "PropertyOverrideItemization.h"
#include <vector>

// Infusion (item mixing) math — pure, no engine dependencies, unit-testable.
//
// Design (user-directed, 2026-07-20): sacrifice a donor item to transfer a
// fraction of its native stats onto a target item as source='mix' override
// rows. No hard cap — instead each attempt risks DESTROYING the target, with
// the risk growing as the target's accumulated mix points approach and exceed
// the same native (Quality, ItemLevel) budget mod-paragon's coin-upgrade cap
// is derived from. Risk can be bought down with Paragon Coins and configured
// "substances" (native consumables), floored so no attempt is ever free of
// tension. Deterministic transfer; RNG only decides survival.
//
// Itemization facts (PropertyWeight, NativeBudget, BudgetSpent) come from
// the platform: mod-property-override/src/PropertyOverrideItemization.h.

namespace mod_item_infusion
{

using mod_property_override::OverrideRow;
using mod_property_override::Property;

struct InfusionConfig
{
    float efficiency    = 0.35f; // fraction of donor stats transferred
    float riskBase      = 0.05f; // destruction chance on an un-infused target
    float riskSlope     = 0.45f; // added risk as mix fill reaches riskPivot
    float riskPivot     = 0.30f; // mix fraction where slope has fully kicked in
                                 // (matches paragon's coin-upgrade budget percent)
    float riskExp       = 1.6f;  // super-linear growth past the pivot
    float riskMax       = 0.90f; // hard ceiling
    float riskFloor     = 0.02f; // mitigation can never push risk below this
    float coinReduction = 0.05f; // risk removed per Paragon Coin pledged
    // Mastery: gear beyond the character's level is riskier to infuse
    // ("you don't know how to mix what you can't yet wield"). An item is
    // mastered once charLevel >= RequiredLevel + grace; below that each
    // missing level adds penaltyPerLevel of risk, capped. The max-level
    // exemption is engine policy, applied by callers.
    uint32 masteryGrace          = 10;
    float masteryPenaltyPerLevel = 0.02f;
    float masteryPenaltyMax      = 0.30f;
    // Substances are tier-banded like every native consumable: a reagent
    // can only stabilize gear up to its own ItemLevel + substanceGrace
    // (a Minor Healing Potion does not steady an epic).
    uint32 substanceGrace        = 15;
};

// One donor stat as read off the ItemTemplate by the engine-side adapter.
// `itemModId` uses the ITEM_MOD_* vocabulary the Property enum is built on;
// the adapter passes the platform's custom ids (100 armor, 101-106 resists)
// for the non-array template fields.
struct DonorStat
{
    uint32 itemModId;
    int32 value;
};

struct YieldEntry
{
    Property prop;
    int32 amount;
};

// Mappable donor stats scaled by `efficiency` (ceil per property, raw values
// summed first so duplicate array slots merge). Unknown ids, zero-weight
// properties, and non-positive results are dropped.
std::vector<YieldEntry> DonorYield(std::vector<DonorStat> const& stats, float efficiency);

// Weighted budget points a yield would add.
float YieldPoints(std::vector<YieldEntry> const& yield);

// Accumulated mix fill of a target: weighted points of its source='mix' rows
// over the native budget. 0 when the item has no native budget (callers
// refuse such targets before ever rolling).
float MixFraction(std::vector<OverrideRow> const& rows, uint32 quality, uint32 itemLevel);

// Extra risk from infusing gear beyond the character's mastery:
// min(masteryPenaltyMax, masteryPenaltyPerLevel *
//     max(0, (requiredLevel + masteryGrace) - charLevel)).
float MasteryPenalty(InfusionConfig const& cfg, uint32 charLevel, uint32 requiredLevel);

// Destruction chance BEFORE mitigation for a target at mix fraction `f`,
// plus any mastery penalty:
// clamp(riskBase + riskSlope * (f / riskPivot)^riskExp + penalty,
//       riskBase, riskMax).
float RiskFor(InfusionConfig const& cfg, float f, float masteryPenalty = 0.f);

// True when a substance of `substanceItemLevel` can stabilize gear whose
// (worse of target/donor) RequiredLevel is `gearRequiredLevel`:
// gearRequiredLevel <= substanceItemLevel + substanceGrace.
bool SubstanceEffective(InfusionConfig const& cfg, uint32 substanceItemLevel,
                        uint32 gearRequiredLevel);

// Risk after pledging `coins` and the given substance reductions,
// clamped to [riskFloor, riskMax].
float MitigatedRisk(InfusionConfig const& cfg, float risk, uint32 coins,
                    std::vector<float> const& substanceReductions);

} // namespace mod_item_infusion

#endif // MOD_ITEM_INFUSION_INFUSIONS_H
