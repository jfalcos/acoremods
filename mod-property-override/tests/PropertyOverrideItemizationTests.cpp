#include "PropertyOverrideItemization.h"

#include <gtest/gtest.h>

using mod_property_override::AllProperties;
using mod_property_override::BudgetSpent;
using mod_property_override::NativeBudget;
using mod_property_override::OverrideRow;
using mod_property_override::Property;
using mod_property_override::PropertyWeight;

TEST(Itemization, EveryPropertyHasAWeight)
{
    for (Property p : AllProperties())
        EXPECT_TRUE(PropertyWeight(p) > 0.f) << static_cast<int>(p);
    // Outside the vocabulary -> zero (BudgetSpent skips such rows).
    EXPECT_EQ(PropertyWeight(static_cast<Property>(99)), 0.f);
}

TEST(Itemization, TriRatingsWeighTriple)
{
    EXPECT_EQ(PropertyWeight(Property::CritRating), 3.f);
    EXPECT_EQ(PropertyWeight(Property::CritMeleeRating), 1.f);
}

TEST(Itemization, NativeBudgetMonotonic)
{
    EXPECT_TRUE(NativeBudget(2, 100) < NativeBudget(3, 100));
    EXPECT_TRUE(NativeBudget(3, 100) < NativeBudget(4, 100));
    EXPECT_TRUE(NativeBudget(4, 100) < NativeBudget(4, 200));
    // Fitted anchors (see derivation in the header).
    EXPECT_TRUE(NativeBudget(4, 200) > 230.f && NativeBudget(4, 200) < 250.f);
    // Out-of-range quality clamps instead of reading past the table.
    EXPECT_EQ(NativeBudget(99, 100), NativeBudget(6, 100));
}

TEST(Itemization, BudgetSpentWeighsRowsOfOwnSourceOnly)
{
    std::vector<OverrideRow> rows =
    {
        { "paragon", static_cast<uint8>(Property::Stamina),     20, 0 }, // w1.0 -> 20
        { "paragon", static_cast<uint8>(Property::AttackPower), 40, 0 }, // w0.5 -> 20
        { "paragon", static_cast<uint8>(Property::Armor),      100, 0 }, // w0.1 -> 10
        { "mix",     static_cast<uint8>(Property::Stamina),     99, 0 }, // other source
        { "gm",      static_cast<uint8>(Property::Stamina),     99, 0 }, // other source
    };
    float spent = BudgetSpent(rows, "paragon");
    EXPECT_TRUE(spent > 49.9f && spent < 50.1f);
    float mixSpent = BudgetSpent(rows, "mix");
    EXPECT_TRUE(mixSpent > 98.9f && mixSpent < 99.1f);
    EXPECT_EQ(BudgetSpent({}, "paragon"), 0.f);
}
