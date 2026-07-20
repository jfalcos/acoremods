#include "ParagonPerks.h"

namespace mod_paragon::perks
{

bool IsPerkProperty(Property p)
{
    return PerkIndex(p).has_value();
}

std::optional<size_t> PerkIndex(Property p)
{
    for (size_t i = 0; i < PERK_SET.size(); ++i)
        if (PERK_SET[i] == p)
            return i;
    return std::nullopt;
}

uint32 CostForNextRank(PerkConfig const& cfg, uint32 currentRanks)
{
    if (currentRanks >= cfg.maxRanks)
        return 0;
    if (!cfg.costStepEvery)
        return 1;
    return 1 + currentRanks / cfg.costStepEvery;
}

uint32 TotalValue(PerkConfig const& cfg, size_t idx, uint32 ranks)
{
    if (idx >= PERK_SET.size())
        return 0;
    if (ranks > cfg.maxRanks)
        ranks = cfg.maxRanks;
    return cfg.valuePerRank[idx] * ranks;
}

} // namespace mod_paragon::perks
