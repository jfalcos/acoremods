#include "MountProgressionMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace mod_mount_progression
{

uint32 MountProgressionMgr::KillXpForRank(Creature const* killed) const
{
    if (!killed)
        return 0;
    if (killed->isWorldBoss())
        return _killXpBoss;
    if (killed->isElite())
        return _killXpElite;
    return _killXpNormal;
}
void MountProgressionMgr::OnCreatureKill(Player* player, Creature* killed)
{
    if (!_enabled || !player || !killed)
        return;

    uint32 activeSpell = GetActiveMount(player);
    if (!activeSpell)
        return;

    CatalogEntry const* entry = GetCatalogEntry(activeSpell);
    if (!entry || entry->type != MountType::Predator)
        return;

    int32 delta = static_cast<int32>(killed->GetLevel())
                - static_cast<int32>(player->GetLevel());
    if (delta < _killMinLevelDelta)
        return;

    uint32 xp = KillXpForRank(killed);
    if (!xp)
        return;

    MountProgress* mp = GetOrCreateProgress(player, activeSpell);
    if (!mp)
        return;

    AwardXP(player, mp, xp, entry);
}
CatalogEntry const* MountProgressionMgr::ActivateMount(Player* player, uint32 spellId)
{
    if (!_enabled || !player)
        return nullptr;
    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry)
        return nullptr;

    uint32 prev = GetActiveMount(player);
    SetActiveMount(player, spellId);
    MountProgress* mp = GetOrCreateProgress(player, spellId);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: guid {} active mount -> {} ({})",
                 player->GetGUID().GetCounter(), spellId, entry->displayName);

    ApplyMountBuff(player, entry, mp ? mp->level : 1);

    if (prev != spellId && _announceOnCast)
        AnnounceActiveMount(player, entry);
    return entry;
}
void MountProgressionMgr::OnSpellCast(Player* player, SpellInfo const* info)
{
    if (!_enabled || !player || !info)
        return;

    if (ActivateMount(player, info->Id))
        return;

    // Non-mount cast: feed arcane or mechanical depending on spell effect.
    bool isCrafting = false;
    for (auto const& eff : info->GetEffects())
    {
        if (eff.Effect == SPELL_EFFECT_CREATE_ITEM)
        {
            isCrafting = true;
            break;
        }
    }

    if (isCrafting)
        AwardActiveIfType(player, MountType::Mechanical, _craftXPPerCraft);
    else if (_castXPPerCast > 0)
        AwardActiveIfType(player, MountType::Arcane, _castXPPerCast);
}
void MountProgressionMgr::OnPlayerTick(Player* player, uint32 diff)
{
    if (!_enabled || !player)
        return;

    uint32 spellId = GetActiveMount(player);
    if (!spellId)
        return;
    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry || entry->type != MountType::Stamina)
        return;
    if (_yardsPerXP == 0)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    float curX = player->GetPositionX();
    float curY = player->GetPositionY();
    float curZ = player->GetPositionZ();

    uint32 xp = 0;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        PlayerTickState& state = _tickState[guidLow];
        state.accumMs += diff;
        if (state.accumMs < 1000)
            return;
        state.accumMs = 0;

        if (!state.hasLastPos)
        {
            state.lastX = curX;
            state.lastY = curY;
            state.lastZ = curZ;
            state.hasLastPos = true;
            return;
        }

        float dx = curX - state.lastX;
        float dy = curY - state.lastY;
        float dz = curZ - state.lastZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        state.lastX = curX;
        state.lastY = curY;
        state.lastZ = curZ;

        if (dist > static_cast<float>(_maxYardsPerTick))
            return;

        state.accumYards += dist;
        float perXP = static_cast<float>(_yardsPerXP);
        xp = static_cast<uint32>(state.accumYards / perXP);
        if (!xp)
            return;
        state.accumYards -= static_cast<float>(xp) * perXP;
    }

    MountProgress* mp = GetOrCreateProgress(player, spellId);
    if (mp)
        AwardXP(player, mp, xp, entry);
}
void MountProgressionMgr::OnPlayerLoot(Player* player, ObjectGuid lootguid)
{
    if (!_enabled || !player)
        return;
    if (!lootguid.IsAnyTypeGameObject())
        return;
    AwardActiveIfType(player, MountType::Mechanical, _gatherXPPerLoot);
}
void MountProgressionMgr::OnPlayerAreaChange(Player* player, uint32 /*oldArea*/,
                                             uint32 newArea)
{
    if (!_enabled || !player || !newArea)
        return;

    uint32 spellId = GetActiveMount(player);
    if (!spellId)
        return;
    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry || entry->type != MountType::Agility)
        return;
    if (_areaXPPerTransition == 0)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    bool isNew = false;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        auto& visited = _tickState[guidLow].visitedAreas;
        isNew = visited.insert(newArea).second;
    }
    if (!isNew)
        return;

    MountProgress* mp = GetOrCreateProgress(player, spellId);
    if (mp)
        AwardXP(player, mp, _areaXPPerTransition, entry);
}

}  // namespace mod_mount_progression
