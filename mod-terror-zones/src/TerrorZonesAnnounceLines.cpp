// Slice 1/7 — per-player rotation/event chat lines (tick, zone entry/leave,
// rotation ending/end warnings) and their formatting helpers.
#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"
#include "TerrorZonesTierMgr.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Group.h"
#include "GroupReference.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "Weather.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>

namespace mod_terror_zones
{

namespace
{
    // Render the signature axis roll for the announcement line.
    // Multiplier axes → "+NN% XP". Probability axes → "NN.N% tier-bump".
    // `value` is the raw rolled axis value (pre-cap-clamp already done).
    std::string FormatSignatureAxisForAnnounce(RewardAxis axis, float value)
    {
        char buf[64];
        if (IsProbabilityAxis(axis))
        {
            std::snprintf(buf, sizeof(buf), "%.1f%% %s",
                          value * 100.0f, AxisShortName(axis));
        }
        else
        {
            int pct = static_cast<int>(std::lround((value - 1.0f) * 100.0f));
            std::snprintf(buf, sizeof(buf), "%+d%% %s",
                          pct, AxisShortName(axis));
        }
        return std::string(buf);
    }

    // Build the "Tier N Flavor (+X% axis)" fragment for an announcement.
    std::string TierFlavorFragment(ActiveSlot const& slot,
                                    TierRollConfig const& cfg,
                                    uint64 tickAt)
    {
        std::string out = TierDisplayName(slot.tier);
        out += ' ';
        out += FlavorDisplayName(slot.flavor);
        FlavorBiasDef const& bias = FlavorBiasOf(slot.flavor);
        if (bias.primary < AXIS_COUNT)
        {
            float rolled = ComputeAxisRoll(tickAt, slot.slotIndex,
                                            slot.flavor, slot.tier,
                                            bias.primary, cfg);
            out += " (";
            out += FormatSignatureAxisForAnnounce(bias.primary, rolled);
            out += ')';
        }
        return out;
    }
}
void TerrorZonesMgr::SendTickLineTo(Player* player,
                                    std::string const& zoneName,
                                    uint32 remainingSec)
{
    if (!player || !player->GetSession())
        return;
    ActiveSlot slotCopy;
    bool have = false;
    uint64 tickAt = 0;
    auto rot = std::atomic_load_explicit(&_rotationSnap,
                                          std::memory_order_acquire);
    if (rot)
    {
        tickAt = rot->tickAt;
        for (ActiveSlot const& s : rot->slots)
            if (s.displayName == zoneName)
            {
                slotCopy = s;
                have = true;
                break;
            }
    }
    if (have && slotCopy.flavor != FLAVOR_NONE)
    {
        std::string frag = TierFlavorFragment(
            slotCopy, TerrorZonesTierMgr::Instance().GetTierConfig(), tickAt);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8040The winds shift. {} is empowered — {}, "
            "{} remaining.|r",
            zoneName, frag,
            FormatRemaining(remainingSec));
    }
    else
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8040The winds shift. {} is empowered — {} remaining.|r",
            zoneName, FormatRemaining(remainingSec));
}
void TerrorZonesMgr::SendEntryLineTo(Player* player,
                                     std::string const& zoneName,
                                     uint32 remainingSec)
{
    if (!player || !player->GetSession())
        return;
    ActiveSlot slotCopy;
    bool have = false;
    auto rot = std::atomic_load_explicit(&_rotationSnap,
                                          std::memory_order_acquire);
    if (rot)
    {
        for (ActiveSlot const& s : rot->slots)
            if (s.displayName == zoneName)
            {
                slotCopy = s;
                have = true;
                break;
            }
    }
    if (have && slotCopy.flavor != FLAVOR_NONE)
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8040You have entered an empowered zone: {} ({} {}). "
            "{} remaining.|r",
            zoneName, TierDisplayName(slotCopy.tier),
            FlavorDisplayName(slotCopy.flavor),
            FormatRemaining(remainingSec));
    else
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8040You have entered an empowered zone: {}. "
            "{} remaining.|r",
            zoneName, FormatRemaining(remainingSec));
}
void TerrorZonesMgr::SendZoneLeaveLineTo(Player* player,
                                           std::string const& zoneName)
{
    if (!player || !player->GetSession() || zoneName.empty())
        return;
    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cffff8040You have left the empowered zone of {}.|r", zoneName);
}
void TerrorZonesMgr::SendRotationEndLineFor(uint32 zoneId,
                                             std::string const& zoneName)
{
    if (zoneId == 0 || zoneName.empty())
        return;
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "|cffff8040The winds settle. %s's empowerment has ended.|r",
        zoneName.c_str());
    std::string line(buf);
    // Reuse the zone-scoped broadcast machinery, but apply the
    // per-player category gate inline.
    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;
        if (p->GetZoneId() != zoneId)
            continue;
        if (!TerrorZonesPlayerPrefsMgr::Instance().IsCategoryEnabledFor(p, ANNOUNCE_ROTATION_END))
            continue;
        ChatHandler(p->GetSession()).PSendSysMessage("{}", line);
    }
}
void TerrorZonesMgr::SendRotationEndingWarning(uint64 nextTickAt)
{
    uint64 now = static_cast<uint64>(::time(nullptr));
    uint32 leadSec = (nextTickAt > now)
        ? static_cast<uint32>(nextTickAt - now) : 0;
    char buf[192];
    if (leadSec >= 60)
    {
        uint32 mins = (leadSec + 30) / 60;
        std::snprintf(buf, sizeof(buf),
            "|cffff8040In %u minute%s, the winds shift. The current "
            "empowerments fade.|r",
            mins, (mins == 1 ? "" : "s"));
    }
    else
    {
        std::snprintf(buf, sizeof(buf),
            "|cffff8040In %u seconds, the winds shift. The current "
            "empowerments fade.|r",
            leadSec);
    }
    std::string line(buf);
    WorldSessionMgr::SessionMap const& sessions =
        sWorldSessionMgr->GetAllSessions();
    for (auto const& kv : sessions)
    {
        Player* p = RealPlayerFromSession(kv.second);
        if (!p)
            continue;
        if (!TerrorZonesPlayerPrefsMgr::Instance().IsCategoryEnabledFor(p, ANNOUNCE_ROTATION_ENDING))
            continue;
        ChatHandler(p->GetSession()).PSendSysMessage("{}", line);
    }
}
std::string TerrorZonesMgr::FormatRemaining(uint32 secs) const
{
    if (secs >= 3600)
    {
        uint32 h = secs / 3600;
        uint32 m = (secs % 3600) / 60;
        if (m == 0)
            return (h == 1) ? "1 hour" : std::to_string(h) + " hours";
        return std::to_string(h) + "h " + std::to_string(m) + "m";
    }
    if (secs >= 60)
    {
        uint32 m = secs / 60;
        return (m == 1) ? "1 minute" : std::to_string(m) + " minutes";
    }
    return std::to_string(secs) + "s";
}
uint32 TerrorZonesMgr::RemainingSeconds(uint64 now) const
{
    if (now == 0)
        now = static_cast<uint64>(::time(nullptr));
    auto rot = std::atomic_load_explicit(&_rotationSnap,
                                          std::memory_order_acquire);
    if (!rot || rot->expiresAt <= now)
        return 0;
    return static_cast<uint32>(rot->expiresAt - now);
}

}  // namespace mod_terror_zones
