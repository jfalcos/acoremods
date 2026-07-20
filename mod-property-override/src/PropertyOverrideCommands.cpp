#include "PropertyOverrideAddonMsg.h"
#include "PropertyOverrideMgr.h"

#include "Chat.h"
#include "ChatCommand.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <sstream>

using namespace Acore::ChatCommands;
using namespace mod_property_override;

namespace
{
    // Slot argument = server equipment slot index (0 = head ... 18 = tabard;
    // 15/16 = main/off hand).
    Item* GetEquippedItem(ChatHandler* handler, uint32 slot)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return nullptr;
        if (slot >= EQUIPMENT_SLOT_END)
        {
            handler->PSendSysMessage("Slot must be 0-{}.", EQUIPMENT_SLOT_END - 1);
            return nullptr;
        }
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot));
        if (!item)
            handler->PSendSysMessage("No item equipped in slot {}.", slot);
        return item;
    }

    void PushInvalidate(Player* player)
    {
        PropertyOverrideMgr::Instance().SendAddonMessage(player, addon::BuildInvalidate());
    }

    bool HandleAddCmd(ChatHandler* handler, char const* args)
    {
        auto& mgr = PropertyOverrideMgr::Instance();
        if (!mgr.IsEnabled())
        {
            handler->SendSysMessage("Property overrides are disabled.");
            return true;
        }

        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        std::string propToken;
        int32 value = 0;
        uint32 duration = 0;
        if (!(in >> slot >> propToken >> value))
        {
            handler->SendSysMessage(
                "Usage: .propover add <slot> <property> <value> [durationSecs]");
            handler->SendSysMessage(
                "Property = name, unique prefix, or ITEM_MOD id. See .propover props");
            return true;
        }
        in >> duration; // optional

        std::optional<Property> prop = ParseProperty(propToken);
        if (!prop)
        {
            handler->PSendSysMessage("Unknown property '{}'.", propToken);
            return true;
        }

        Item* item = GetEquippedItem(handler, slot);
        if (!item)
            return true;

        if (!mgr.AddOverride(handler->GetPlayer(), item, *prop, value, duration))
        {
            handler->SendSysMessage("Failed to add override.");
            return true;
        }

        if (duration)
            handler->PSendSysMessage("Item {} (guid {}): {} {:+d} for {}s.",
                                     item->GetTemplate()->Name1,
                                     item->GetGUID().GetCounter(),
                                     PropertyName(*prop), value, duration);
        else
            handler->PSendSysMessage("Item {} (guid {}): {} {:+d} (permanent).",
                                     item->GetTemplate()->Name1,
                                     item->GetGUID().GetCounter(),
                                     PropertyName(*prop), value);
        PushInvalidate(handler->GetPlayer());
        return true;
    }

    bool HandleClearCmd(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        if (!(in >> slot))
        {
            handler->SendSysMessage("Usage: .propover clear <slot>");
            return true;
        }

        Item* item = GetEquippedItem(handler, slot);
        if (!item)
            return true;

        bool hadAny = PropertyOverrideMgr::Instance().ClearOverrides(handler->GetPlayer(), item);
        if (hadAny)
            handler->PSendSysMessage("Overrides cleared for item guid {}.",
                                     item->GetGUID().GetCounter());
        else
            handler->PSendSysMessage("No overrides on item guid {}.",
                                     item->GetGUID().GetCounter());
        PushInvalidate(handler->GetPlayer());
        return true;
    }

    bool HandleListCmd(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        if (!(in >> slot))
        {
            handler->SendSysMessage("Usage: .propover list <slot>");
            return true;
        }

        Item* item = GetEquippedItem(handler, slot);
        if (!item)
            return true;

        auto rows = PropertyOverrideMgr::Instance().GetActiveOverrides(
            handler->GetPlayer(), item->GetGUID().GetCounter());
        if (rows.empty())
        {
            handler->PSendSysMessage("No overrides on item guid {}.",
                                     item->GetGUID().GetCounter());
            return true;
        }

        handler->PSendSysMessage("Overrides on {} (guid {}):",
                                 item->GetTemplate()->Name1, item->GetGUID().GetCounter());
        for (OverrideRow const& row : rows)
        {
            if (row.expiry)
                handler->PSendSysMessage("  {} {:+d} (expires at {})",
                                         PropertyName(static_cast<Property>(row.property)),
                                         row.value, row.expiry);
            else
                handler->PSendSysMessage("  {} {:+d} (permanent)",
                                         PropertyName(static_cast<Property>(row.property)),
                                         row.value);
        }
        return true;
    }

    Player* GetTargetPlayer(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            handler->SendSysMessage("No player target.");
        return target;
    }

    bool HandlePlayerAddCmd(ChatHandler* handler, char const* args)
    {
        auto& mgr = PropertyOverrideMgr::Instance();
        if (!mgr.IsEnabled())
        {
            handler->SendSysMessage("Property overrides are disabled.");
            return true;
        }

        std::istringstream in(args ? args : "");
        std::string propToken;
        int32 value = 0;
        uint32 duration = 0;
        if (!(in >> propToken >> value))
        {
            handler->SendSysMessage("Usage: .propover padd <property> <value> [durationSecs]");
            handler->SendSysMessage("Targets the selected player, or yourself. See .propover props");
            return true;
        }
        in >> duration; // optional

        std::optional<Property> prop = ParseProperty(propToken);
        if (!prop)
        {
            handler->PSendSysMessage("Unknown property '{}'.", propToken);
            return true;
        }

        Player* target = GetTargetPlayer(handler);
        if (!target)
            return true;

        if (!mgr.SetPlayerOverride(target, "gm", *prop, value, duration))
        {
            handler->SendSysMessage("Failed to set player override.");
            return true;
        }

        if (duration)
            handler->PSendSysMessage("{}: {} {:+d} for {}s (source gm).",
                                     target->GetName(), PropertyName(*prop), value, duration);
        else
            handler->PSendSysMessage("{}: {} {:+d} permanent (source gm).",
                                     target->GetName(), PropertyName(*prop), value);
        return true;
    }

    bool HandlePlayerClearCmd(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        std::string source = "gm";
        in >> source; // optional explicit source; defaults to gm

        Player* target = GetTargetPlayer(handler);
        if (!target)
            return true;

        bool hadAny = PropertyOverrideMgr::Instance().ClearPlayerOverrides(target, source);
        if (hadAny)
            handler->PSendSysMessage("Cleared source '{}' overrides on {}.",
                                     source, target->GetName());
        else
            handler->PSendSysMessage("No source '{}' overrides on {}.",
                                     source, target->GetName());
        return true;
    }

    bool HandlePlayerListCmd(ChatHandler* handler, char const* /*args*/)
    {
        Player* target = GetTargetPlayer(handler);
        if (!target)
            return true;

        auto rows = PropertyOverrideMgr::Instance().GetPlayerOverrides(target);
        if (rows.empty())
        {
            handler->PSendSysMessage("No player overrides on {}.", target->GetName());
            return true;
        }

        handler->PSendSysMessage("Player overrides on {}:", target->GetName());
        for (PlayerRow const& row : rows)
        {
            if (row.expiry)
                handler->PSendSysMessage("  [{}] {} {:+d} (expires at {})",
                                         row.source,
                                         PropertyName(static_cast<Property>(row.property)),
                                         row.value, row.expiry);
            else
                handler->PSendSysMessage("  [{}] {} {:+d} (permanent)",
                                         row.source,
                                         PropertyName(static_cast<Property>(row.property)),
                                         row.value);
        }
        return true;
    }

    bool HandlePropsCmd(ChatHandler* handler, char const* /*args*/)
    {
        std::string line;
        for (Property p : AllProperties())
        {
            if (!line.empty())
                line += ", ";
            line += PropertyName(p);
            line += "(";
            line += std::to_string(static_cast<uint32>(p));
            line += ")";
            if (line.size() > 180)
            {
                handler->SendSysMessage(line.c_str());
                line.clear();
            }
        }
        if (!line.empty())
            handler->SendSysMessage(line.c_str());
        return true;
    }

    class PO_CommandScript : public CommandScript
    {
    public:
        PO_CommandScript() : CommandScript("PO_CommandScript") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable sub =
            {
                { "add",    HandleAddCmd,         SEC_GAMEMASTER, Console::No },
                { "clear",  HandleClearCmd,       SEC_GAMEMASTER, Console::No },
                { "list",   HandleListCmd,        SEC_GAMEMASTER, Console::No },
                { "props",  HandlePropsCmd,       SEC_GAMEMASTER, Console::No },
                { "padd",   HandlePlayerAddCmd,   SEC_GAMEMASTER, Console::No },
                { "pclear", HandlePlayerClearCmd, SEC_GAMEMASTER, Console::No },
                { "plist",  HandlePlayerListCmd,  SEC_GAMEMASTER, Console::No },
            };
            static ChatCommandTable root =
            {
                { "propover", sub },
            };
            return root;
        }
    };
}

void AddPropertyOverrideCommandScripts()
{
    new PO_CommandScript();
}
