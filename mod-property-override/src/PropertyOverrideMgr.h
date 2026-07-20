#ifndef MOD_PROPERTY_OVERRIDE_MGR_H
#define MOD_PROPERTY_OVERRIDE_MGR_H

#include "ObjectGuid.h"
#include "PropertyOverrideProperties.h"
#include <string>
#include <unordered_map>
#include <vector>

class Item;
class Player;

namespace mod_property_override
{

struct OverrideRow
{
    uint8 property;
    int32 value;
    uint64 expiry; // unix seconds, 0 = permanent

    bool IsExpired(uint64 now) const { return expiry != 0 && now >= expiry; }
};

struct AppliedRow
{
    uint8 property;
    float value;
};

// Snapshot of what was actually applied for one equipped item. Unapply
// always subtracts this snapshot (never re-reads the override cache), so
// removal stays symmetric even if rows were edited or expired in between.
struct AppliedItem
{
    std::vector<AppliedRow> rows;
    uint64 minExpiry = 0; // earliest non-zero expiry among rows, 0 = none
};

using ItemOverrideMap = std::unordered_map<ObjectGuid::LowType, std::vector<OverrideRow>>;
using AppliedMap = std::unordered_map<ObjectGuid::LowType, AppliedItem>;

// Pure diff between "what is applied" and "what should be applied".
// Executor order matters: unapply first, then prune expired rows, then apply.
// An item whose earliest expiry passed while equipped appears in both
// `unapply` and `apply` (old snapshot off, remaining rows back on).
struct SyncActions
{
    std::vector<ObjectGuid::LowType> unapply;
    std::vector<ObjectGuid::LowType> apply;
    std::vector<ObjectGuid::LowType> expired; // prune these items' expired rows
};

// `equipped` = item guids currently in equipment slots, not broken, present
// in `overrides` (the Player-dependent filtering happens in the caller).
SyncActions ComputeSyncActions(ItemOverrideMap const& overrides,
                               AppliedMap const& applied,
                               std::vector<ObjectGuid::LowType> const& equipped,
                               uint64 now);

// World-thread only (all player/item hooks run there) — lock-free by design,
// following mod-living-world's threading convention.
class PropertyOverrideMgr
{
public:
    static PropertyOverrideMgr& Instance();

    void LoadConfig();
    bool IsEnabled() const { return _enabled; }
    bool IsDebug() const { return _debug; }

    // One-time startup sweep: drop rows whose item_instance no longer exists.
    void StartupCleanup();

    void LoadPlayer(Player* player);
    void UnloadPlayer(ObjectGuid::LowType playerGuid);

    // The only code path that applies/unapplies stats. Idempotent —
    // callable from any hook, any order, any number of times.
    void Sync(Player* player);

    void OnPlayerTick(Player* player, uint32 diffMs);

    // Fires from AllItemScript::CanItemRemove while the item is still in its
    // slot: unapply + drop from cache before the engine removes it.
    void HandleItemDestroyed(Player* player, Item* item);

    // GM/purchase API. The item may be anywhere (equipped or not).
    bool AddOverride(Player* owner, Item* item, Property prop, int32 value, uint32 durationSecs);
    bool ClearOverrides(Player* owner, Item* item);

    // Non-expired rows for an item of this (online) owner. Empty if none.
    std::vector<OverrideRow> GetActiveOverrides(Player* owner, ObjectGuid::LowType itemGuid) const;

    // Whisper-to-self LANG_ADDON packet to this player's client.
    void SendAddonMessage(Player* player, std::string const& payload) const;

private:
    PropertyOverrideMgr() = default;

    struct PlayerState
    {
        ItemOverrideMap overrides; // by item guid, loaded at login
        AppliedMap applied;        // by item guid, snapshots of live stats
        uint32 reconcileTimerMs = 0;
    };

    void ApplyItem(Player* player, PlayerState& state, ObjectGuid::LowType itemGuid, uint64 now);
    void UnapplyItem(Player* player, PlayerState& state, ObjectGuid::LowType itemGuid);
    void PruneExpiredRows(PlayerState& state, ObjectGuid::LowType itemGuid, uint64 now);
    static void ApplyStat(Player* player, uint8 property, float value, bool apply);

    std::unordered_map<ObjectGuid::LowType, PlayerState> _players; // by player guid
    bool _enabled = true;
    bool _debug = false;
};

} // namespace mod_property_override

#endif // MOD_PROPERTY_OVERRIDE_MGR_H
