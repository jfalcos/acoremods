#ifndef MOD_TERROR_ZONES_PLAYER_PREFS_MGR_H
#define MOD_TERROR_ZONES_PLAYER_PREFS_MGR_H

// Full-decomposition split of TerrorZonesMgr's per-player announcement
// preference subsystem into its own independent manager. Owns its own
// config (TerrorZones.Announce.*) and its own state (_prefs) -- no
// dependency on TerrorZonesMgr, and nothing in TerrorZonesMgr reaches
// into this manager's private state. TerrorZonesMgr's rotation/zone-cross
// hooks (OnPlayerLogin, OnPlayerUpdateZone) call this manager's public
// API the same way any other file in the module does.

// AnnounceCategory (and IsCategoryAnnouncementAllowed, the pure gating
// function this manager delegates to) live in TerrorZonesMgr.h alongside
// the module's other shared value types (Flavor, Tier, EventType, ...).
// This does not create a circular dependency: TerrorZonesMgr.h has no
// need to include this header back.
#include "TerrorZonesMgr.h"
#include <unordered_map>

class Player;

namespace mod_terror_zones
{

class TerrorZonesPlayerPrefsMgr
{
public:
    static TerrorZonesPlayerPrefsMgr& Instance();

    void LoadConfig();

    void LoadPlayerPref(Player* player);
    void FlushPlayerPref(Player* player);
    void UnloadPlayerPref(ObjectGuid guid);

    bool IsAnnounceEnabled(Player const* player) const;
    void SetAnnounceEnabled(Player* player, bool enabled);

    uint8 GetAnnounceCategories(Player const* player) const;
    void  SetAnnounceCategories(Player* player, uint8 mask);
    uint8 GetGlobalAnnounceCategoryMask() const
    { return _announceCategoryGlobal; }

    // Three-input gating composition (master + global + per-player bit).
    bool IsCategoryEnabledFor(Player const* player, AnnounceCategory cat) const;

    // Last-empowered-zone cache. TerrorZonesMgr's OnPlayerUpdateZone hook
    // needs the player's *previous* empowered zone to fire a zone-leave
    // line -- AC's UpdateZone hook only carries the new zone -- so it
    // reads/writes this cache via these accessors rather than reaching
    // into a shared map.
    struct LastEmpoweredZone
    {
        uint32 zoneId = 0;
        std::string zoneName;
    };
    LastEmpoweredZone GetLastEmpoweredZone(uint32 guidLow) const;
    void SetLastEmpoweredZone(uint32 guidLow, uint32 zoneId,
                               std::string const& zoneName);

private:
    TerrorZonesPlayerPrefsMgr() = default;

    struct PlayerPref
    {
        bool announceEnabled = true;
        bool dirty = false;
        bool loaded = false;
        // Slice 7 — per-category bitmask. Defaults to 0xFF when the row
        // is missing (never opted out of any category).
        uint8 announceCategories = ANNOUNCE_CATEGORY_ALL;
        // Slice 7 — last empowered zone the player was tracked in. Used
        // to fire the zone-leave line on UpdateZone since AC's
        // `OnPlayerUpdateZone(player, newZone, newArea)` doesn't carry
        // the prior zone. In-memory only (no DB persistence) — the
        // entry path on login re-establishes it.
        uint32 lastEmpoweredZoneId = 0;
        std::string lastEmpoweredZoneName;
    };
    std::unordered_map<uint32 /*guidLow*/, PlayerPref> _prefs;

    bool _announceServerWide = true;
    uint8 _announceCategoryGlobal = ANNOUNCE_CATEGORY_ALL;
};

}  // namespace mod_terror_zones

#endif  // MOD_TERROR_ZONES_PLAYER_PREFS_MGR_H
