#include "TerrorZonesPlayerPrefsMgr.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Player.h"

namespace mod_terror_zones
{

TerrorZonesPlayerPrefsMgr& TerrorZonesPlayerPrefsMgr::Instance()
{
    static TerrorZonesPlayerPrefsMgr inst;
    return inst;
}

void TerrorZonesPlayerPrefsMgr::LoadConfig()
{
    _announceServerWide = sConfigMgr->GetOption<bool>(
        "TerrorZones.Announce.ServerWide", true);
    bool announceZoneEntry = sConfigMgr->GetOption<bool>(
        "TerrorZones.Announce.ZoneEntry", true);

    // Slice 7 — per-category global mask. Each bit reads its own knob;
    // the legacy `Announce.ServerWide` is OR'd into the RotationTick bit
    // and `Announce.ZoneEntry` is OR'd into the ZoneEntry bit so existing
    // operator configs keep working.
    auto readCategoryBit = [](char const* key, bool dflt) -> bool {
        return sConfigMgr->GetOption<bool>(key, dflt);
    };
    uint8 mask = 0;
    if (readCategoryBit("TerrorZones.Announce.RotationTick", true)
        || _announceServerWide)
        mask |= AnnounceCategoryBit(ANNOUNCE_ROTATION_TICK);
    if (readCategoryBit("TerrorZones.Announce.RotationEnding", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_ROTATION_ENDING);
    if (readCategoryBit("TerrorZones.Announce.RotationEnd", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_ROTATION_END);
    if (readCategoryBit("TerrorZones.Announce.ZoneEntry", true)
        || announceZoneEntry)
        mask |= AnnounceCategoryBit(ANNOUNCE_ZONE_ENTRY);
    if (readCategoryBit("TerrorZones.Announce.ZoneLeave", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_ZONE_LEAVE);
    if (readCategoryBit("TerrorZones.Announce.EventStart", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_EVENT_START);
    if (readCategoryBit("TerrorZones.Announce.EventEnding", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_EVENT_ENDING);
    if (readCategoryBit("TerrorZones.Announce.EventEnd", true))
        mask |= AnnounceCategoryBit(ANNOUNCE_EVENT_END);
    _announceCategoryGlobal = mask;
}

void TerrorZonesPlayerPrefsMgr::LoadPlayerPref(Player* player)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();
    bool enabled = _announceServerWide;
    uint8 categories = ANNOUNCE_CATEGORY_ALL;

    QueryResult r = CharacterDatabase.Query(
        "SELECT announce_enabled, announce_categories "
        "FROM character_terror_zones_prefs WHERE guid = {}", guidLow);
    if (r)
    {
        Field* f = r->Fetch();
        enabled = f[0].Get<uint8>() != 0;
        categories = f[1].Get<uint8>();
    }

    PlayerPref& pref = _prefs[guidLow];
    pref.announceEnabled = enabled;
    pref.dirty = false;
    pref.loaded = true;
    pref.announceCategories = categories;
    // Don't reset lastEmpoweredZone* here — login path may have
    // already populated it before this DB round-trip completes.
}

void TerrorZonesPlayerPrefsMgr::FlushPlayerPref(Player* player)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();

    auto it = _prefs.find(guidLow);
    if (it == _prefs.end() || !it->second.dirty)
        return;
    bool enabled = it->second.announceEnabled;
    uint8 categories = it->second.announceCategories;
    it->second.dirty = false;

    CharacterDatabase.Execute(
        "INSERT INTO character_terror_zones_prefs "
        "  (guid, announce_enabled, announce_categories) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "  announce_enabled = VALUES(announce_enabled), "
        "  announce_categories = VALUES(announce_categories)",
        guidLow, enabled ? 1 : 0, static_cast<uint32>(categories));
}

void TerrorZonesPlayerPrefsMgr::UnloadPlayerPref(ObjectGuid guid)
{
    uint32 guidLow = guid.GetCounter();
    _prefs.erase(guidLow);
}

bool TerrorZonesPlayerPrefsMgr::IsAnnounceEnabled(Player const* player) const
{
    if (!player)
        return _announceServerWide;
    uint32 guidLow = player->GetGUID().GetCounter();
    auto it = _prefs.find(guidLow);
    if (it == _prefs.end() || !it->second.loaded)
        return _announceServerWide;
    return it->second.announceEnabled;
}

bool TerrorZonesPlayerPrefsMgr::IsCategoryEnabledFor(
    Player const* player, AnnounceCategory cat) const
{
    if (!player)
        return false;
    uint32 guidLow = player->GetGUID().GetCounter();
    bool master;
    uint8 mask;
    auto it = _prefs.find(guidLow);
    if (it == _prefs.end() || !it->second.loaded)
    {
        // Pref row hasn't loaded yet (rare — happens between
        // OnPlayerLogin and LoadPlayerPref completing). Default
        // to the server-wide master + all-on mask so a line
        // in this window doesn't get spuriously suppressed.
        master = _announceServerWide;
        mask = ANNOUNCE_CATEGORY_ALL;
    }
    else
    {
        master = it->second.announceEnabled;
        mask = it->second.announceCategories;
    }
    return IsCategoryAnnouncementAllowed(cat, _announceCategoryGlobal,
                                          master, mask);
}

void TerrorZonesPlayerPrefsMgr::SetAnnounceEnabled(Player* player, bool enabled)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();
    PlayerPref& pref = _prefs[guidLow];
    if (pref.loaded && pref.announceEnabled == enabled)
        return;
    pref.announceEnabled = enabled;
    pref.dirty = true;
    pref.loaded = true;
}

uint8 TerrorZonesPlayerPrefsMgr::GetAnnounceCategories(Player const* player) const
{
    if (!player)
        return ANNOUNCE_CATEGORY_ALL;
    uint32 guidLow = player->GetGUID().GetCounter();
    auto it = _prefs.find(guidLow);
    if (it == _prefs.end() || !it->second.loaded)
        return ANNOUNCE_CATEGORY_ALL;
    return it->second.announceCategories;
}

void TerrorZonesPlayerPrefsMgr::SetAnnounceCategories(Player* player, uint8 mask)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();
    PlayerPref& pref = _prefs[guidLow];
    if (pref.loaded && pref.announceCategories == mask)
        return;
    pref.announceCategories = mask;
    pref.dirty = true;
    pref.loaded = true;
}

TerrorZonesPlayerPrefsMgr::LastEmpoweredZone
TerrorZonesPlayerPrefsMgr::GetLastEmpoweredZone(uint32 guidLow) const
{
    auto it = _prefs.find(guidLow);
    if (it == _prefs.end())
        return {};
    LastEmpoweredZone out;
    out.zoneId = it->second.lastEmpoweredZoneId;
    out.zoneName = it->second.lastEmpoweredZoneName;
    return out;
}

void TerrorZonesPlayerPrefsMgr::SetLastEmpoweredZone(
    uint32 guidLow, uint32 zoneId, std::string const& zoneName)
{
    PlayerPref& pref = _prefs[guidLow];
    pref.lastEmpoweredZoneId = zoneId;
    pref.lastEmpoweredZoneName = zoneName;
}

}  // namespace mod_terror_zones
