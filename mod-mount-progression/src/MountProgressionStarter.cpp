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

namespace
{
    // The starter quest is granted via Player::AddQuest with no live
    // questgiver, so QuestSortID (the quest-log group header) can't fall
    // back to a questgiver's position -- it needs a real zone ID, and one
    // quest ID can only carry one QuestSortID. Rather than mislabel every
    // non-Human race under Northshire Abbey's header, each starting zone
    // gets its own quest_template row (900000 + offset below), same text,
    // correct QuestSortID for that zone. Offsets are fixed to the 8
    // starting-zone rows shipped in this module's SQL, not admin-configurable.
    uint32 StarterQuestOffsetForRace(uint8 race)
    {
        switch (race)
        {
            case RACE_ORC:           return 1; // Valley of Trials
            case RACE_TROLL:         return 1; // Valley of Trials (shared with Orc)
            case RACE_DWARF:         return 2; // Coldridge Valley
            case RACE_GNOME:         return 2; // Coldridge Valley (shared with Dwarf)
            case RACE_NIGHTELF:      return 3; // Shadowglen
            case RACE_UNDEAD_PLAYER: return 4; // Tirisfal Glades
            case RACE_TAUREN:        return 5; // Mulgore
            case RACE_BLOODELF:      return 6; // Sunstrider Isle
            case RACE_DRAENEI:       return 7; // Azuremyst Isle
            default:                 return 0; // RACE_HUMAN, and fallback for any other/future race
        }
    }
}

