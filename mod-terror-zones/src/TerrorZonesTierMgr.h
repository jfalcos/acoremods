#ifndef MOD_TERROR_ZONES_TIER_MGR_H
#define MOD_TERROR_ZONES_TIER_MGR_H

// Full-decomposition split of TerrorZonesMgr's Slice 5 (empowerment
// tiers) subsystem. Covers the tier rarity weights rolled at rotation
// tick and the per-axis reward-roll math (RollAxis / TierRollConfig).
//
// `SetActiveTier` (the GM-facing force-set) stays on TerrorZonesMgr --
// it mutates the live rotation + republishes the Core/Combat snapshots,
// which is Core-owned state, not Tier-owned config. This manager only
// owns the tier weights + roll-math config and the read-only accessors
// built on top of them.

#include "TerrorZonesMgr.h"

namespace mod_terror_zones
{

class TerrorZonesTierMgr
{
public:
    static TerrorZonesTierMgr& Instance();

    void LoadConfig();

    bool IsTierEnabled() const
    { return TerrorZonesMgr::Instance().IsEnabled() && _tierEnabled; }

    // Weighted tier roll for a fresh rotation slot. Called from
    // RunRotation with the tick's seeded RNG so tier selection is
    // deterministic across resumes, same as zone/flavor selection.
    Tier RollTierWeighted(IRng& rng) const;

    // Evaluate one axis roll for a given slot (uses the slot's persisted
    // flavor + tier + slotIndex together with the rotation's tickAt --
    // read via TerrorZonesMgr::GetRotationSnapshot() -- so every call
    // for this rotation returns the same number).
    float RollAxis(ActiveSlot const& slot, RewardAxis axis) const;

    // Read-only access to the loaded tier config (used by /zones output
    // + announcement helpers to render rolled values in the chat line).
    TierRollConfig const& GetTierConfig() const { return _tierCfg; }

private:
    TerrorZonesTierMgr() = default;

    bool _tierEnabled = true;
    uint32 _tierWeights[TIER_MAX] = {40, 30, 20, 8, 2};
    TierRollConfig _tierCfg{};
};

}  // namespace mod_terror_zones

#endif  // MOD_TERROR_ZONES_TIER_MGR_H
