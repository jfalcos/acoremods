#include <gtest/gtest.h>

#include "ItemInfusions.h"

using namespace mod_item_infusion;
using mod_property_override::NativeBudget;
using mod_property_override::OverrideRow;
using mod_property_override::Property;

namespace
{
    InfusionConfig Cfg() { return {}; } // defaults under test
}

TEST(Infusions, DonorYieldMapsAndScales)
{
    // 4=Str, 7=Sta, 38=AP, 100=Armor (platform id), 45=SpellPower.
    std::vector<DonorStat> stats =
    {
        { 4, 18 }, { 7, 27 }, { 38, 36 }, { 100, 120 }, { 45, 20 },
    };
    auto yield = DonorYield(stats, 0.35f);
    ASSERT_EQ(yield.size(), 5u);
    for (auto const& e : yield)
    {
        if (e.prop == Property::Strength)    EXPECT_EQ(e.amount, 7);  // ceil(6.3)
        if (e.prop == Property::Stamina)     EXPECT_EQ(e.amount, 10); // ceil(9.45)
        if (e.prop == Property::AttackPower) EXPECT_EQ(e.amount, 13); // ceil(12.6)
        if (e.prop == Property::Armor)       EXPECT_EQ(e.amount, 42);
        if (e.prop == Property::SpellPower)  EXPECT_EQ(e.amount, 7);
    }
}

TEST(Infusions, DonorYieldMergesDuplicatesBeforeScaling)
{
    // Two +3 Sta slots at 35%: merged ceil(6 * 0.35) = 3, whereas per-slot
    // scaling would give 2 * ceil(1.05) = 4.
    std::vector<DonorStat> stats = { { 7, 3 }, { 7, 3 } };
    auto yield = DonorYield(stats, 0.35f);
    ASSERT_EQ(yield.size(), 1u);
    EXPECT_EQ(yield[0].amount, 3);
}

TEST(Infusions, DonorYieldDropsJunk)
{
    std::vector<DonorStat> stats =
    {
        { 99, 50 },   // not a valid property id
        { 12, 0 },    // zero value -> no yield
        { 4, -10 },   // negative native stat -> dropped
    };
    EXPECT_TRUE(DonorYield(stats, 0.35f).empty());
    EXPECT_TRUE(DonorYield({}, 0.35f).empty());
}

TEST(Infusions, YieldPointsUsesItemizationWeights)
{
    // 20 Sta (w1.0) + 40 AP (w0.5) = 40 points.
    std::vector<YieldEntry> yield =
    {
        { Property::Stamina, 20 }, { Property::AttackPower, 40 },
    };
    float pts = YieldPoints(yield);
    EXPECT_TRUE(pts > 39.9f && pts < 40.1f);
}

TEST(Infusions, MixFractionCountsOnlyMixRows)
{
    std::vector<OverrideRow> rows =
    {
        { "mix",     static_cast<uint8>(Property::Stamina), 20, 0 },
        { "paragon", static_cast<uint8>(Property::Stamina), 99, 0 },
        { "gm",      static_cast<uint8>(Property::Stamina), 99, 0 },
    };
    float native = NativeBudget(4, 200);
    float f = MixFraction(rows, 4, 200);
    EXPECT_TRUE(f > 20.f / native - 0.001f && f < 20.f / native + 0.001f);
    // No native budget -> fraction pinned to 0 (callers refuse such targets).
    EXPECT_EQ(MixFraction(rows, 0, 0), 0.f);
}

TEST(Infusions, RiskCurveShape)
{
    InfusionConfig cfg = Cfg();
    // Fresh item: base risk.
    EXPECT_TRUE(RiskFor(cfg, 0.f) > 0.049f && RiskFor(cfg, 0.f) < 0.051f);
    // At the pivot (paragon-cap-equivalent fill): base + slope = 50%.
    EXPECT_TRUE(RiskFor(cfg, 0.30f) > 0.499f && RiskFor(cfg, 0.30f) < 0.501f);
    // Monotonic non-decreasing.
    float prev = 0.f;
    for (float f = 0.f; f <= 2.f; f += 0.05f)
    {
        float r = RiskFor(cfg, f);
        EXPECT_TRUE(r >= prev);
        prev = r;
    }
    // Ceiling.
    EXPECT_TRUE(RiskFor(cfg, 10.f) < 0.901f);
    // Negative fraction clamps to base, never below.
    EXPECT_TRUE(RiskFor(cfg, -1.f) > 0.049f);
}

