#include "ParagonPerks.h"

#include <gtest/gtest.h>

using namespace mod_paragon::perks;
using mod_property_override::Property;

TEST(ParagonPerks, PerkSetMembership)
{
    EXPECT_TRUE(IsPerkProperty(Property::Strength));
    EXPECT_TRUE(IsPerkProperty(Property::Stamina));
    EXPECT_TRUE(IsPerkProperty(Property::AttackPower));
    EXPECT_TRUE(IsPerkProperty(Property::SpellPower));
    EXPECT_FALSE(IsPerkProperty(Property::CritRating));
    EXPECT_FALSE(IsPerkProperty(Property::Armor));
    EXPECT_FALSE(IsPerkProperty(Property::Mana));
}

TEST(ParagonPerks, PerkIndexMatchesSetOrder)
{
    for (size_t i = 0; i < PERK_SET.size(); ++i)
    {
        auto idx = PerkIndex(PERK_SET[i]);
        ASSERT_TRUE(idx.has_value());
        EXPECT_EQ(*idx, i);
    }
    EXPECT_FALSE(PerkIndex(Property::HasteRating).has_value());
}

TEST(ParagonPerks, CostCurveStepsUp)
{
    PerkConfig cfg; // maxRanks 20, costStepEvery 5
    EXPECT_EQ(CostForNextRank(cfg, 0), 1u);
    EXPECT_EQ(CostForNextRank(cfg, 4), 1u);
    EXPECT_EQ(CostForNextRank(cfg, 5), 2u);
    EXPECT_EQ(CostForNextRank(cfg, 9), 2u);
    EXPECT_EQ(CostForNextRank(cfg, 10), 3u);
    EXPECT_EQ(CostForNextRank(cfg, 19), 4u);
}

TEST(ParagonPerks, MaxRankNotPurchasable)
{
    PerkConfig cfg;
    EXPECT_EQ(CostForNextRank(cfg, cfg.maxRanks), 0u);
    EXPECT_EQ(CostForNextRank(cfg, cfg.maxRanks + 5), 0u);
}

TEST(ParagonPerks, ZeroCostStepMeansFlatCost)
{
    PerkConfig cfg;
    cfg.costStepEvery = 0;
    EXPECT_EQ(CostForNextRank(cfg, 0), 1u);
    EXPECT_EQ(CostForNextRank(cfg, 19), 1u);
}

TEST(ParagonPerks, TotalValueScalesAndClamps)
{
    PerkConfig cfg; // primaries 5/rank, AP 10/rank, SP 6/rank
    EXPECT_EQ(TotalValue(cfg, 0, 0), 0u);
    EXPECT_EQ(TotalValue(cfg, 2, 3), 15u);  // Stamina x3
    EXPECT_EQ(TotalValue(cfg, 5, 2), 20u);  // AttackPower x2
    EXPECT_EQ(TotalValue(cfg, 6, 1), 6u);   // SpellPower x1
    EXPECT_EQ(TotalValue(cfg, 2, 99), 5u * cfg.maxRanks); // clamped
    EXPECT_EQ(TotalValue(cfg, 99, 5), 0u);  // bad index
}
