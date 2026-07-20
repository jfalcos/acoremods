#include "PropertyOverrideProperties.h"

#include <gtest/gtest.h>

using namespace mod_property_override;

TEST(Properties, EveryPropertyHasANameAndRoundTrips)
{
    for (Property p : AllProperties())
    {
        char const* name = PropertyName(p);
        ASSERT_NE(name, nullptr);
        auto parsed = ParseProperty(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_EQ(*parsed, p);
        EXPECT_TRUE(IsValidProperty(static_cast<uint8>(p)));
    }
}

TEST(Properties, IdsMatchItemModVocabulary)
{
    // Spot-check the ITEM_MOD_* alignment contract (ItemTemplate.h).
    EXPECT_EQ(static_cast<uint8>(Property::Strength), 4);
    EXPECT_EQ(static_cast<uint8>(Property::Stamina), 7);
    EXPECT_EQ(static_cast<uint8>(Property::CritRating), 32);
    EXPECT_EQ(static_cast<uint8>(Property::AttackPower), 38);
    EXPECT_EQ(static_cast<uint8>(Property::SpellPower), 45);
}

TEST(Properties, ParseIsCaseInsensitive)
{
    EXPECT_EQ(ParseProperty("stamina"), Property::Stamina);
    EXPECT_EQ(ParseProperty("STAMINA"), Property::Stamina);
    EXPECT_EQ(ParseProperty("spellpower"), Property::SpellPower);
    EXPECT_EQ(ParseProperty("critrating"), Property::CritRating);
}

TEST(Properties, ParseAcceptsNumericIds)
{
    EXPECT_EQ(ParseProperty("0"), Property::Mana);
    EXPECT_EQ(ParseProperty("7"), Property::Stamina);
    EXPECT_EQ(ParseProperty("45"), Property::SpellPower);
    EXPECT_EQ(ParseProperty("106"), Property::ArcaneResistance);
}

TEST(Properties, ParseAcceptsUniquePrefixes)
{
    EXPECT_EQ(ParseProperty("STR"), Property::Strength);
    EXPECT_EQ(ParseProperty("agi"), Property::Agility);
    EXPECT_EQ(ParseProperty("sta"), Property::Stamina);
    EXPECT_EQ(ParseProperty("stam"), Property::Stamina);
    EXPECT_EQ(ParseProperty("INT"), Property::Intellect);
    EXPECT_EQ(ParseProperty("spi"), Property::Spirit);
    EXPECT_EQ(ParseProperty("spellpo"), Property::SpellPower);
    EXPECT_EQ(ParseProperty("attack"), Property::AttackPower);
    EXPECT_EQ(ParseProperty("armorpen"), Property::ArmorPenetrationRating);
    EXPECT_EQ(ParseProperty("exp"), Property::ExpertiseRating);
    EXPECT_EQ(ParseProperty("armor"), Property::Armor); // exact match beats prefix ambiguity
}

TEST(Properties, ParseRejectsAmbiguousPrefixes)
{
    EXPECT_FALSE(ParseProperty("hit").has_value());   // HitMelee/Ranged/Spell/HitRating
    EXPECT_FALSE(ParseProperty("crit").has_value());  // CritMelee/.../CritRating
    EXPECT_FALSE(ParseProperty("haste").has_value()); // HasteMelee/.../HasteRating
    EXPECT_FALSE(ParseProperty("spell").has_value()); // SpellPower/SpellPenetration
    EXPECT_FALSE(ParseProperty("arm").has_value());   // Armor/ArmorPenetrationRating
}

TEST(Properties, ParseRejectsInvalid)
{
    EXPECT_FALSE(ParseProperty("").has_value());
    EXPECT_FALSE(ParseProperty("2").has_value());    // gap in ITEM_MOD ids
    EXPECT_FALSE(ParseProperty("41").has_value());   // deprecated, not supported
    EXPECT_FALSE(ParseProperty("255").has_value());
    EXPECT_FALSE(ParseProperty("-1").has_value());
    EXPECT_FALSE(ParseProperty("st").has_value());   // too short for a prefix
    EXPECT_FALSE(ParseProperty("Stamina ").has_value());
    EXPECT_FALSE(ParseProperty("staminax").has_value());
    EXPECT_FALSE(ParseProperty("4x").has_value());
}

TEST(Properties, IsValidPropertyBounds)
{
    EXPECT_TRUE(IsValidProperty(0));
    EXPECT_TRUE(IsValidProperty(106));
    EXPECT_FALSE(IsValidProperty(2));
    EXPECT_FALSE(IsValidProperty(99));
    EXPECT_FALSE(IsValidProperty(107));
    EXPECT_FALSE(IsValidProperty(255));
}
