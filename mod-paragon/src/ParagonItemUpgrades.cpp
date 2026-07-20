#include "ParagonItemUpgrades.h"

#include <algorithm>

namespace mod_paragon::upgrades
{

namespace
{
    using P = Property;

    // chunk = stat per purchase, sized so one purchase ≈ 5 weighted budget
    // points across the board (AP chunk 10 × weight 0.5 = 5, MP5 chunk
    // 2 × 2.5 = 5, ...). Weights live in the platform (PropertyWeight).
    struct Entry
    {
        P prop;
        Category cat;
        uint32 chunk;
        char const* label;
    };

    std::vector<Entry> const& Table()
    {
        static std::vector<Entry> const table =
        {
            { P::Mana,                   Category::PowerAndRegen,  100, "Maximum Mana" },
            { P::Health,                 Category::PowerAndRegen,  100, "Maximum Health" },
            { P::Agility,                Category::Primary,          5, "Agility" },
            { P::Strength,               Category::Primary,          5, "Strength" },
            { P::Intellect,              Category::Primary,          5, "Intellect" },
            { P::Spirit,                 Category::Primary,          5, "Spirit" },
            { P::Stamina,                Category::Primary,          5, "Stamina" },
            { P::DefenseRating,          Category::DefenseRatings,   5, "Defense Rating" },
            { P::DodgeRating,            Category::DefenseRatings,   5, "Dodge Rating" },
            { P::ParryRating,            Category::DefenseRatings,   5, "Parry Rating" },
            { P::BlockRating,            Category::DefenseRatings,   5, "Block Rating" },
            { P::HitMeleeRating,         Category::OffenseRatings,   5, "Hit Rating (melee only)" },
            { P::HitRangedRating,        Category::OffenseRatings,   5, "Hit Rating (ranged only)" },
            { P::HitSpellRating,         Category::OffenseRatings,   5, "Hit Rating (spells only)" },
            { P::CritMeleeRating,        Category::OffenseRatings,   5, "Crit Rating (melee only)" },
            { P::CritRangedRating,       Category::OffenseRatings,   5, "Crit Rating (ranged only)" },
            { P::CritSpellRating,        Category::OffenseRatings,   5, "Crit Rating (spells only)" },
            { P::HasteMeleeRating,       Category::OffenseRatings,   5, "Haste Rating (melee only)" },
            { P::HasteRangedRating,      Category::OffenseRatings,   5, "Haste Rating (ranged only)" },
            { P::HasteSpellRating,       Category::OffenseRatings,   5, "Haste Rating (spells only)" },
            // Tri-rating conveniences weigh triple (labels say so).
            { P::HitRating,              Category::OffenseRatings,   5, "Hit Rating (ALL, 3x budget)" },
            { P::CritRating,             Category::OffenseRatings,   5, "Crit Rating (ALL, 3x budget)" },
            { P::ResilienceRating,       Category::DefenseRatings,   5, "Resilience (ALL, 3x budget)" },
            { P::HasteRating,            Category::OffenseRatings,   5, "Haste Rating (ALL, 3x budget)" },
            { P::ExpertiseRating,        Category::OffenseRatings,   5, "Expertise Rating" },
            { P::AttackPower,            Category::PowerAndRegen,   10, "Attack Power" },
            { P::RangedAttackPower,      Category::PowerAndRegen,   10, "Ranged Attack Power" },
            { P::ManaRegen,              Category::PowerAndRegen,    2, "Mana per 5s" },
            { P::ArmorPenetrationRating, Category::OffenseRatings,   5, "Armor Penetration Rating" },
            { P::SpellPower,             Category::PowerAndRegen,    5, "Spell Power" },
            { P::HealthRegen,            Category::PowerAndRegen,    2, "Health per 5s" },
            { P::SpellPenetration,       Category::OffenseRatings,   5, "Spell Penetration" },
            { P::BlockValue,             Category::DefenseRatings,  10, "Block Value" },
            { P::Armor,                  Category::Defenses,        50, "Armor" },
            { P::HolyResistance,         Category::Defenses,         5, "Holy Resistance" },
            { P::FireResistance,         Category::Defenses,         5, "Fire Resistance" },
            { P::NatureResistance,       Category::Defenses,         5, "Nature Resistance" },
            { P::FrostResistance,        Category::Defenses,         5, "Frost Resistance" },
            { P::ShadowResistance,       Category::Defenses,         5, "Shadow Resistance" },
            { P::ArcaneResistance,       Category::Defenses,         5, "Arcane Resistance" },
        };
        return table;
    }
}

char const* CategoryName(Category c)
{
    switch (c)
    {
        case Category::Primary:        return "Primary stats";
        case Category::OffenseRatings: return "Offense ratings";
        case Category::DefenseRatings: return "Defense ratings";
        case Category::PowerAndRegen:  return "Power & regeneration";
        case Category::Defenses:       return "Armor & resistances";
        default:                       return "Unknown";
    }
}

std::vector<PropertyDef> const& AllUpgradeDefs()
{
    static std::vector<PropertyDef> const defs = []
    {
        std::vector<PropertyDef> v;
        v.reserve(Table().size());
        for (Entry const& e : Table())
            v.push_back({ e.prop, e.chunk, e.label });
        return v;
    }();
    return defs;
}

PropertyDef const* FindDef(Property p)
{
    for (PropertyDef const& def : AllUpgradeDefs())
        if (def.prop == p)
            return &def;
    return nullptr;
}

Category CategoryOf(Property p)
{
    for (Entry const& e : Table())
        if (e.prop == p)
            return e.cat;
    return Category::Max;
}

std::vector<PropertyDef> const& DefsInCategory(Category c)
{
    static std::array<std::vector<PropertyDef>,
                      static_cast<size_t>(Category::Max)> const byCat = []
    {
        std::array<std::vector<PropertyDef>,
                   static_cast<size_t>(Category::Max)> arr;
        for (Entry const& e : Table())
            arr[static_cast<size_t>(e.cat)].push_back({ e.prop, e.chunk, e.label });
        return arr;
    }();
    static std::vector<PropertyDef> const empty;
    size_t idx = static_cast<size_t>(c);
    return idx < byCat.size() ? byCat[idx] : empty;
}

float UpgradeBudget(UpgradeConfig const& cfg, uint32 quality, uint32 itemLevel)
{
    return mod_property_override::NativeBudget(quality, itemLevel) * cfg.budgetPercent;
}

uint32 CostForNextChunk(float spentFraction)
{
    spentFraction = std::clamp(spentFraction, 0.f, 1.f);
    return 1 + static_cast<uint32>(spentFraction * 3.f);
}

} // namespace mod_paragon::upgrades
