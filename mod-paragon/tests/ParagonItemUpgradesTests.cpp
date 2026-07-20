#include "ParagonItemUpgrades.h"

#include <gtest/gtest.h>

#include <set>

using namespace mod_paragon::upgrades;
using mod_property_override::AllProperties;
using mod_property_override::NativeBudget;
using mod_property_override::Property;
using mod_property_override::PropertyWeight;

TEST(ItemUpgrades, TableCoversEverySupportedProperty)
{
    for (Property p : AllProperties())
    {
        PropertyDef const* def = FindDef(p);
        ASSERT_NE(def, nullptr) << static_cast<int>(p);
        EXPECT_TRUE(def->chunk > 0);
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
        float points = PropertyWeight(def.prop) * static_cast<float>(def.chunk);
        EXPECT_TRUE(points >= 4.f && points <= 15.f + 1.f)
            << "prop " << static_cast<int>(def.prop) << " points " << points;
    }
}

TEST(ItemUpgrades, UpgradeBudgetAppliesPercent)
{
    UpgradeConfig cfg;
    cfg.budgetPercent = 0.30f;
    float native = NativeBudget(4, 200);
    EXPECT_TRUE(UpgradeBudget(cfg, 4, 200) > native * 0.29f);
    EXPECT_TRUE(UpgradeBudget(cfg, 4, 200) < native * 0.31f);
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
