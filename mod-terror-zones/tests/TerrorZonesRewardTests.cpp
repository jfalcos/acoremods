// Pure-function unit tests for mod-terror-zones Slice 3 reward math
// (plan §8.1). Only `ComputeMultipliedValue` is exercised here —
// `TryTierBump`, `ApplyXpMultiplier`, and the loot / quest hook paths
// all require live Player / Loot / ItemTemplateContainer state that
// can't be fabricated cheaply in a unit harness. Those gates are
// covered by in-game verification (plan §11 steps 3–6).

#include "TerrorZonesMgr.h"

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

using namespace mod_terror_zones;

TEST(TerrorZonesRewards, ZeroBaselineReturnsZero)
{
    EXPECT_EQ(ComputeMultipliedValue(0, 1.5f), 0u);
    EXPECT_EQ(ComputeMultipliedValue(0, 0.0f), 0u);
    EXPECT_EQ(ComputeMultipliedValue(0, 1000.0f), 0u);
}

TEST(TerrorZonesRewards, UnityMultiplierLeavesValueUnchanged)
{
    EXPECT_EQ(ComputeMultipliedValue(100, 1.0f), 100u);
    EXPECT_EQ(ComputeMultipliedValue(123456, 1.0f), 123456u);
}

TEST(TerrorZonesRewards, DefaultPlus50Percent)
{
    // Spec §4.4 default: +50% XP / +50% gold. 100 XP → 150 XP.
    EXPECT_EQ(ComputeMultipliedValue(100, 1.5f), 150u);
    EXPECT_EQ(ComputeMultipliedValue(1000, 1.5f), 1500u);
    // Small values still scale deterministically (integer truncation
    // of 1 * 1.5 = 1.5 → 1, matching "floor").
    EXPECT_EQ(ComputeMultipliedValue(1, 1.5f), 1u);
    EXPECT_EQ(ComputeMultipliedValue(2, 1.5f), 3u);
}

TEST(TerrorZonesRewards, ZeroMultiplierZerosOutput)
{
    // A GM setting XpMultiplier=0 in config effectively disables XP in
    // empowered zones. Guarded at config load (clamp floor at 0) so the
    // helper never sees a negative.
    EXPECT_EQ(ComputeMultipliedValue(500, 0.0f), 0u);
}

TEST(TerrorZonesRewards, NegativeMultiplierTreatedAsZero)
{
    // Defensive: if a misconfigured float ever leaks through.
    EXPECT_EQ(ComputeMultipliedValue(500, -0.5f), 0u);
}

TEST(TerrorZonesRewards, LargeMultiplierSaturatesAtMax)
{
    // Flavors in Slice 4 could stack multipliers up aggressively; make
    // sure we saturate to UINT32_MAX rather than wrap.
    uint32 big = 1'000'000'000u;
    uint32 result = ComputeMultipliedValue(big, 100.0f);
    EXPECT_EQ(result, std::numeric_limits<uint32>::max());
}

TEST(TerrorZonesRewards, UintMaxTimesUnityStays)
{
    uint32 max = std::numeric_limits<uint32>::max();
    EXPECT_EQ(ComputeMultipliedValue(max, 1.0f), max);
}

TEST(TerrorZonesRewards, FractionalDownMultiplierTruncates)
{
    // 0.5× baseline — confirms truncation is consistent with integer math.
    EXPECT_EQ(ComputeMultipliedValue(100, 0.5f), 50u);
    EXPECT_EQ(ComputeMultipliedValue(101, 0.5f), 50u);  // 50.5 → 50
}

// -----------------------------------------------------------------------------
// Slice 10 — effort-anchored gold floor (pure helpers)
// -----------------------------------------------------------------------------

TEST(TerrorZonesGoldFloor, BaseCopperFollowsLevelCurve)
{
    // Default 0.5 * level^2.
    EXPECT_EQ(GoldFloorBaseCopper(72, 0.5f, 2.0f), 2592u);   // 0.5*5184
    EXPECT_EQ(GoldFloorBaseCopper(80, 0.5f, 2.0f), 3200u);   // 0.5*6400
    EXPECT_EQ(GoldFloorBaseCopper(10, 0.5f, 2.0f), 50u);     // 0.5*100
    // Linear exponent.
    EXPECT_EQ(GoldFloorBaseCopper(40, 1.0f, 1.0f), 40u);
}

TEST(TerrorZonesGoldFloor, BaseCopperZeroInputsDisable)
{
    EXPECT_EQ(GoldFloorBaseCopper(0, 0.5f, 2.0f), 0u);       // level 0
    EXPECT_EQ(GoldFloorBaseCopper(72, 0.0f, 2.0f), 0u);      // perLevel 0
    EXPECT_EQ(GoldFloorBaseCopper(72, -1.0f, 2.0f), 0u);     // negative
}

