#include "ParagonItemUpgrades.h"

#include <algorithm>

namespace mod_paragon::upgrades
{

namespace
{
    using P = Property;

    // chunk = stat per purchase; weight = itemization cost per stat point.
    // Chunks are sized so one purchase ≈ 5 weighted budget points across the
    // board (AP chunk 10 × weight 0.5 = 5, MP5 chunk 2 × 2.5 = 5, ...).
    struct Entry
    {
        P prop;
        Category cat;
        uint32 chunk;
        float weight;
        char const* label;
    };

    std::vector<Entry> const& Table()
    {
        static std::vector<Entry> const table =
        {
            { P::Mana,                   Category::PowerAndRegen,  100, 0.05f, "Maximum Mana" },
            { P::Health,                 Category::PowerAndRegen,  100, 0.05f, "Maximum Health" },
            { P::Agility,                Category::Primary,          5, 1.0f,  "Agility" },
            { P::Strength,               Category::Primary,          5, 1.0f,  "Strength" },
            { P::Intellect,              Category::Primary,          5, 1.0f,  "Intellect" },
            { P::Spirit,                 Category::Primary,          5, 1.0f,  "Spirit" },
            { P::Stamina,                Category::Primary,          5, 1.0f,  "Stamina" },
            { P::DefenseRating,          Category::DefenseRatings,   5, 1.0f,  "Defense Rating" },
            { P::DodgeRating,            Category::DefenseRatings,   5, 1.0f,  "Dodge Rating" },
            { P::ParryRating,            Category::DefenseRatings,   5, 1.0f,  "Parry Rating" },
            { P::BlockRating,            Category::DefenseRatings,   5, 1.0f,  "Block Rating" },
            { P::HitMeleeRating,         Category::OffenseRatings,   5, 1.0f,  "Hit Rating (melee only)" },
            { P::HitRangedRating,        Category::OffenseRatings,   5, 1.0f,  "Hit Rating (ranged only)" },
            { P::HitSpellRating,         Category::OffenseRatings,   5, 1.0f,  "Hit Rating (spells only)" },
            { P::CritMeleeRating,        Category::OffenseRatings,   5, 1.0f,  "Crit Rating (melee only)" },
            { P::CritRangedRating,       Category::OffenseRatings,   5, 1.0f,  "Crit Rating (ranged only)" },
            { P::CritSpellRating,        Category::OffenseRatings,   5, 1.0f,  "Crit Rating (spells only)" },
            { P::HasteMeleeRating,       Category::OffenseRatings,   5, 1.0f,  "Haste Rating (melee only)" },
            { P::HasteRangedRating,      Category::OffenseRatings,   5, 1.0f,  "Haste Rating (ranged only)" },
            { P::HasteSpellRating,       Category::OffenseRatings,   5, 1.0f,  "Haste Rating (spells only)" },
            // Tri-rating conveniences apply three server-side rating mods per
            // point, so they weigh triple (labels say so).
            { P::HitRating,              Category::OffenseRatings,   5, 3.0f,  "Hit Rating (ALL, 3x budget)" },
            { P::CritRating,             Category::OffenseRatings,   5, 3.0f,  "Crit Rating (ALL, 3x budget)" },
            { P::ResilienceRating,       Category::DefenseRatings,   5, 3.0f,  "Resilience (ALL, 3x budget)" },
            { P::HasteRating,            Category::OffenseRatings,   5, 3.0f,  "Haste Rating (ALL, 3x budget)" },
            { P::ExpertiseRating,        Category::OffenseRatings,   5, 1.0f,  "Expertise Rating" },
            { P::AttackPower,            Category::PowerAndRegen,   10, 0.5f,  "Attack Power" },
            { P::RangedAttackPower,      Category::PowerAndRegen,   10, 0.5f,  "Ranged Attack Power" },
            { P::ManaRegen,              Category::PowerAndRegen,    2, 2.5f,  "Mana per 5s" },
            { P::ArmorPenetrationRating, Category::OffenseRatings,   5, 1.0f,  "Armor Penetration Rating" },
            { P::SpellPower,             Category::PowerAndRegen,    5, 1.0f,  "Spell Power" },
            { P::HealthRegen,            Category::PowerAndRegen,    2, 2.5f,  "Health per 5s" },
            { P::SpellPenetration,       Category::OffenseRatings,   5, 1.0f,  "Spell Penetration" },
            { P::BlockValue,             Category::DefenseRatings,  10, 0.5f,  "Block Value" },
            { P::Armor,                  Category::Defenses,        50, 0.1f,  "Armor" },
            { P::HolyResistance,         Category::Defenses,         5, 1.0f,  "Holy Resistance" },
            { P::FireResistance,         Category::Defenses,         5, 1.0f,  "Fire Resistance" },
            { P::NatureResistance,       Category::Defenses,         5, 1.0f,  "Nature Resistance" },
            { P::FrostResistance,        Category::Defenses,         5, 1.0f,  "Frost Resistance" },
            { P::ShadowResistance,       Category::Defenses,         5, 1.0f,  "Shadow Resistance" },
            { P::ArcaneResistance,       Category::Defenses,         5, 1.0f,  "Arcane Resistance" },
        };
        return table;
    }

    // Fitted through-origin slopes: weighted budget points per ItemLevel,
    // from the acore_world item corpus (see header). Qualities without
    // native stat data get conservative values.
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
            v.push_back({ e.prop, e.chunk, e.weight, e.label });
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
            arr[static_cast<size_t>(e.cat)].push_back({ e.prop, e.chunk, e.weight, e.label });
        return arr;
    }();
    static std::vector<PropertyDef> const empty;
    size_t idx = static_cast<size_t>(c);
    return idx < byCat.size() ? byCat[idx] : empty;
}

float NativeBudget(uint32 quality, uint32 itemLevel)
{
    if (quality >= BUDGET_PER_ILVL.size())
        quality = BUDGET_PER_ILVL.size() - 1;
    return BUDGET_PER_ILVL[quality] * static_cast<float>(itemLevel);
}

float UpgradeBudget(UpgradeConfig const& cfg, uint32 quality, uint32 itemLevel)
{
    return NativeBudget(quality, itemLevel) * cfg.budgetPercent;
}

float BudgetSpent(std::vector<OverrideRow> const& rows)
{
    float spent = 0.f;
    for (OverrideRow const& row : rows)
        if (PropertyDef const* def = FindDef(static_cast<Property>(row.property)))
            spent += def->weight * static_cast<float>(row.value);
    return spent;
}

uint32 CostForNextChunk(float spentFraction)
{
    spentFraction = std::clamp(spentFraction, 0.f, 1.f);
    return 1 + static_cast<uint32>(spentFraction * 3.f);
}

} // namespace mod_paragon::upgrades
