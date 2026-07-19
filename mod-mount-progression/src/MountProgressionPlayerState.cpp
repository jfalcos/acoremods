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

void MountProgressionMgr::LoadPlayerState(Player* player)
{
    if (!player)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(
        "SELECT spell_id, mount_level, mount_xp "
        "FROM character_mount_progress WHERE guid = {}",
        guidLow);

    std::lock_guard<std::mutex> lock(_stateMutex);
    auto& bucket = _progress[guidLow];
    bucket.clear();

    if (result)
    {
        do
        {
            Field* f = result->Fetch();
            MountProgress mp;
            mp.spellId = f[0].Get<uint32>();
            mp.level   = f[1].Get<uint16>();
            mp.xp      = f[2].Get<uint32>();
            mp.dirty   = false;
            bucket.emplace(mp.spellId, mp);
        } while (result->NextRow());
    }

    if (_debug)
        LOG_INFO("module", "mod-mount-progression: loaded {} mount progress rows "
                 "for guid {}", bucket.size(), guidLow);
}
void MountProgressionMgr::SavePlayerState(Player* player)
{
    if (!player)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_stateMutex);
    auto it = _progress.find(guidLow);
    if (it == _progress.end())
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    uint64 now = static_cast<uint64>(::time(nullptr));
    uint32 saved = 0;

    for (auto& kv : it->second)
    {
        uint32 spellId = kv.first;
        MountProgress& mp = kv.second;
        if (!mp.dirty)
            continue;

        trans->Append(
            "INSERT INTO character_mount_progress "
            "(guid, spell_id, mount_level, mount_xp, updated_at) "
            "VALUES ({}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE "
            "mount_level = VALUES(mount_level), "
            "mount_xp = VALUES(mount_xp), "
            "updated_at = VALUES(updated_at)",
            guidLow, spellId, mp.level, mp.xp, now);

        mp.dirty = false;
        ++saved;
    }

    if (saved)
        CharacterDatabase.CommitTransaction(trans);

    if (_debug && saved)
        LOG_INFO("module", "mod-mount-progression: saved {} dirty rows for guid {}",
                 saved, guidLow);
}
void MountProgressionMgr::UnloadPlayerState(ObjectGuid guid)
{
    uint32 guidLow = guid.GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    _progress.erase(guidLow);
    _active.erase(guidLow);
    _tickState.erase(guidLow);
    _xpAnnounce.erase(guidLow);
}
MountProgress* MountProgressionMgr::GetOrCreateProgress(Player* player, uint32 spellId)
{
    if (!player)
        return nullptr;

    uint32 guidLow = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    auto& bucket = _progress[guidLow];
    auto it = bucket.find(spellId);
    if (it != bucket.end())
        return &it->second;

    MountProgress mp;
    mp.spellId = spellId;
    mp.level   = 1;
    mp.xp      = 0;
    mp.dirty   = true;
    auto result = bucket.emplace(spellId, mp);
    return &result.first->second;
}
MountProgress const* MountProgressionMgr::GetProgress(Player* player, uint32 spellId) const
{
    if (!player)
        return nullptr;

    uint32 guidLow = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    auto it = _progress.find(guidLow);
    if (it == _progress.end())
        return nullptr;
    auto mit = it->second.find(spellId);
    return mit != it->second.end() ? &mit->second : nullptr;
}
std::vector<MountProgress> MountProgressionMgr::GetAllProgress(Player* player) const
{
    std::vector<MountProgress> out;
    if (!player)
        return out;

    uint32 guidLow = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    auto it = _progress.find(guidLow);
    if (it == _progress.end())
        return out;

    out.reserve(it->second.size());
    for (auto const& kv : it->second)
        out.push_back(kv.second);
    return out;
}
void MountProgressionMgr::SetActiveMount(Player* player, uint32 spellId)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    _active[guidLow] = spellId;
}
uint32 MountProgressionMgr::GetActiveMount(Player* player) const
{
    if (!player)
        return 0;
    uint32 guidLow = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    auto it = _active.find(guidLow);
    return it != _active.end() ? it->second : 0;
}
void MountProgressionMgr::ClearActiveMount(ObjectGuid guid)
{
    uint32 guidLow = guid.GetCounter();
    std::lock_guard<std::mutex> lock(_stateMutex);
    _active.erase(guidLow);
}

}  // namespace mod_mount_progression
