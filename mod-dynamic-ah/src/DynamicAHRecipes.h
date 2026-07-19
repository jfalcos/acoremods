#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <array>

namespace ModDynamicAH
{

struct SkillStats
{
    uint16_t min = 65535;
    uint16_t max = 0;
    uint32_t count = 0;
    std::array<uint32_t, 6> bins{ {0,0,0,0,0,0} }; // [0-75), [75-150), [150-225), [225-300), [300-375), [375-450+]
};

// Encapsulated index of reagent -> recipe skill distribution.
class RecipeUsageIndex
{
public:
    static RecipeUsageIndex& Instance();

    // Ensure the index is built (idempotent).
    void EnsureBuilt();

    // Highest profession difficulty among recipes using this item as reagent.
    uint16_t MaxSkillForReagent(uint32_t itemId) const;

    // Effective difficulty (median blended toward max when usage skews late-game).
    uint16_t EffectiveSkillForReagent(uint32_t itemId) const;

    // Every reagent item used by a recipe belonging to `skillLineId` (e.g. SKILL_TAILORING)
    // whose minimum skill requirement falls in the same tier bucket as `skillValue`. Derived
    // directly from live spell/skill-line data, so it covers every profession completely and
    // self-heals as recipes/items change — unlike a hand-curated table. Complements (doesn't
    // replace) any hand-curated material tables the caller also uses.
    std::vector<uint32_t> const &ItemsForSkillLineAtSkill(uint32_t skillLineId, uint16_t skillValue) const;

private:
    void Build(); // one-time
    static uint8_t BucketOf(uint16_t req);
    bool _built = false;
    std::unordered_map<uint32_t, SkillStats> _stats;
    std::unordered_map<uint64_t, std::vector<uint32_t>> _bySkillLineBucket; // (skillLine<<8)|bucket -> items
};

} // namespace ModDynamicAH