TEST(Infusions, MasteryPenaltyShape)
{
    InfusionConfig cfg = Cfg(); // grace 10, 2%/level, cap 30%
    // At or past RequiredLevel + grace: mastered, no penalty.
    EXPECT_EQ(MasteryPenalty(cfg, 30, 20), 0.f);
    EXPECT_EQ(MasteryPenalty(cfg, 80, 70), 0.f);
    // Linear in missing levels: L25 vs req-20 item -> 5 short -> 10%.
    float p = MasteryPenalty(cfg, 25, 20);
    EXPECT_TRUE(p > 0.099f && p < 0.101f);
    // Capped: L25 vs req-40 item -> 25 short -> 50% uncapped -> 30%.
    float pc = MasteryPenalty(cfg, 25, 40);
    EXPECT_TRUE(pc > 0.299f && pc < 0.301f);
    // No-requirement gear (req 0) is mastered from level >= grace.
    EXPECT_EQ(MasteryPenalty(cfg, 10, 0), 0.f);
}

TEST(Infusions, RiskIncludesMasteryPenaltyAndStillClamps)
{
    InfusionConfig cfg = Cfg();
    // Zero penalty == the historical two-arg behavior.
    EXPECT_EQ(RiskFor(cfg, 0.15f, 0.f), RiskFor(cfg, 0.15f));
    // Penalty stacks additively before the clamp: fresh item + 30% -> 35%.
    float r = RiskFor(cfg, 0.f, 0.30f);
    EXPECT_TRUE(r > 0.349f && r < 0.351f);
    // riskMax still wins over fill + penalty.
    EXPECT_TRUE(RiskFor(cfg, 10.f, 0.30f) < 0.901f);
}

TEST(Infusions, SubstanceTierBanding)
{
    InfusionConfig cfg = Cfg(); // grace 15
    // A Minor Healing Potion (ilvl 5) stabilizes gear up to req 20...
    EXPECT_TRUE(SubstanceEffective(cfg, 5, 20));
    // ...but not a req-21 blue, and certainly not a req-70 epic.
    EXPECT_FALSE(SubstanceEffective(cfg, 5, 21));
    EXPECT_FALSE(SubstanceEffective(cfg, 5, 70));
    // Endgame reagents (ilvl ~70+) cover all native gear.
    EXPECT_TRUE(SubstanceEffective(cfg, 70, 80));
    // No-requirement gear accepts anything.
    EXPECT_TRUE(SubstanceEffective(cfg, 5, 0));
}

TEST(Infusions, MitigationStacksAndFloors)
{
    InfusionConfig cfg = Cfg();
    // Coins subtract flat: 50% risk, 4 coins at -5% each -> 30%.
    float r = MitigatedRisk(cfg, 0.50f, 4, {});
    EXPECT_TRUE(r > 0.299f && r < 0.301f);
    // Substances are multiplicative (fractions of current risk), then
    // coins: 50% * 0.9 * 0.8 = 36%, minus 2 coins -> 26%.
    float r2 = MitigatedRisk(cfg, 0.50f, 2, { 0.10f, 0.20f });
    EXPECT_TRUE(r2 > 0.259f && r2 < 0.261f);
    // Multiplicative means cheap reagents shave LESS off deep gambles:
    // one -20% substance turns 90% into 72%, not 70%.
    float rDeep = MitigatedRisk(cfg, 0.90f, 0, { 0.20f });
    EXPECT_TRUE(rDeep > 0.719f && rDeep < 0.721f);
    // Overkill mitigation floors at riskFloor, never zero.
    float r3 = MitigatedRisk(cfg, 0.10f, 50, { 0.20f, 0.20f });
    EXPECT_TRUE(r3 > 0.019f && r3 < 0.021f);
    // No mitigation passes risk through.
    float r4 = MitigatedRisk(cfg, 0.42f, 0, {});
    EXPECT_TRUE(r4 > 0.419f && r4 < 0.421f);
}
