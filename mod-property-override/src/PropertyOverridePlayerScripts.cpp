#include "PropertyOverrideAddonMsg.h"
#include "PropertyOverrideMgr.h"

#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

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

    class PO_PlayerScript : public PlayerScript
    {
    public:
        PO_PlayerScript() : PlayerScript("PO_PlayerScript") {}

        void OnPlayerLogin(Player* player) override
        {
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
            PropertyOverrideMgr::Instance().OnPlayerTick(player, diffMs);
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
                std::vector<addon::RowView> rows;
                if (Item* item = ResolveQueryItem(player, query))
                    for (OverrideRow const& row :
                         mgr.GetActiveOverrides(player, item->GetGUID().GetCounter()))
                        rows.push_back({ row.property, row.value, row.expiry });

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
