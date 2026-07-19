#ifndef MOD_TERROR_ZONES_TELEPORT_MGR_H
#define MOD_TERROR_ZONES_TELEPORT_MGR_H

// Full-decomposition split of TerrorZonesMgr's teleport-unlock subsystem
// into its own manager. Cumulative (not rotation-scoped) per-tier credit
// earned fighting in a zone empowered at a given tier unlocks that tier
// as a permanent teleport destination, selectable from a single
// multi-tier beacon item's gossip menu (TerrorZonesTeleportItem.cpp).
//
// Owns its own config (TerrorZones.Teleport.*) and its own state
// (_tierProgress, the in-session working copy of
// character_terror_zones_tier_progress). Reaches back into TerrorZonesMgr
// (GetActiveRotation, GetPool, IsDebug) through its public API only, the
// same way TerrorZonesContractMgr does -- and TerrorZonesContractMgr
// calls into this manager's AccrueTierTeleportCredit the same way any
// other file in the module would.

#include "Define.h"
#include "ObjectGuid.h"
#include <array>
#include <unordered_map>

class Player;

namespace mod_terror_zones
{

class TerrorZonesTeleportMgr
{
public:
    static TerrorZonesTeleportMgr& Instance();

    void LoadConfig();

    bool IsEnabled() const;

    void LoadTierTeleportProgress(Player* player);
    void UnloadTierTeleportProgress(ObjectGuid guid);

    // Cumulative (not rotation-scoped) credit toward the Tier-`tier`
    // teleport unlock. Called alongside AccrueContractCredit for the same
    // kill/credit — independent bucket, no rotation cap, never resets. On
    // first crossing the configured threshold, grants the single
    // multi-tier beacon item (if not already owned) and announces it.
    void AccrueTierTeleportCredit(Player* player, uint8 tier, uint32 addCredit);

    // Whether `player` has unlocked the Tier-`tier` teleport destination.
    // In-session working copy of character_terror_zones_tier_progress —
    // used by the beacon item's gossip menu to decide which tiers to list.
    bool IsTierUnlockedFor(Player const* player, uint8 tier) const;

    // Teleports `player` to whichever pool zone is currently empowered at
    // `tier` in the active rotation. Sends a chat error and returns false
    // if no slot is at that tier right now, or the zone has no configured
    // landing point yet (tp_map unset). Called from the beacon item's
    // gossip-select handler.
    bool TeleportPlayerToTier(Player* player, uint8 tier);

private:
    TerrorZonesTeleportMgr() = default;

    bool   _teleportEnabled = true;
    // Index by tier (1-5); [0] unused. Sized for TIER_NONE..TIER_5.
    uint32 _teleportUnlockThreshold[6] = {0, 800, 1200, 1800, 2600, 4000};
    // Single multi-tier beacon item (entry id), granted once on first
    // unlock. Its gossip menu lists whichever tiers _tierProgress marks
    // unlocked — no per-tier item/spell needed.
    uint32 _teleportItemEntry = 0;

    struct TierProgress
    {
        uint32 lifetimeCredit = 0;
        bool   unlocked = false;
    };
    // guidLow -> per-tier progress ([0] unused), loaded whole at login,
    // erased at logout. character_terror_zones_tier_progress is the
    // source of truth; this is just the in-session working copy.
    std::unordered_map<uint32, std::array<TierProgress, 6>> _tierProgress;
};

}  // namespace mod_terror_zones

#endif  // MOD_TERROR_ZONES_TELEPORT_MGR_H
