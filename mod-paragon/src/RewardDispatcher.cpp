#include "RewardDispatcher.h"

#include "ParagonMgr.h"
#include "ParagonStrings.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "Player.h"
#include "WorldSession.h"

using namespace mod_paragon;

namespace
{
    void SendSimpleMail(Player* player, std::string const& subject,
                        std::string const& body, uint32 itemId, uint32 itemCount)
    {
        if (!player)
            return;

        MailDraft draft(subject, body);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        if (itemId && itemCount)
        {
            if (Item* item = Item::CreateItem(itemId, itemCount, player))
            {
                item->SaveToDB(trans);
                draft.AddItem(item);
            }
            else
                LOG_ERROR("module",
                          "mod-paragon: CreateItem(template={}, count={}) failed for {}",
                          itemId, itemCount, player->GetName());
        }

        MailSender sender(MAIL_CREATURE,
                          ParagonMgr::Instance().QuartermasterEntry(),
                          MAIL_STATIONERY_GM);
        draft.SendMailTo(trans, MailReceiver(player), sender);
        CharacterDatabase.CommitTransaction(trans);
    }
}

bool RewardDispatcher::HasDelivered(uint32 guid, uint32 level)
{
    return CharacterDatabase.Query(
               "SELECT 1 FROM paragon_char_delivery WHERE guid = {} AND level = {} LIMIT 1",
               guid, level) != nullptr;
}

void RewardDispatcher::MarkDelivered(uint32 guid, uint32 level)
{
    CharacterDatabase.Execute(
        "INSERT IGNORE INTO paragon_char_delivery (guid, level, ts) "
        "VALUES ({}, {}, UNIX_TIMESTAMP())",
        guid, level);
}

void RewardDispatcher::DeliverPendingFor(Player* player)
{
    if (!player || !player->GetSession())
        return;

    uint32 topLevel = ParagonMgr::Instance().ComputePL(
        ParagonMgr::Instance().GetLifetimePX(player->GetSession()->GetAccountId()));
    for (uint32 lv = 1; lv <= topLevel; ++lv)
        DeliverLevelTo(player, lv);
}

void RewardDispatcher::DeliverLevelTo(Player* player, uint32 level)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    if (level && HasDelivered(guid, level))
        return;

    // Every level grants a coin.
    SendSimpleMail(player, "Paragon Coin", "A token of perseverance.",
                   ParagonMgr::Instance().CoinItemId(), 1);

    // Milestone levels add their catalog reward.
    if (level)
    {
        if (QueryResult qr = WorldDatabase.Query(
                "SELECT mail_subject, mail_body, item_id "
                "FROM paragon_rewards WHERE level = {}", level))
        {
            Field* f = qr->Fetch();
            uint32 itemId = f[2].Get<uint32>();
            SendSimpleMail(player, f[0].Get<std::string>(), f[1].Get<std::string>(),
                           itemId, itemId ? 1 : 0);
        }
        MarkDelivered(guid, level);
    }

    if (WorldSession* session = player->GetSession())
        ChatHandler(session).PSendSysMessage(LANG_PARAGON_REWARD_DELIVERED, level);
}

void RewardDispatcher::SendHandbookTo(Player* player)
{
    SendSimpleMail(player, "The Paragon Handbook",
                   "Every great journey deserves a map. Within these pages: "
                   "paragon leveling, coins and perks, item upgrades, and "
                   "infusion. Visit me to begin.\n\n- The Paragon Quartermaster",
                   ParagonMgr::HANDBOOK_ITEM_ID, 1);
}
