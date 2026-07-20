#include "PropertyOverrideProperties.h"

#include <array>
#include <cctype>
#include <charconv>

namespace mod_property_override
{

namespace
{
    struct PropertyDef
    {
        Property id;
        char const* name;
    };

    // Kept in id order. Names are single tokens (commands split on spaces).
    constexpr std::array<PropertyDef, 40> DEFS =
    {{
        { Property::Mana,                   "Mana" },
        { Property::Health,                 "Health" },
        { Property::Agility,                "Agility" },
        { Property::Strength,               "Strength" },
        { Property::Intellect,              "Intellect" },
        { Property::Spirit,                 "Spirit" },
        { Property::Stamina,                "Stamina" },
        { Property::DefenseRating,          "DefenseRating" },
        { Property::DodgeRating,            "DodgeRating" },
        { Property::ParryRating,            "ParryRating" },
        { Property::BlockRating,            "BlockRating" },
        { Property::HitMeleeRating,         "HitMeleeRating" },
        { Property::HitRangedRating,        "HitRangedRating" },
        { Property::HitSpellRating,         "HitSpellRating" },
        { Property::CritMeleeRating,        "CritMeleeRating" },
        { Property::CritRangedRating,       "CritRangedRating" },
        { Property::CritSpellRating,        "CritSpellRating" },
        { Property::HasteMeleeRating,       "HasteMeleeRating" },
        { Property::HasteRangedRating,      "HasteRangedRating" },
        { Property::HasteSpellRating,       "HasteSpellRating" },
        { Property::HitRating,              "HitRating" },
        { Property::CritRating,             "CritRating" },
        { Property::ResilienceRating,       "ResilienceRating" },
        { Property::HasteRating,            "HasteRating" },
        { Property::ExpertiseRating,        "ExpertiseRating" },
        { Property::AttackPower,            "AttackPower" },
        { Property::RangedAttackPower,      "RangedAttackPower" },
        { Property::ManaRegen,              "ManaRegen" },
        { Property::ArmorPenetrationRating, "ArmorPenetrationRating" },
        { Property::SpellPower,             "SpellPower" },
        { Property::HealthRegen,            "HealthRegen" },
        { Property::SpellPenetration,       "SpellPenetration" },
        { Property::BlockValue,             "BlockValue" },
        { Property::Armor,                  "Armor" },
        { Property::HolyResistance,         "HolyResistance" },
        { Property::FireResistance,         "FireResistance" },
        { Property::NatureResistance,       "NatureResistance" },
        { Property::FrostResistance,        "FrostResistance" },
        { Property::ShadowResistance,       "ShadowResistance" },
        { Property::ArcaneResistance,       "ArcaneResistance" },
    }};

    bool EqualsNoCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }
}

bool IsValidProperty(uint8 raw)
{
    for (PropertyDef const& def : DEFS)
        if (static_cast<uint8>(def.id) == raw)
            return true;
    return false;
}

char const* PropertyName(Property p)
{
    for (PropertyDef const& def : DEFS)
        if (def.id == p)
            return def.name;
    return nullptr;
}

std::vector<Property> const& AllProperties()
{
    static std::vector<Property> const all = []
    {
        std::vector<Property> v;
        v.reserve(DEFS.size());
        for (PropertyDef const& def : DEFS)
            v.push_back(def.id);
        return v;
    }();
    return all;
}

std::optional<Property> ParseProperty(std::string_view token)
{
    if (token.empty())
        return std::nullopt;

    for (PropertyDef const& def : DEFS)
        if (EqualsNoCase(token, def.name))
            return def.id;

    // Unique prefix of at least 3 chars ("sta" -> Stamina, "spellpo" ->
    // SpellPower). Ambiguous prefixes ("hit", "crit") are rejected.
    if (token.size() >= 3)
    {
        std::optional<Property> match;
        for (PropertyDef const& def : DEFS)
        {
            std::string_view name = def.name;
            if (token.size() <= name.size() &&
                EqualsNoCase(token, name.substr(0, token.size())))
            {
                if (match)
                    return std::nullopt; // ambiguous
                match = def.id;
            }
        }
        if (match)
            return match;
    }

    uint32 raw = 0;
    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), raw);
    if (ec == std::errc() && ptr == token.data() + token.size() &&
        raw <= 255 && IsValidProperty(static_cast<uint8>(raw)))
        return static_cast<Property>(raw);

    return std::nullopt;
}

} // namespace mod_property_override
