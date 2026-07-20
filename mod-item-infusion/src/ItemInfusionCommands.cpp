#include "ItemInfusionMgr.h"
#include "PropertyOverrideMgr.h"

#include "Bag.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <sstream>

using namespace Acore::ChatCommands;
using namespace mod_item_infusion;
using mod_property_override::PropertyName;

namespace
{
    Player* CmdPlayer(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            handler->SendSysMessage("Must be in-game.");
        return player;
    }

    Item* GetEquipped(ChatHandler* handler, uint32 slot)
    {
        if (slot >= EQUIPMENT_SLOT_END)
        {
            handler->PSendSysMessage("Target slot must be 0-{}.", EQUIPMENT_SLOT_END - 1);
            return nullptr;
        }
        Item* item = handler->GetPlayer()->GetItemByPos(
            INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot));
        if (!item)
            handler->PSendSysMessage("No item equipped in slot {}.", slot);
        return item;
    }

    // bag 0 = backpack (slot 0-15), bags 1-4 = the equipped bags.
    Item* GetBagItem(ChatHandler* handler, uint32 bag, uint32 slot)
    {
        Player* player = handler->GetPlayer();
        Item* item = nullptr;
        if (bag == 0)
        {
            if (slot < INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START)
                item = player->GetItemByPos(
                    INVENTORY_SLOT_BAG_0,
                    static_cast<uint8>(INVENTORY_SLOT_ITEM_START + slot));
        }
        else if (bag <= INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START)
        {
            if (Bag* b = player->GetBagByPos(
                    static_cast<uint8>(INVENTORY_SLOT_BAG_START + bag - 1)))
                if (slot < b->GetBagSize())
                    item = b->GetItemByPos(static_cast<uint32>(slot));
        }
        if (!item)
            handler->PSendSysMessage("No item in bag {} slot {}.", bag, slot);
        return item;
    }

    bool HandleInfuseCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        uint32 targetSlot = 0, bag = 0, slot = 0, coins = 0;
        if (!(in >> targetSlot >> bag >> slot))
        {
            handler->SendSysMessage("Usage: .infuse <targetSlot 0-18> <bag 0-4> <bagSlot> [coins]");
            handler->SendSysMessage("Bag 0 = backpack. Substances are only offered at the Alchemist.");
            return true;
        }
        in >> coins; // optional

        Item* target = GetEquipped(handler, targetSlot);
        if (!target)
            return true;
        Item* donor = GetBagItem(handler, bag, slot);
        if (!donor)
            return true;

        ItemInfusionMgr::Instance().TryInfuse(player, target, donor, coins, {});
        return true;
    }

    bool HandleListCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        if (!(in >> slot))
        {
            handler->SendSysMessage("Usage: .infuse list <slot 0-18>");
            return true;
        }

        Item* item = GetEquipped(handler, slot);
        if (!item)
            return true;

        auto& mgr = ItemInfusionMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        ItemTemplate const* proto = item->GetTemplate();
        auto rows = props.GetActiveOverrides(player, item->GetGUID().GetCounter());
        float mixPts = mod_property_override::BudgetSpent(rows, "mix");
        float f = MixFraction(rows, proto->Quality, proto->ItemLevel);

        handler->PSendSysMessage("{} - |cffffd100infused {:.0f} pts, next risk {:.0f}%|r:",
                                 proto->Name1, mixPts,
                                 RiskFor(mgr.Cfg(), f) * 100.f);
        bool any = false;
        for (auto const& row : rows)
            if (row.source == "mix")
            {
                handler->PSendSysMessage("  {} |cff1eff00+{}|r",
                                         PropertyName(static_cast<mod_property_override::Property>(
                                             row.property)),
                                         row.value);
                any = true;
            }
        if (!any)
            handler->SendSysMessage("  (no infusions yet)");
        return true;
    }

    bool HandleRiskCmd(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        int32 pct = -1;
        if (!(in >> pct) || pct > 100)
        {
            handler->SendSysMessage("Usage: .infuse risk <0-100|-1>  (-1 = live math)");
            return true;
        }
        ItemInfusionMgr::Instance().SetRiskOverride(pct);
        if (pct < 0)
            handler->SendSysMessage("Infusion risk override cleared (live math).");
        else
            handler->PSendSysMessage("Infusion risk forced to {}% for all rolls.", pct);
        return true;
    }

    class ItemInfusion_CommandScript : public CommandScript
    {
    public:
        ItemInfusion_CommandScript() : CommandScript("ItemInfusion_CommandScript") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable sub =
            {
                { "list", HandleListCmd, SEC_PLAYER,     Console::No },
                { "risk", HandleRiskCmd, SEC_GAMEMASTER, Console::No },
                { "",     HandleInfuseCmd, SEC_PLAYER,   Console::No },
            };
            static ChatCommandTable root =
            {
                { "infuse", sub },
            };
            return root;
        }
    };
}

void AddItemInfusionCommandScripts()
{
    new ItemInfusion_CommandScript();
}
