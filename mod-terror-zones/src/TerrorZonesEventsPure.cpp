// Slice 6 — dynamic events: pure, unit-tested helper functions
// (event-type selection, window math). See TerrorZonesEvents*.cpp
// for the stateful event lifecycle that uses these.
#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace mod_terror_zones
{

EventType SelectEventType(EventScheduleConfig const& cfg, IRng& rng)
{
    uint64 total = 0;
    for (uint32 i = 1; i <= EVENT_TYPE_MAX; ++i)
        if (cfg.typeEnabled[i])
            total += cfg.typeWeights[i];
    if (total == 0)
        return EVENT_NONE;

    constexpr uint32 RESOLUTION = 1'000'000;
    uint32 roll = rng.NextUInt(RESOLUTION);
    double target = (static_cast<double>(roll) / RESOLUTION)
                  * static_cast<double>(total);
    double acc = 0.0;
    for (uint32 i = 1; i <= EVENT_TYPE_MAX; ++i)
    {
        if (!cfg.typeEnabled[i])
            continue;
        acc += static_cast<double>(cfg.typeWeights[i]);
        if (target < acc)
            return static_cast<EventType>(i);
    }
    // Defensive fallback — the loop should always pick.
    for (uint32 i = EVENT_TYPE_MAX; i >= 1; --i)
        if (cfg.typeEnabled[i] && cfg.typeWeights[i] > 0)
            return static_cast<EventType>(i);
    return EVENT_NONE;
}

bool ShouldFireSecondEvent(float chance, IRng& rng)
{
    if (chance <= 0.0f)
        return false;
    if (chance >= 1.0f)
        return true;
    constexpr uint32 RESOLUTION = 1'000'000;
    uint32 roll = rng.NextUInt(RESOLUTION);
    float t = static_cast<float>(roll) / static_cast<float>(RESOLUTION);
    return t < chance;
}

bool WithinEventWindow(uint64 now, uint64 starts, uint64 ends)
{
    if (ends <= starts)
        return false;
    return now >= starts && now < ends;
}

void PickSubregionAnchor(float ax, float ay, float r, IRng& rng,
                         float& outX, float& outY)
{
    if (r <= 0.0f)
    {
        outX = ax;
        outY = ay;
        return;
    }
    constexpr uint32 RESOLUTION = 1'000'000;
    float u1 = static_cast<float>(rng.NextUInt(RESOLUTION))
             / static_cast<float>(RESOLUTION);
    float u2 = static_cast<float>(rng.NextUInt(RESOLUTION))
             / static_cast<float>(RESOLUTION);
    float theta = u1 * 2.0f * static_cast<float>(M_PI);
    float radius = std::sqrt(u2) * r;   // uniform over the disc
    outX = ax + radius * std::cos(theta);
    outY = ay + radius * std::sin(theta);
}

char const* EventTypeDisplayName(EventType type)
{
    switch (type)
    {
        case EVENT_WORLD_BOSS:       return "World Boss";
        case EVENT_TREASURE_CARAVAN: return "Treasure Caravan";
        case EVENT_RARE_NODE_SURGE:  return "Rare Node Surge";
        case EVENT_CHAMPION_GROUNDS: return "Champion Hunting Grounds";
        case EVENT_NONE:
        default:                     return "—";
    }
}

char const* EventTypeCommandKey(EventType type)
{
    switch (type)
    {
        case EVENT_WORLD_BOSS:       return "worldboss";
        case EVENT_TREASURE_CARAVAN: return "caravan";
        case EVENT_RARE_NODE_SURGE:  return "nodes";
        case EVENT_CHAMPION_GROUNDS: return "champion";
        default:                     return "none";
    }
}

EventType ParseEventTypeKey(char const* key)
{
    if (!key || !*key)
        return EVENT_NONE;
    std::string s(key);
    for (char& c : s) c = static_cast<char>(std::tolower(c));
    if (s == "worldboss" || s == "boss")        return EVENT_WORLD_BOSS;
    if (s == "nodes" || s == "nodesurge"
        || s == "node_surge" || s == "surge")   return EVENT_RARE_NODE_SURGE;
    if (s == "caravan" || s == "treasure")      return EVENT_TREASURE_CARAVAN;
    if (s == "champion" || s == "grounds")      return EVENT_CHAMPION_GROUNDS;
    return EVENT_NONE;
}

char const* EventStateDisplayName(EventState state)
{
    switch (state)
    {
        case EVENT_STATE_PENDING: return "pending";
        case EVENT_STATE_ACTIVE:  return "active";
        case EVENT_STATE_EXPIRED: return "expired";
        default:                  return "?";
    }
}

}  // namespace mod_terror_zones