TEST(TerrorZonesGoldFloor, EffortRatioAndClamp)
{
    // A normal empowered mob (~7x native HP) reads ~7 effort.
    EXPECT_FLOAT_EQ(KillEffortFactor(67200, 9600, 1.0f, 12.0f), 7.0f);
    // Clamped to max for a boss-tier HP pool.
    EXPECT_FLOAT_EQ(KillEffortFactor(960000, 9600, 1.0f, 12.0f), 12.0f);
    // Squishy kill never drops below min.
    EXPECT_FLOAT_EQ(KillEffortFactor(100, 9600, 1.0f, 12.0f), 1.0f);
}

TEST(TerrorZonesGoldFloor, EffortDegenerateInputs)
{
    EXPECT_FLOAT_EQ(KillEffortFactor(0, 9600, 1.0f, 12.0f), 1.0f);   // no HP
    EXPECT_FLOAT_EQ(KillEffortFactor(67200, 0, 1.0f, 12.0f), 1.0f);  // no ref
    // max < min is repaired up to min.
    EXPECT_FLOAT_EQ(KillEffortFactor(67200, 9600, 2.0f, 1.0f), 2.0f);
}

TEST(TerrorZonesGoldFloor, ComposeAndCap)
{
    // 2592 base * 7 effort * 1.55 roll = 28123c, under the 50g cap.
    EXPECT_EQ(ComputeGoldFloorCopper(2592, 7.0f, 1.55f, 500000u), 28123u);
    // Cap bites: 2592 * 12 * 20 = 622080 > 500000.
    EXPECT_EQ(ComputeGoldFloorCopper(2592, 12.0f, 20.0f, 500000u), 500000u);
    // Same inputs, cap 0 = uncapped → full 622080.
    EXPECT_EQ(ComputeGoldFloorCopper(2592, 12.0f, 20.0f, 0u), 622080u);
}

TEST(TerrorZonesGoldFloor, ComposeZeroInputsReturnZero)
{
    EXPECT_EQ(ComputeGoldFloorCopper(0, 7.0f, 1.55f, 500000u), 0u);
    EXPECT_EQ(ComputeGoldFloorCopper(2592, 0.0f, 1.55f, 500000u), 0u);
    EXPECT_EQ(ComputeGoldFloorCopper(2592, 7.0f, 0.0f, 500000u), 0u);
}

// -----------------------------------------------------------------------------
// Slice 10 Pass 2 — contract credit + mailed reward (pure helpers)
// -----------------------------------------------------------------------------

TEST(TerrorZonesContract, KillCreditScalesWithHp)
{
    // ~67k-HP T3 wolf / 1000 = 67 credit.
    EXPECT_EQ(KillCredit(67270, 1000), 67u);
    EXPECT_EQ(KillCredit(9600, 1000), 9u);
    // Any real kill is worth at least 1.
    EXPECT_EQ(KillCredit(500, 1000), 1u);
    // No HP → no credit; divisor 0 treated as 1.
    EXPECT_EQ(KillCredit(0, 1000), 0u);
    EXPECT_EQ(KillCredit(2500, 0), 2500u);
}

TEST(TerrorZonesContract, GoldScalesWithCreditAndTier)
{
    // 3000 credit * 30c * 1.7 (T3) = 153000c (~15.3g).
    EXPECT_EQ(ContractGoldCopper(3000, 30, 1.7f, 2000000u), 153000u);
    // T1 (mult 1.0): 3000 * 30 = 90000c.
    EXPECT_EQ(ContractGoldCopper(3000, 30, 1.0f, 2000000u), 90000u);
    // Cap bites.
    EXPECT_EQ(ContractGoldCopper(3000, 30, 100.0f, 2000000u), 2000000u);
    // Zero inputs → 0.
    EXPECT_EQ(ContractGoldCopper(0, 30, 1.7f, 2000000u), 0u);
    EXPECT_EQ(ContractGoldCopper(3000, 30, 0.0f, 2000000u), 0u);
}

TEST(TerrorZonesContract, BandIndexBuckets)
{
    EXPECT_EQ(ContractBandIndexForLevel(5), 0);
    EXPECT_EQ(ContractBandIndexForLevel(10), 0);
    EXPECT_EQ(ContractBandIndexForLevel(19), 0);
    EXPECT_EQ(ContractBandIndexForLevel(20), 1);
    EXPECT_EQ(ContractBandIndexForLevel(72), 6);
    EXPECT_EQ(ContractBandIndexForLevel(79), 6);
    EXPECT_EQ(ContractBandIndexForLevel(80), 7);
    EXPECT_EQ(ContractBandIndexForLevel(83), 7);
}
