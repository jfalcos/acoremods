#include "PropertyOverrideItemization.h"

#include <array>

namespace mod_property_override
{

float PropertyWeight(Property p)
{
    switch (p)
    {
        case Property::Mana:
        case Property::Health:
            return 0.05f;
        case Property::AttackPower:
        case Property::RangedAttackPower:
        case Property::BlockValue:
            return 0.5f;
        case Property::ManaRegen:
        case Property::HealthRegen:
            return 2.5f;
        // Tri-rating conveniences: three rating mods per point.
        case Property::HitRating:
        case Property::CritRating:
        case Property::ResilienceRating:
        case Property::HasteRating:
            return 3.0f;
        case Property::Armor:
            return 0.1f;
        case Property::Agility:
        case Property::Strength:
        case Property::Intellect:
        case Property::Spirit:
        case Property::Stamina:
        case Property::DefenseRating:
        case Property::DodgeRating:
        case Property::ParryRating:
        case Property::BlockRating:
        case Property::HitMeleeRating:
        case Property::HitRangedRating:
        case Property::HitSpellRating:
        case Property::CritMeleeRating:
        case Property::CritRangedRating:
        case Property::CritSpellRating:
        case Property::HasteMeleeRating:
        case Property::HasteRangedRating:
        case Property::HasteSpellRating:
        case Property::ExpertiseRating:
        case Property::ArmorPenetrationRating:
        case Property::SpellPower:
        case Property::SpellPenetration:
        case Property::HolyResistance:
        case Property::FireResistance:
        case Property::NatureResistance:
        case Property::FrostResistance:
        case Property::ShadowResistance:
        case Property::ArcaneResistance:
            return 1.0f;
        default:
            return 0.f; // outside the vocabulary
    }
}

namespace
{
    // Fitted through-origin slopes: weighted budget points per ItemLevel
    // (derivation in the header).
    constexpr std::array<float, 7> BUDGET_PER_ILVL =
    {
        0.20f, // 0 poor
        0.30f, // 1 common
        0.41f, // 2 uncommon (fit: 0.4111, n=4243)
        0.58f, // 3 rare     (fit: 0.5794, n=4666)
        1.20f, // 4 epic     (fit: 1.1999, n=9315)
        1.50f, // 5 legendary
        1.50f, // 6 artifact
    };
}

float NativeBudget(uint32 quality, uint32 itemLevel)
{
    if (quality >= BUDGET_PER_ILVL.size())
        quality = BUDGET_PER_ILVL.size() - 1;
    return BUDGET_PER_ILVL[quality] * static_cast<float>(itemLevel);
}

float BudgetSpent(std::vector<OverrideRow> const& rows, std::string_view source)
{
    float spent = 0.f;
    for (OverrideRow const& row : rows)
        if (row.source == source)
            spent += PropertyWeight(static_cast<Property>(row.property)) *
                     static_cast<float>(row.value);
    return spent;
}

} // namespace mod_property_override
