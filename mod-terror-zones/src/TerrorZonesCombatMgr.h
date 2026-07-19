#ifndef MOD_TERROR_ZONES_COMBAT_MGR_H
#define MOD_TERROR_ZONES_COMBAT_MGR_H

// Full-decomposition split of TerrorZonesMgr's Slice 2 (creature level
// scaling) and Slice 8 (combat HP/damage difficulty) subsystems into
// their own manager. These two slices were already implemented together
// in TerrorZonesScaling.cpp and share the same eligibility predicate, so
// they're one manager here too rather than two.
//
// This is the module's hot-path core: OnUnitDealDamage fires on every
// damage dispatch (melee + spell + DoT) during combat, and MUST stay
// lock-free. It reads TerrorZonesMgr::GetCombatHotSnapshot() /
// GetRotationSnapshot() -- cheap atomic-load + shared_ptr-copy
// accessors, deliberately NOT the by-value GetActiveRotation() other
// callers use, to avoid cloning per-slot std::string on the hot path.
// TerrorZonesMgr still owns building/publishing those snapshots
// (PublishCombatHot/PublishRotationSnap) since they're assembled from
// rotation + event-boss state that core/events own.

#include "Define.h"
#include <unordered_map>
#include <unordered_set>

class Creature;
class Player;
class Unit;

namespace mod_terror_zones
{

enum Tier : uint8;

class TerrorZonesCombatMgr
{
public:
    static TerrorZonesCombatMgr& Instance();

    void LoadConfig();

    // --- Slice 2: creature level scaling ---
    bool IsScalingEnabled() const;

    // Returns 0 when the zone isn't empowered (caller leaves baseline
    // alone). Otherwise returns max(pool.level_max, highest-online-in-zone)
    // — the target level mobs in that zone should scale to.
    uint8 ComputeTargetLevel(uint32 zoneId) const;

    // Eligibility predicate per SLICE_2_PLAN §6. Safe to call from the
    // OnBeforeCreatureSelectLevel hook (creature's position is already
    // set at that point, so faction and friendly checks are valid).
    bool IsScalingEligible(Creature const* creature) const;

    // OnBeforeCreatureSelectLevel entry point — mutates `level` in place
    // when scaling applies.
    void OnBeforeCreatureSelectLevel(Creature const* creature, uint8& level);

    // Tick-edge zone walks. `edgeOn=true` when a zone newly becomes
    // empowered; `edgeOn=false` when a zone leaves the empowered set.
    void WalkZoneRescale(uint32 zoneId, bool edgeOn, bool force = false);

    // Rate-limited wrapper for the per-player-zone-crossing trigger
    // (OnPlayerUpdateZone) only. A player riding a transport whose path
    // clips a zone's boundary can flip GetZoneId() back and forth many
    // times in quick succession, and each flip is a genuine, distinct
    // zone-crossing as far as the hook is concerned — there's no single
    // "event" to dedupe by identity, only by how recently the same zone
    // was last walked. Skips the (expensive, whole-zone) walk if `zoneId`
    // was already force-rescaled within the last `kZoneRescaleDebounceSec`
    // seconds; otherwise walks and stamps the time. Deliberately NOT
    // folded into `WalkZoneRescale` itself — the rotation-tick edge walk
    // and the GM `.zones settier` retune must never be silently dropped
    // just because an unrelated zone-crossing rescale happened moments
    // earlier.
    void WalkZoneRescaleDebounced(uint32 zoneId, bool edgeOn, bool force);

    // --- Slice 8: combat difficulty ---
    bool IsCombatEnabled() const;

    // Post-SelectLevel HP mult entry point. Reads zone empowerment +
    // eligibility + event-boss status, multiplies the creature's
    // computed MaxHealth by `ComputeCombatHpMult(...)`. No-op when
    // the creature isn't eligible or the zone isn't empowered.
    void OnAfterCreatureSelectLevel(Creature* creature);

    // Outgoing-damage mult entry point. Fires from the UnitScript
    // OnDamage hook. Same predicate as the HP path — eligible
    // attacker, empowered zone, event-boss bonus if indexed.
    void OnUnitDealDamage(Unit* attacker, Unit* victim, uint32& damage);

