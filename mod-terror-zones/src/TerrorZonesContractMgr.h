#ifndef MOD_TERROR_ZONES_CONTRACT_MGR_H
#define MOD_TERROR_ZONES_CONTRACT_MGR_H

// Full-decomposition split of TerrorZonesMgr's Slice 10 Pass 2 per-TZ
// "contract" credit + mailed reward subsystem into its own manager. Owns
// its own config (TerrorZones.Contract.*) and its own state (the
// session-only progress-message cache) -- the DB table
// (character_terror_zones_progress) is the actual reward source of
// truth, so this manager is otherwise stateless between calls.
//
// Unlike TerrorZonesPlayerPrefsMgr, this subsystem is NOT independent of
// the rest of the module -- accruing/mailing contract credit inherently
// needs to know which zone is empowered at what tier (TerrorZonesMgr),
// whether a kill is scaling-eligible or an event boss (TerrorZonesMgr),
// and whether a class-drop gear cell is populated (TerrorZonesMgr). This
// manager reaches back into TerrorZonesMgr through its public API only --
// nothing here touches TerrorZonesMgr's private state, and nothing in
// TerrorZonesMgr reaches into this manager's private state.

#include "Define.h"
#include <cstdint>
#include <unordered_map>

class Creature;
class Player;

namespace mod_terror_zones
{

class TerrorZonesContractMgr
{
public:
    static TerrorZonesContractMgr& Instance();

    void LoadConfig();

    bool IsEnabled() const;

    // Accrue contract credit for an eligible kill in an empowered zone.
    // Write-through: upserts the per-(guid, rotation, zone) row in
    // character_terror_zones_progress (capped), capturing the killer's
    // level / class / spec / zone tier for the offline mail-out. Splits
    // across the killer's group members standing in the same zone.
    // Called from OnPlayerCreatureKill.
    void AccrueContractCredit(Creature* killed, Player* killer);

    // Settle + mail every unmailed contract whose rotation has ended
    // (tick_at < beforeTickAt). Gold (+ optional archetype gear when
    // credit clears the threshold) scaled by stored credit/tier, mailed
    // to the character by guid (offline-safe), then the settled rows are
    // deleted. Called from RunRotation once the new rotation is live.
    void MailContractRewards(uint64_t beforeTickAt);

    // Current banked contract credit for (guid, zone) in the active
    // rotation — used by `.zones` to show progress. DB-backed read.
    uint32 GetContractCreditFor(uint32 guidLow, uint32 zoneId) const;

    // New rotation begins — reset the best-effort progress-message cache
    // (the DB row is the reward source of truth; this only drives the
    // player-facing chat lines). Called from RunRotationContinued.
    void ClearMsgCreditCache() { _contractMsgCredit.clear(); }

private:
    TerrorZonesContractMgr() = default;

    bool   _contractEnabled              = true;
    uint32 _contractCreditPerKillDivisor = 1000;  // credit = mobMaxHp / this
    uint32 _contractCreditCapPerZone     = 3000;  // per player per zone/rotation
    uint32 _contractGoldPerCreditCopper  = 30;    // gold = credit * this * tierMult
    uint32 _contractGoldCapCopper        = 2000000;   // 200g ceiling per mail
    uint32 _contractGearCreditThreshold  = 1500;  // min credit to roll mailed gear
    // Per-tier gold multiplier for the mailed lump sum (index 0 = TIER_NONE).
    // Sized for TIER_NONE..TIER_5 (see TerrorZonesMgr.h's Tier enum).
    float  _contractTierGoldMult[6] = {1.0f, 1.0f, 1.3f, 1.7f, 2.2f, 3.0f};
    // Player-facing progress feedback.
    bool   _contractAnnounceProgress   = true;
    uint32 _contractProgressEveryCredit = 100;  // periodic "credit banked" line

    // Messaging-only running credit per (guidLow<<32 | zoneId) for the
    // current session/rotation. Drives the zone-entry / gear-threshold /
    // periodic chat lines (the DB row is the reward source of truth; this
    // is best-effort and reset each rotation tick). Not persisted.
    std::unordered_map<uint64_t, uint32> _contractMsgCredit;
};

}  // namespace mod_terror_zones

#endif  // MOD_TERROR_ZONES_CONTRACT_MGR_H
