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

bool MountProgressionMgr::AwardActiveIfType(Player* player, MountType required,
                                            uint32 amount)
{
    if (!player || amount == 0)
        return false;
    uint32 spellId = GetActiveMount(player);
    if (!spellId)
        return false;
    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry || entry->type != required)
        return false;
    MountProgress* mp = GetOrCreateProgress(player, spellId);
    if (!mp)
        return false;
    AwardXP(player, mp, amount, entry);
    return true;
}
void MountProgressionMgr::AnnounceActiveMount(Player* player,
                                              CatalogEntry const* entry)
{
    if (!player || !entry || !player->GetSession())
        return;
    MountProgress const* mp = GetProgress(player, entry->spellId);
    uint16 level = mp ? mp->level : 1;
    uint32 xp = mp ? mp->xp : 0;
    uint32 need = XPToNextLevel(entry->rarity, level);
    uint32 magnitude = ComputeBuffMagnitude(entry, level);

    ChatHandler ch(player->GetSession());
    if (need == 0)
        ch.PSendSysMessage(
            "|cff40ff80[Mount]|r Active: |cffffd100{}|r ({} {}) - lvl {}/{} (max) - "
            "XP: {} - |cffffd100{} +{}|r",
            entry->displayName, RarityName(entry->rarity),
            TypeName(entry->type), level, _maxLevel, XPSourceName(entry->type),
            BuffEffectLabel(entry->type), magnitude);
    else
        ch.PSendSysMessage(
            "|cff40ff80[Mount]|r Active: |cffffd100{}|r ({} {}) - lvl {}/{}, xp {}/{} - "
            "XP: {} - |cffffd100{} +{}|r",
            entry->displayName, RarityName(entry->rarity),
            TypeName(entry->type), level, _maxLevel, xp, need, XPSourceName(entry->type),
            BuffEffectLabel(entry->type), magnitude);
}
void MountProgressionMgr::AnnounceXPGain(Player* player, CatalogEntry const* entry,
                                         uint32 amount)
{
    if (!_announceXPGain || !player || !entry || !player->GetSession() || amount == 0)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    uint64 now = static_cast<uint64>(::time(nullptr));
    uint32 flushAmount = 0;

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        XpAnnounceState& state = _xpAnnounce[guidLow];
        state.accumXp += amount;

        if (state.lastFlushTime != 0 &&
            now - state.lastFlushTime < _announceXPGainIntervalSeconds)
            return;  // still within throttle window; keep accumulating

        flushAmount = state.accumXp;
        state.accumXp = 0;
        state.lastFlushTime = now;
    }

    if (flushAmount == 0)
        return;

    MountProgress const* mp = GetProgress(player, entry->spellId);
    uint16 level = mp ? mp->level : 1;
    uint32 magnitude = ComputeBuffMagnitude(entry, level);

    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cff7f7fffYour {} grows stronger. (+{} XP)|r — lvl {}/{}, "
        "|cffffd100{} +{}|r",
        entry->displayName, flushAmount, level, _maxLevel,
        BuffEffectLabel(entry->type), magnitude);
}
void MountProgressionMgr::AwardXP(Player* player, MountProgress* mp, uint32 amount,
                                  CatalogEntry const* entry)
{
    if (!mp || !entry || amount == 0)
        return;

    if (mp->level >= _maxLevel)
        return;

    uint32 leveledUpTo = 0;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        mp->xp += amount;
        mp->dirty = true;

        while (mp->level < _maxLevel)
        {
            uint32 need = XPToNextLevel(entry->rarity, mp->level);
            if (need == 0 || mp->xp < need)
                break;
            mp->xp -= need;
            mp->level += 1;
            leveledUpTo = mp->level;
        }

        if (mp->level >= _maxLevel)
            mp->xp = 0;
    }

    if (leveledUpTo && player->GetSession())
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff40ff40Your {} reached mount level {}!|r",
            entry->displayName, leveledUpTo);
    }
    else if (!leveledUpTo)
    {
        AnnounceXPGain(player, entry, amount);
    }

    if (leveledUpTo && GetActiveMount(player) == entry->spellId)
        ApplyMountBuff(player, entry, static_cast<uint16>(leveledUpTo));
}
bool MountProgressionMgr::AwardActiveMountXP(Player* player, uint32 amount)
{
    if (!player || amount == 0)
        return false;

    uint32 spellId = GetActiveMount(player);
    if (!spellId)
        return false;

    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry)
        return false;

    MountProgress* mp = GetOrCreateProgress(player, spellId);
    if (!mp)
        return false;

    AwardXP(player, mp, amount, entry);
    return true;
}
bool MountProgressionMgr::SetActiveMountLevel(Player* player, uint16 level)
{
    if (!player)
        return false;

    uint32 spellId = GetActiveMount(player);
    if (!spellId)
        return false;

    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry)
        return false;

    MountProgress* mp = GetOrCreateProgress(player, spellId);
    if (!mp)
        return false;

    if (level < 1)
        level = 1;
    if (level > _maxLevel)
        level = _maxLevel;

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        mp->level = level;
        mp->xp = 0;
        mp->dirty = true;
    }

    ApplyMountBuff(player, entry, level);
    return true;
}
double MountProgressionMgr::CurveFraction(uint16 level) const
{
    uint16 lvl = level < 1 ? 1 : level;
    if (lvl > _maxLevel)
        lvl = _maxLevel;

    switch (_buffLevelCurve)
    {
        case 1:  // stepped quartiles
        {
            double q = static_cast<double>(_maxLevel) / 4.0;
            if (lvl < static_cast<uint16>(std::ceil(q * 1)))
                return 0.25;
            if (lvl < static_cast<uint16>(std::ceil(q * 2)))
                return 0.50;
            if (lvl < static_cast<uint16>(std::ceil(q * 3)))
                return 0.75;
            return 1.0;
        }
        case 2:  // quadratic
        {
            double f = static_cast<double>(lvl) / _maxLevel;
            return f * f;
        }
        default:  // 0 — linear
            return static_cast<double>(lvl) / _maxLevel;
    }
}
uint32 MountProgressionMgr::ComputeBuffMagnitude(CatalogEntry const* entry,
                                                 uint16 level) const
{
    if (!entry)
        return 0;

    uint32 ceiling = _buffCeiling[static_cast<size_t>(entry->rarity)];
    double frac = CurveFraction(level);

    // Mode 0 (spec-literal % of base stat) is config-exposed but not yet
    // implemented — falls through to mode 1. See SLICE_3_PLAN.md §5.1.
    double magnitude = static_cast<double>(ceiling) * frac;
    if (magnitude < 0.0)
        magnitude = 0.0;
    return static_cast<uint32>(std::round(magnitude));
}
void MountProgressionMgr::ApplyMountBuff(Player* player,
                                         CatalogEntry const* entry,
                                         uint16 level)
{
    // Skip dead/ghost players: the carrier spell is non-passive and not flagged
    // ALLOW_DEAD_TARGET, so AddAura() returns null for them (which logged a misleading
    // "spell_dbc row missing or immune?" warning, e.g. from OnPlayerReleasedGhost). The buff
    // re-applies naturally on resurrect/remount.
    if (!_enabled || !player || !entry || !player->IsAlive())
        return;

    size_t typeIdx = static_cast<size_t>(entry->type);
    if (typeIdx >= static_cast<size_t>(MountType::MAX))
        return;

    uint32 carrierId = _carrierSpell[typeIdx];
    if (!carrierId)
        return;

    int32 amount = static_cast<int32>(ComputeBuffMagnitude(entry, level));

    RemoveMountBuff(player);

    Aura* aura = player->AddAura(carrierId, player);
    if (!aura)
    {
        LOG_WARN("module",
                 "mod-mount-progression: AddAura({}) failed for guid {} "
                 "(mount {}, type {}); spell_dbc row missing or immune?",
                 carrierId, player->GetGUID().GetCounter(),
                 entry->spellId, TypeName(entry->type));
        return;
    }

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (AuraEffect* eff = aura->GetEffect(i))
            eff->ChangeAmount(amount);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: applied carrier {} ({}) amount={} "
                 "to guid {} for mount {} lvl {}",
                 carrierId, TypeName(entry->type), amount,
                 player->GetGUID().GetCounter(), entry->spellId, level);
}
void MountProgressionMgr::RemoveMountBuff(Player* player)
{
    if (!player)
        return;
    for (size_t i = 0; i < static_cast<size_t>(MountType::MAX); ++i)
        if (_carrierSpell[i])
            player->RemoveAurasDueToSpell(_carrierSpell[i]);
}

}  // namespace mod_mount_progression