    // Slice 10 Pass 3 — engage-time group HP scaling (Model C). Fires
    // from the same OnDamage hook for player→creature hits. On the first
    // hit on a full-HP eligible empowered mob (tracked per rotation), if
    // the attacker is grouped, multiplies the mob's max HP by
    // GroupHpFactor(...) over the group's combined live EHP. Per-hit
    // damage is unchanged. No-op for solo players / event bosses.
    void ApplyGroupHpScaling(Unit* attacker, Unit* victim);

    // Release a killed creature's guid from the group-scale tracking set
    // so it re-scales on respawn within the same rotation. Called from
    // OnPlayerCreatureKill.
    void OnCreatureKilled(Creature* killed);

    // New rotation begins — release every creature's group-scale
    // tracking. Called from RunRotationContinued.
    void ClearGroupScaledGuids() { _groupScaledGuids.clear(); }

    // Read-only getters the `.zones` command uses to render the
    // Difficulty sub-line without having to reach into private state.
    float GetCombatHpMult() const { return _combatHpMult; }
    float GetCombatDamageMult() const { return _combatDamageMult; }
    float GetTierHpBonus(Tier t) const;
    float GetTierDamageBonus(Tier t) const;
    float GetEventBossHpUplift() const { return _eventBossHpMultUplift; }
    float GetEventBossDamageUplift() const { return _eventBossDamageMultUplift; }
    uint32 GetEliteDensityPerMille(Tier t) const;
    float GetEliteHpUplift() const { return _eliteHpMultUplift; }
    float GetEliteDamageUplift() const { return _eliteDamageMultUplift; }

private:
    TerrorZonesCombatMgr() = default;

    // Slice 2 — scaling.
    bool _scalingEnabled = true;
    bool _scalingRescaleOnTick = true;
    bool _scalingSkipWorldBosses = true;
    bool _scalingSkipFriendly = false;
    // Player-level aggregate used to pick an empowered zone's mob level.
    // false → median of real players in the zone (default); true → max.
    bool _scalingUseMaxLevel = false;
    std::unordered_set<uint32> _scalingNeverEntries;

    // Last force-rescale timestamp per zone, for WalkZoneRescaleDebounced.
    // World-thread-only, same as the rest of this class's state.
    static constexpr uint64 kZoneRescaleDebounceSec = 300;  // 5 minutes
    std::unordered_map<uint32, uint64> _zoneRescaleDebounceAt;

    // Slice 8 — combat difficulty. Applied post-SelectLevel (HP) +
    // at outgoing damage dispatch. Per-tier HP bonus composes
    // multiplicatively on top of the base mult; damage tier bonus is
    // flat 1.0 by default per plan §2.2 but tunable. Event-boss
    // uplift stacks on top when the attacker's GUID is an event boss.
    bool  _combatEnabled   = true;
    float _combatHpMult    = 2.0f;
    float _combatDamageMult = 1.3f;
    // Sized for TIER_NONE..TIER_5 (see TerrorZonesMgr.h's Tier enum).
    float _tierHpBonus[6]     = {1.0f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    float _tierDamageBonus[6] = {1.0f, 1.0f, 1.0f,  1.0f, 1.0f,  1.0f};
    float _eventBossHpMultUplift     = 4.0f;
    float _eventBossDamageMultUplift = 1.75f;

    // Slice 8b — elite density per tier. Per-mille values (0..1000)
    // so the hot path can roll an integer mod-1000 instead of a
    // float comparison. Default ladder: T1/T2 = 0 (no promotion),
    // T3 = 150 (15%), T4 = 250 (25%), T5 = 400 (40%).
    uint32 _eliteDensityPerMille[6] = {0, 0, 0, 150, 250, 400};
    float  _eliteHpMultUplift     = 1.5f;
    float  _eliteDamageMultUplift = 1.3f;

    // Slice 10 Pass 3 — engage-time group HP scaling (Model C).
    bool   _groupScalingEnabled   = true;
    float  _groupScalingDampen    = 0.75f;  // per extra-member-EHP weight
    float  _groupScalingMaxFactor = 8.0f;   // HP-factor ceiling
    // Creatures already group-scaled this rotation (raw ObjectGuid). A
    // mob scales at most once per engagement; killing it releases the
    // guid (OnCreatureKilled) so a respawn re-scales. Cleared each
    // rotation tick. World-thread-only (combat + kill + tick all there).
    std::unordered_set<uint64> _groupScaledGuids;
};

}  // namespace mod_terror_zones

#endif  // MOD_TERROR_ZONES_COMBAT_MGR_H
