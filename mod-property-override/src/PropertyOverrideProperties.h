#ifndef MOD_PROPERTY_OVERRIDE_PROPERTIES_H
#define MOD_PROPERTY_OVERRIDE_PROPERTIES_H

#include "Define.h"
#include <optional>
#include <string_view>
#include <vector>

namespace mod_property_override
{

// Property ids reuse the game's own ITEM_MOD_* values (ItemTemplate.h), so
// stored rows speak native item-stat vocabulary and the apply path mirrors
// Player::_ApplyItemBonuses 1:1. Values >= 100 are module-custom extensions
// for stats items grant through dedicated template fields instead of
// ITEM_MOD (armor, resistances). Ids are persisted in
// item_property_override.property — never renumber, only append.
//
// Deliberately omitted: deprecated ITEM_MOD ids (spell healing/damage done
// 41/42, the *_TAKEN rating family) and percent/proc effects, which use
// different modifier channels and need their own design pass.
enum class Property : uint8
{
    Mana                   = 0,
    Health                 = 1,
    Agility                = 3,
    Strength               = 4,
    Intellect              = 5,
    Spirit                 = 6,
    Stamina                = 7,
    DefenseRating          = 12,
    DodgeRating            = 13,
    ParryRating            = 14,
    BlockRating            = 15,
    HitMeleeRating         = 16,
    HitRangedRating        = 17,
    HitSpellRating         = 18,
    CritMeleeRating        = 19,
    CritRangedRating       = 20,
    CritSpellRating        = 21,
    HasteMeleeRating       = 28,
    HasteRangedRating      = 29,
    HasteSpellRating       = 30,
    HitRating              = 31,
    CritRating             = 32,
    ResilienceRating       = 35,
    HasteRating            = 36,
    ExpertiseRating        = 37,
    AttackPower            = 38,
    RangedAttackPower      = 39,
    ManaRegen              = 43, // mana per 5s
    ArmorPenetrationRating = 44,
    SpellPower             = 45,
    HealthRegen            = 46, // health per 5s
    SpellPenetration       = 47,
    BlockValue             = 48,
    // Module-custom block (not ITEM_MOD; items grant these via dedicated
    // template fields):
    Armor                  = 100,
    HolyResistance         = 101,
    FireResistance         = 102,
    NatureResistance       = 103,
    FrostResistance        = 104,
    ShadowResistance       = 105,
    ArcaneResistance       = 106,
};

bool IsValidProperty(uint8 raw);

// Command/display token, e.g. "CritRating" — nullptr for invalid values.
char const* PropertyName(Property p);

// All supported properties, in id order (for tests and listings).
std::vector<Property> const& AllProperties();

// Accepts a case-insensitive property name ("spellpower"), a unique
// case-insensitive prefix of at least 3 chars ("sta" -> Stamina), or a
// numeric id ("45").
std::optional<Property> ParseProperty(std::string_view token);

} // namespace mod_property_override

#endif // MOD_PROPERTY_OVERRIDE_PROPERTIES_H