void MountProgressionMgr::SaveActiveMountToDB(Player* player)
{
    if (!player)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    uint32 spellId = GetActiveMount(player);

    if (!spellId)
    {
        CharacterDatabase.Execute(
            "DELETE FROM character_mount_active WHERE guid = {}", guidLow);
        return;
    }

    uint64 now = static_cast<uint64>(::time(nullptr));
    CharacterDatabase.Execute(
        "INSERT INTO character_mount_active (guid, spell_id, last_active_time) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "spell_id = VALUES(spell_id), "
        "last_active_time = VALUES(last_active_time)",
        guidLow, spellId, now);
}
void MountProgressionMgr::LoadActiveMountFromDB(Player* player)
{
    if (!_enabled || !player)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    QueryResult result = CharacterDatabase.Query(
        "SELECT spell_id, last_active_time FROM character_mount_active "
        "WHERE guid = {}",
        guidLow);
    if (!result)
        return;

    Field* f = result->Fetch();
    uint32 spellId = f[0].Get<uint32>();
    uint64 lastActive = f[1].Get<uint64>();

    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry)
    {
        CharacterDatabase.Execute(
            "DELETE FROM character_mount_active WHERE guid = {}", guidLow);
        return;
    }

    uint64 now = static_cast<uint64>(::time(nullptr));
    if (_offlineGraceSeconds > 0 &&
        now - lastActive > _offlineGraceSeconds)
    {
        CharacterDatabase.Execute(
            "DELETE FROM character_mount_active WHERE guid = {}", guidLow);
        if (_debug)
            LOG_INFO("module",
                     "mod-mount-progression: guid {} offline {}s > grace {}s, "
                     "carrier not reapplied",
                     guidLow, static_cast<uint64>(now - lastActive),
                     _offlineGraceSeconds);
        return;
    }

    SetActiveMount(player, spellId);
    MountProgress const* mp = GetProgress(player, spellId);
    uint16 level = mp ? mp->level : 1;
    ApplyMountBuff(player, entry, level);
}
bool MountProgressionMgr::HasMadeStarterChoice(Player* player) const
{
    if (!player)
        return false;
    uint32 guidLow = player->GetGUID().GetCounter();
    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM character_mount_starter_choice WHERE guid = {}", guidLow);
    return result != nullptr;
}
uint32 MountProgressionMgr::GetStarterSpell(MountType t) const
{
    switch (t)
    {
        case MountType::Stamina:  return _starterSpell[0];
        case MountType::Predator: return _starterSpell[1];
        case MountType::Arcane:   return _starterSpell[2];
        default:                  return 0;
    }
}
CatalogEntry const* MountProgressionMgr::GrantStarterMount(Player* player, uint32 spellId)
{
    if (!_enabled || !player)
        return nullptr;
    if (HasMadeStarterChoice(player))
        return nullptr;

    CatalogEntry const* entry = GetCatalogEntry(spellId);
    if (!entry)
        return nullptr;

    if (!player->HasSpell(spellId))
        player->learnSpell(spellId);

    ActivateMount(player, spellId);

    uint32 guidLow = player->GetGUID().GetCounter();
    uint64 now = static_cast<uint64>(::time(nullptr));
    CharacterDatabase.Execute(
        "INSERT INTO character_mount_starter_choice (guid, spell_id, chosen_at) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "spell_id = VALUES(spell_id), "
        "chosen_at = VALUES(chosen_at)",
        guidLow, spellId, now);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: guid {} made starter mount choice -> {} ({})",
                 guidLow, spellId, entry->displayName);

    return entry;
}
bool MountProgressionMgr::ResetStarterChoice(Player* player)
{
    if (!player)
        return false;

    uint32 guidLow = player->GetGUID().GetCounter();
    bool hadChoice = HasMadeStarterChoice(player);

    CharacterDatabase.Execute(
        "DELETE FROM character_mount_starter_choice WHERE guid = {}", guidLow);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: guid {} starter choice reset (had_choice={})",
                 guidLow, hadChoice);

    return hadChoice;
}
void MountProgressionMgr::MaybeSendStarterQuest(Player* player)
{
    if (!_enabled || !_starterQuestEnabled || !player)
        return;
    if (HasMadeStarterChoice(player))
        return;

    uint32 guidLow = player->GetGUID().GetCounter();

    QueryResult already = CharacterDatabase.Query(
        "SELECT 1 FROM character_mount_starter_quest_sent WHERE guid = {}", guidLow);
    if (already)
        return;

    uint32 questId = _starterQuestId + StarterQuestOffsetForRace(player->getRace());
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest)
    {
        player->AddQuest(quest, nullptr);
        // Without this the client never receives the quest's text and
        // shows "Missing Header" in the quest log for this entry. The
        // quest LOG window (L) is populated by SMSG_QUEST_QUERY_RESPONSE
        // (SendQuestQueryResponse) -- the same packet
        // HandleQuestQueryOpcode sends in response to CMSG_QUEST_QUERY --
        // NOT SMSG_QUESTGIVER_QUEST_DETAILS (SendQuestGiverQuestDetails),
        // which is the live NPC accept/decline offer dialog and doesn't
        // touch the log's cached quest data.
        player->PlayerTalkClass->SendQuestQueryResponse(quest);
    }

    MailDraft draft(
        "Your First Companion",
        "A folded letter arrives, sealed with a hoofprint pressed into wax.\n\n"
        "\"Every rider needs a first companion -- one that will grow "
        "alongside you. I've something for you; come find me, and we'll "
        "see which one suits you best.\"\n\n"
        "-- The Mount Tamer\n\n"
        "(A new task has been added to your quest log.)");

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    draft.SendMailTo(trans, MailReceiver(player, guidLow), MailSender(MAIL_NORMAL, 0));
    trans->Append(
        "INSERT INTO character_mount_starter_quest_sent (guid, sent_at) "
        "VALUES ({}, {}) "
        "ON DUPLICATE KEY UPDATE sent_at = VALUES(sent_at)",
        guidLow, static_cast<uint64>(::time(nullptr)));
    CharacterDatabase.CommitTransaction(trans);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: guid {} sent starter mail+quest (quest={})",
                 guidLow, questId);
}
void MountProgressionMgr::CompleteStarterQuest(Player* player)
{
    if (!_enabled || !_starterQuestEnabled || !player)
        return;

    uint32 questId = _starterQuestId + StarterQuestOffsetForRace(player->getRace());
    uint16 slot = player->FindQuestSlot(questId);
    if (slot >= MAX_QUEST_LOG_SIZE)
        return;

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return;

    player->CompleteQuest(questId);
    player->RewardQuest(quest, 0, player);

    if (_debug)
        LOG_INFO("module",
                 "mod-mount-progression: guid {} completed starter quest {}",
                 player->GetGUID().GetCounter(), questId);
}

}  // namespace mod_mount_progression
