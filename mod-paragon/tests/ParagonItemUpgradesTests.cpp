#include "ParagonItemUpgrades.h"

#include <gtest/gtest.h>

#include <set>

using namespace mod_paragon::upgrades;
using mod_property_override::AllProperties;
using mod_property_override::OverrideRow;
using mod_property_override::Property;

TEST(ItemUpgrades, TableCoversEverySupportedProperty)
{
    for (Property p : AllProperties())
    {
        PropertyDef const* def = FindDef(p);
        ASSERT_NE(def, nullptr) << static_cast<int>(p);
        EXPECT_TRUE(def->chunk > 0);
        EXPECT_TRUE(def->weight > 0.f);
        EXPECT_TRUE(def->label && def->label[0] != '\0');
        EXPECT_NE(CategoryOf(p), Category::Max);
    }
}

TEST(ItemUpgrades, CategoriesPartitionThePropertySet)
{
    std::set<uint8> seen;
    size_t total = 0;
    for (uint8 c = 0; c < static_cast<uint8>(Category::Max); ++c)
    {
        auto const& defs = DefsInCategory(static_cast<Category>(c));
        EXPECT_FALSE(defs.empty());
        EXPECT_LE(defs.size(), 16u); // gossip menu headroom (client caps ~32)
        for (auto const& def : defs)
        {
            EXPECT_TRUE(seen.insert(static_cast<uint8>(def.prop)).second); // no overlap
            ++total;
        }
    }
    EXPECT_EQ(total, AllProperties().size()); // complete cover
}

TEST(ItemUpgrades, ChunksAreBudgetNormalized)
{
    // One purchase should cost ~5 weighted budget points for every property
    // (that's the knob that keeps all stats equally paced).
    for (auto const& def : AllUpgradeDefs())
    {
        float points = def.weight * static_cast<float>(def.chunk);
        EXPECT_TRUE(points >= 4.f && points <= 15.f + 1.f)
            << "prop " << static_cast<int>(def.prop) << " points " << points;
    }
}

TEST(ItemUpgrades, NativeBudgetMonotonic)
{
    EXPECT_TRUE(NativeBudget(2, 100) < NativeBudget(3, 100));
    EXPECT_TRUE(NativeBudget(3, 100) < NativeBudget(4, 100));
    EXPECT_TRUE(NativeBudget(4, 100) < NativeBudget(4, 200));
    // Fitted anchors (see derivation in the header).
    EXPECT_TRUE(NativeBudget(4, 200) > 230.f && NativeBudget(4, 200) < 250.f);
}

TEST(ItemUpgrades, UpgradeBudgetAppliesPercent)
{
    UpgradeConfig cfg;
    cfg.budgetPercent = 0.30f;
    float native = NativeBudget(4, 200);
    EXPECT_TRUE(UpgradeBudget(cfg, 4, 200) > native * 0.29f);
    EXPECT_TRUE(UpgradeBudget(cfg, 4, 200) < native * 0.31f);
}

TEST(ItemUpgrades, BudgetSpentWeighsRows)
{
    std::vector<OverrideRow> rows =
    {
        { static_cast<uint8>(Property::Stamina),     20, 0 }, // w1.0 -> 20
        { static_cast<uint8>(Property::AttackPower), 40, 0 }, // w0.5 -> 20
        { static_cast<uint8>(Property::Armor),      100, 0 }, // w0.1 -> 10
    };
    float spent = BudgetSpent(rows);
    EXPECT_TRUE(spent > 49.9f && spent < 50.1f);
    EXPECT_EQ(BudgetSpent({}), 0.f);
}

TEST(ItemUpgrades, CostTiersRiseWithFill)
{
    EXPECT_EQ(CostForNextChunk(0.0f), 1u);
    EXPECT_EQ(CostForNextChunk(0.32f), 1u);
    EXPECT_EQ(CostForNextChunk(0.34f), 2u);
    EXPECT_EQ(CostForNextChunk(0.67f), 3u);
    EXPECT_EQ(CostForNextChunk(1.0f), 4u);
    EXPECT_EQ(CostForNextChunk(-1.f), 1u);  // clamped
    EXPECT_EQ(CostForNextChunk(9.f), 4u);   // clamped
}
