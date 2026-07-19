// Slice 5 — empowerment tiers. Tier rarity weights + per-axis reward
// roll math, split out of TerrorZonesMgr as its own manager.
#include "TerrorZonesTierMgr.h"

#include "Config.h"
#include "Log.h"

#include <algorithm>
#include <string>

namespace mod_terror_zones
{

TerrorZonesTierMgr& TerrorZonesTierMgr::Instance()
{
    static TerrorZonesTierMgr inst;
    return inst;
}

void TerrorZonesTierMgr::LoadConfig()
{
    _tierEnabled = sConfigMgr->GetOption<bool>("TerrorZones.Tier.Enable", true);

    // Tier rarity weights. Default 40/30/20/8/2 makes T5 a ~2% rotation
    // (~1 per day on a 1h cycle). Admins can zero weights to exclude
    // specific tiers (e.g., a "no T5" server sets the T5 weight to 0).
    _tierWeights[0] = sConfigMgr->GetOption<uint32>("TerrorZones.Tier.Rarity.T1", 40);
    _tierWeights[1] = sConfigMgr->GetOption<uint32>("TerrorZones.Tier.Rarity.T2", 30);
    _tierWeights[2] = sConfigMgr->GetOption<uint32>("TerrorZones.Tier.Rarity.T3", 20);
    _tierWeights[3] = sConfigMgr->GetOption<uint32>("TerrorZones.Tier.Rarity.T4", 8);
    _tierWeights[4] = sConfigMgr->GetOption<uint32>("TerrorZones.Tier.Rarity.T5", 2);

    // Per-tier per-axis (base, spread). Defaults mirror
    // TERROR_ZONES_SPEC.md v0.2 §4.4 table.
    struct AxisDefault { float base; float spread; };
    static AxisDefault const kDefaults[TIER_MAX][AXIS_COUNT] = {
        // T1
        {{1.20f, 0.10f}, {1.20f, 0.10f}, {0.025f, 0.25f}, {1.15f, 0.10f}, {0.010f, 0.30f}},
        // T2
        {{1.35f, 0.12f}, {1.35f, 0.12f}, {0.040f, 0.25f}, {1.30f, 0.12f}, {0.020f, 0.30f}},
        // T3
        {{1.55f, 0.15f}, {1.55f, 0.15f}, {0.065f, 0.30f}, {1.50f, 0.15f}, {0.040f, 0.35f}},
        // T4
        {{1.85f, 0.18f}, {1.85f, 0.18f}, {0.095f, 0.30f}, {1.75f, 0.18f}, {0.075f, 0.35f}},
        // T5
        {{2.30f, 0.20f}, {2.30f, 0.20f}, {0.140f, 0.30f}, {2.10f, 0.20f}, {0.140f, 0.40f}},
    };
    static char const* const kTierPrefix[TIER_MAX] = {
        "TerrorZones.Tier.T1.",
        "TerrorZones.Tier.T2.",
        "TerrorZones.Tier.T3.",
        "TerrorZones.Tier.T4.",
        "TerrorZones.Tier.T5.",
    };
    static char const* const kAxisSuffix[AXIS_COUNT] = {
        "Xp.",
        "Gold.",
        "TierBump.",
        "Gathering.",
        "Uniques.",
    };

    for (uint32 t = 0; t < TIER_MAX; ++t)
    {
        for (uint32 a = 0; a < AXIS_COUNT; ++a)
        {
            std::string baseKey = std::string(kTierPrefix[t]) + kAxisSuffix[a] + "Base";
            std::string spreadKey = std::string(kTierPrefix[t]) + kAxisSuffix[a] + "Spread";
            float base = sConfigMgr->GetOption<float>(baseKey,
                kDefaults[t][a].base);
            float spread = sConfigMgr->GetOption<float>(spreadKey,
                kDefaults[t][a].spread);
            if (base < 0.0f)   base = 0.0f;
            if (spread < 0.0f) spread = 0.0f;
            if (spread > 0.99f) spread = 0.99f;  // floor can't go negative
            _tierCfg.tierTable[t][a].base   = base;
            _tierCfg.tierTable[t][a].spread = spread;
        }
    }

    _tierCfg.signatureFloorBump = sConfigMgr->GetOption<float>(
        "TerrorZones.Tier.Bias.SignatureFloorBump", 0.15f);
    _tierCfg.signatureCeilingBump = sConfigMgr->GetOption<float>(
        "TerrorZones.Tier.Bias.SignatureCeilingBump", 0.30f);
    _tierCfg.secondaryFloorBump = sConfigMgr->GetOption<float>(
        "TerrorZones.Tier.Bias.SecondaryFloorBump", 0.05f);

    _tierCfg.axisCaps[AXIS_XP]        = sConfigMgr->GetOption<float>("TerrorZones.Tier.Cap.Xp",        5.0f);
    _tierCfg.axisCaps[AXIS_GOLD]      = sConfigMgr->GetOption<float>("TerrorZones.Tier.Cap.Gold",      8.0f);
    _tierCfg.axisCaps[AXIS_TIER_BUMP] = sConfigMgr->GetOption<float>("TerrorZones.Tier.Cap.TierBump",  0.50f);
    _tierCfg.axisCaps[AXIS_GATHERING] = sConfigMgr->GetOption<float>("TerrorZones.Tier.Cap.Gathering", 4.0f);
    _tierCfg.axisCaps[AXIS_UNIQUES]   = sConfigMgr->GetOption<float>("TerrorZones.Tier.Cap.Uniques",   0.50f);
    for (uint32 a = 0; a < AXIS_COUNT; ++a)
        if (_tierCfg.axisCaps[a] < 0.0f)
            _tierCfg.axisCaps[a] = 0.0f;

    LOG_INFO("module",
             "mod-terror-zones: tier enable={} "
             "rarity=[t1={},t2={},t3={},t4={},t5={}] "
             "biases=[sig_floor={:.2f},sig_ceil={:.2f},sec_floor={:.2f}] "
             "caps=[xp={:.2f},gold={:.2f},tier_bump={:.2f},"
             "gather={:.2f},uniques={:.2f}] "
             "t3_bases=[xp={:.2f},gold={:.2f},tier_bump={:.3f},"
             "gather={:.2f},uniques={:.3f}]",
             _tierEnabled,
             _tierWeights[0], _tierWeights[1], _tierWeights[2],
             _tierWeights[3], _tierWeights[4],
             _tierCfg.signatureFloorBump, _tierCfg.signatureCeilingBump,
             _tierCfg.secondaryFloorBump,
             _tierCfg.axisCaps[AXIS_XP],
             _tierCfg.axisCaps[AXIS_GOLD],
             _tierCfg.axisCaps[AXIS_TIER_BUMP],
             _tierCfg.axisCaps[AXIS_GATHERING],
             _tierCfg.axisCaps[AXIS_UNIQUES],
             _tierCfg.tierTable[2][AXIS_XP].base,
             _tierCfg.tierTable[2][AXIS_GOLD].base,
             _tierCfg.tierTable[2][AXIS_TIER_BUMP].base,
             _tierCfg.tierTable[2][AXIS_GATHERING].base,
             _tierCfg.tierTable[2][AXIS_UNIQUES].base);
}

Tier TerrorZonesTierMgr::RollTierWeighted(IRng& rng) const
{
    return SelectTier(_tierWeights, rng);
}

float TerrorZonesTierMgr::RollAxis(ActiveSlot const& slot, RewardAxis axis) const
{
    auto rot = TerrorZonesMgr::Instance().GetRotationSnapshot();
    uint64 tickAt = rot ? rot->tickAt : 0;
    return ComputeAxisRoll(tickAt, slot.slotIndex, slot.flavor,
                            slot.tier, axis, _tierCfg);
}

} // namespace mod_terror_zones
