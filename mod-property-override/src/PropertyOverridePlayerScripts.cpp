#include "PropertyOverrideAddonMsg.h"
#include "PropertyOverrideMgr.h"

#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

#include <algorithm>

using namespace mod_property_override;

namespace
{
    // Converts the addon's raw client coordinates to a server-side item.
    // Client conventions: equipped inv-slot ids are 1-based (1 = head ...
    // 19 = tabard); bag 0 = backpack, 1-4 = equipped bags, bag slots 1-based.
    Item* ResolveQueryItem(Player* player, addon::Query const& query)
    {
        if (query.kind == addon::Query::Kind::Equipped)
        {
            if (query.slot < 1 || query.slot > EQUIPMENT_SLOT_END)
                return nullptr;
            return player->GetItemByPos(INVENTORY_SLOT_BAG_0,
                                        static_cast<uint8>(query.slot - 1));
        }

        if (query.slot < 1)
            return nullptr;
        if (query.bag == 0)
        {
            uint32 slot = INVENTORY_SLOT_ITEM_START + (query.slot - 1);
            if (slot >= INVENTORY_SLOT_ITEM_END)
                return nullptr;
            return player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot));
        }
        if (query.bag > 4)
            return nullptr;
        return player->GetItemByPos(
            static_cast<uint8>(INVENTORY_SLOT_BAG_START + (query.bag - 1)),
            static_cast<uint8>(query.slot - 1));
    }

    // Playerbot sessions are full Player objects running every hook; unless
    // opted in they skip the login DB load and the per-tick reconcile.
    // Logout/unload stays ungated (harmless no-op for never-loaded players,
    // self-healing if the config flips mid-session).
    bool SkipBot(Player* player)
    {
        return !PropertyOverrideMgr::Instance().ProcessBots() &&
               player && player->GetSession() && player->GetSession()->IsBot();
    }

    class PO_PlayerScript : public PlayerScript
    {
    public:
        PO_PlayerScript() : PlayerScript("PO_PlayerScript") {}

        void OnPlayerLogin(Player* player) override
        {
            if (SkipBot(player))
                return;
            auto& mgr = PropertyOverrideMgr::Instance();
            mgr.LoadPlayer(player);
            mgr.Sync(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            if (player)
                PropertyOverrideMgr::Instance().UnloadPlayer(player->GetGUID().GetCounter());
        }

        void OnPlayerEquip(Player* player, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/,
                           bool /*update*/) override
        {
            PropertyOverrideMgr::Instance().Sync(player);
        }

        void OnPlayerUnequip(Player* player, Item* /*it*/) override
        {
            PropertyOverrideMgr::Instance().Sync(player);
        }

        void OnPlayerAfterMoveItemFromInventory(Player* player, Item* /*it*/, uint8 /*bag*/,
                                                uint8 /*slot*/, bool /*update*/) override
        {
            PropertyOverrideMgr::Instance().Sync(player);
        }

        void OnPlayerUpdate(Player* player, uint32 diffMs) override
        {
            if (SkipBot(player))
                return;
            PropertyOverrideMgr::Instance().OnPlayerTick(player, diffMs);
        }

        // Fires inside the CHAR_DELETE_REMOVE transaction (Player.cpp
        // DeleteFromDB), before commit; the recycle-bin path re-fires it at
        // final purge. Touch only the transaction — may run for offline
        // characters, so no manager state is involved.
        void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
        {
            trans->Append("DELETE FROM player_property_override WHERE player_guid = {}", guid);
            // Best-effort for the character's item rows (char deletion removes
            // item_instance wholesale without per-item hooks); the startup
            // orphan sweep is the backstop for traded-away stale owners.
            trans->Append("DELETE FROM item_property_override WHERE owner_guid = {}", guid);
        }

        // Addon channel: the PropertyOverlay addon whispers itself with an
        // "IPROP\t..." LANG_ADDON message. Consume it (return false so it is
        // never delivered/echoed) and reply on the same channel.
        [[nodiscard]] bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang,
                                              std::string& msg, Player* receiver) override
        {
            if (lang != LANG_ADDON || receiver != player || !addon::IsAddonMessage(msg))
                return true;

            auto& mgr = PropertyOverrideMgr::Instance();
            if (!mgr.IsEnabled())
                return true;

            addon::Query query = addon::ParseQuery(msg);
            if (query.kind != addon::Query::Kind::Invalid)
            {
                // Same property can exist under several sources ('paragon',
                // 'mix', ...); the tooltip wants one merged line per property.
                std::vector<addon::RowView> rows;
                if (Item* item = ResolveQueryItem(player, query))
                    for (OverrideRow const& row :
                         mgr.GetActiveOverrides(player, item->GetGUID().GetCounter()))
                    {
                        auto it = std::find_if(rows.begin(), rows.end(),
                                               [&](addon::RowView const& v)
                                               { return v.property == row.property; });
                        if (it == rows.end())
                            rows.push_back({ row.property, row.value, row.expiry });
                        else
                        {
                            it->value += row.value;
                            if (row.expiry != 0 && (it->expiry == 0 || row.expiry < it->expiry))
                                it->expiry = row.expiry;
                        }
                    }

                mgr.SendAddonMessage(player, addon::BuildReply(query, rows));
            }
            return false; // malformed IPROP traffic is swallowed silently
        }
    };
}

void AddPropertyOverridePlayerScripts()
{
    new PO_PlayerScript();
}
