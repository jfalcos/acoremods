#include "ParagonMgr.h"
#include "ParagonStrings.h"
#include "RewardDispatcher.h"

#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <sstream>

using namespace Acore::ChatCommands;
using namespace mod_paragon;
using mod_property_override::ParseProperty;
using mod_property_override::Property;
using mod_property_override::PropertyName;

namespace
{
    Player* CmdPlayer(ChatHandler* handler)
    {
        return handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    bool HandleInfoCmd(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        auto& pm = ParagonMgr::Instance();
        if (!pm.IsEnabled())
        {
            handler->PSendSysMessage(LANG_PARAGON_DISABLED);
            return true;
        }

        uint32 accountId = player->GetSession()->GetAccountId();
        uint64 lifetimePx = pm.GetLifetimePX(accountId);
        handler->PSendSysMessage(LANG_PARAGON_INFO,
                                 pm.ComputePL(lifetimePx),
                                 static_cast<unsigned long long>(
                                     pm.ComputeProgressInLevel(lifetimePx)),
                                 pm.PxPerLevel(),
                                 static_cast<unsigned long long>(lifetimePx));
        handler->PSendSysMessage("XP allocation: {}% (cap {}%). Coins: {}.",
                                 pm.GetAllocPercent(accountId),
                                 pm.MaxAllocPercentFor(player->GetLevel()),
                                 player->GetItemCount(pm.CoinItemId(), false));
        return true;
    }

    bool HandleSetXpCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        uint32 percent = 0;
        if (!(in >> percent) || percent > 100)
        {
            handler->SendSysMessage("Usage: .paragon setxp <0-100>");
            return true;
        }

        auto& pm = ParagonMgr::Instance();
        uint32 cap = pm.MaxAllocPercentFor(player->GetLevel());
        if (percent > cap)
        {
            handler->PSendSysMessage("Cannot allocate more than {}% at your level.", cap);
            return true;
        }

        pm.SetAllocPercent(player->GetSession()->GetAccountId(), percent);
        handler->PSendSysMessage("Paragon XP allocation set to {}%.", percent);
        return true;
    }

    bool HandleToggleCmd(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        auto& pm = ParagonMgr::Instance();
        uint32 accountId = player->GetSession()->GetAccountId();
        bool nowOptedOut = !pm.IsOptedOut(accountId);
        pm.SetOptOut(accountId, nowOptedOut);
        handler->PSendSysMessage("Paragon progression is now {}.",
                                 nowOptedOut ? "OFF" : "ON");
        return true;
    }

    bool HandlePerksCmd(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        auto& pm = ParagonMgr::Instance();
        auto const& cfg = pm.PerkCfg();

        handler->PSendSysMessage("Paragon perks (coins: {}):",
                                 player->GetItemCount(pm.CoinItemId(), false));
        for (size_t i = 0; i < perks::PERK_SET.size(); ++i)
        {
            Property prop = perks::PERK_SET[i];
            uint32 ranks = pm.GetPerkRanks(player->GetGUID().GetCounter(), prop);
            uint32 cost = perks::CostForNextRank(cfg, ranks);
            uint32 total = perks::TotalValue(cfg, i, ranks);
            if (cost)
                handler->PSendSysMessage("  {} |cff9d9d9d+{}|r |cff1eff00+{}|r  "
                                         "|cffffd100rank {}/{}, {} coin(s)|r",
                                         PropertyName(prop), total,
                                         perks::TotalValue(cfg, i, ranks + 1),
                                         ranks, cfg.maxRanks, cost);
            else
                handler->PSendSysMessage("  |cff9d9d9d{} +{} (rank {}/{} MAX)|r",
                                         PropertyName(prop), total, ranks, cfg.maxRanks);
        }
        handler->SendSysMessage("Buy with .paragon buy <perk> or at the Paragon Quartermaster.");
        return true;
    }

    bool HandleBuyCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        std::string token;
        if (!(in >> token))
        {
            handler->SendSysMessage("Usage: .paragon buy <perk> (see .paragon perks)");
            return true;
        }

        auto prop = ParseProperty(token);
        if (!prop || !perks::IsPerkProperty(*prop))
        {
            handler->PSendSysMessage("'{}' is not a paragon perk. See .paragon perks.", token);
            return true;
        }

        ParagonMgr::Instance().TryPurchasePerk(player, *prop);
        return true;
    }

    bool HandleUpgradeCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        std::string token;
        if (!(in >> slot >> token) || slot >= EQUIPMENT_SLOT_END)
        {
            handler->SendSysMessage("Usage: .paragon upgrade <slot 0-18> <property>");
            return true;
        }

        auto prop = ParseProperty(token);
        if (!prop)
        {
            handler->PSendSysMessage("Unknown property '{}'.", token);
            return true;
        }

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot));
        if (!item)
        {
            handler->PSendSysMessage("No item equipped in slot {}.", slot);
            return true;
        }

        ParagonMgr::Instance().TryPurchaseItemUpgrade(player, item, *prop);
        return true;
    }

    bool HandleUpgradesCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;

        std::istringstream in(args ? args : "");
        uint32 slot = 0;
        if (!(in >> slot) || slot >= EQUIPMENT_SLOT_END)
        {
            handler->SendSysMessage("Usage: .paragon upgrades <slot 0-18>");
            return true;
        }

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot));
        if (!item)
        {
            handler->PSendSysMessage("No item equipped in slot {}.", slot);
            return true;
        }

        auto& pm = ParagonMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        ItemTemplate const* proto = item->GetTemplate();
        auto rows = props.GetActiveOverrides(player, item->GetGUID().GetCounter());
        float budget = upgrades::UpgradeBudget(pm.UpgradeCfg(),
                                               proto->Quality, proto->ItemLevel);
        handler->PSendSysMessage("{} - |cffffd100upgrade budget {:.0f}/{:.0f}|r:",
                                 proto->Name1,
                                 mod_property_override::BudgetSpent(rows, "paragon"), budget);
        for (auto const& row : rows)
            handler->PSendSysMessage("  [{}] {} |cff1eff00+{}|r",
                                     row.source,
                                     PropertyName(static_cast<Property>(row.property)),
                                     row.value);
        if (rows.empty())
            handler->SendSysMessage("  (no upgrades yet)");
        return true;
    }

    // ---- GM subcommands ----

    bool HandleAddPxCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        std::istringstream in(args ? args : "");
        uint64 amount = 0;
        if (!(in >> amount) || !amount)
        {
            handler->SendSysMessage("Usage: .paragon addpx <amount>");
            return true;
        }
        ParagonMgr::Instance().AddPX(player, amount);
        handler->PSendSysMessage(LANG_PARAGON_ADD_OK,
                                 static_cast<unsigned long long>(amount));
        return true;
    }

    bool HandleSetLevelCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        std::istringstream in(args ? args : "");
        uint32 level = 0;
        if (!(in >> level))
        {
            handler->SendSysMessage("Usage: .paragon setlevel <n>");
            return true;
        }
        auto& pm = ParagonMgr::Instance();
        pm.SetLifetimePX(player, static_cast<uint64>(level) * pm.PxPerLevel(), level);
        handler->PSendSysMessage("Account paragon level set to {}.", level);
        return true;
    }

    bool HandleCoinsCmd(ChatHandler* handler, char const* args)
    {
        Player* player = CmdPlayer(handler);
        if (!player)
            return false;
        std::istringstream in(args ? args : "");
        uint32 amount = 0;
        if (!(in >> amount) || !amount)
        {
            handler->SendSysMessage("Usage: .paragon coins <amount>");
            return true;
        }
        player->AddItem(ParagonMgr::Instance().CoinItemId(), amount);
        handler->PSendSysMessage("Granted {} Paragon Coin(s).", amount);
        return true;
    }

    bool HandleResetPerksCmd(ChatHandler* handler, char const* /*args*/)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;
        ParagonMgr::Instance().ResetPerks(target);
        handler->PSendSysMessage("Cleared paragon perks on {}.", target->GetName());
        return true;
    }

    bool HandleDebugCmd(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        uint32 state = 0;
        if (!(in >> state) || state > 1)
        {
            handler->SendSysMessage("Usage: .paragon debug <0|1>");
            return true;
        }
        ParagonMgr::Instance().SetDebug(state != 0);
        handler->PSendSysMessage(LANG_PARAGON_DEBUG_TOG, state ? "ON" : "OFF");
        return true;
    }

    bool HandleReloadCmd(ChatHandler* handler, char const* /*args*/)
    {
        ParagonMgr::Instance().LoadConfig();
        handler->PSendSysMessage(LANG_PARAGON_CFG_RELOAD);
        return true;
    }

    bool HandleVendorCmd(ChatHandler* handler, char const* /*args*/)
    {
        QueryResult qr = WorldDatabase.Query(
            "SELECT slot, item, cost FROM paragon_vendor_stock "
            "WHERE week_id = FLOOR(UNIX_TIMESTAMP()/604800) % 4 ORDER BY slot");
        if (!qr)
        {
            handler->SendSysMessage("No vendor stock this week.");
            return true;
        }
        do
        {
            Field* f = qr->Fetch();
            handler->PSendSysMessage("  slot {} - item {} - {} coin(s)",
                                     f[0].Get<uint8>(), f[1].Get<uint32>(),
                                     f[2].Get<uint8>());
        } while (qr->NextRow());
        return true;
    }

    class Paragon_CommandScript : public CommandScript
    {
    public:
        Paragon_CommandScript() : CommandScript("Paragon_CommandScript") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable sub =
            {
                { "info",       HandleInfoCmd,       SEC_PLAYER,     Console::No },
                { "setxp",      HandleSetXpCmd,      SEC_PLAYER,     Console::No },
                { "toggle",     HandleToggleCmd,     SEC_PLAYER,     Console::No },
                { "perks",      HandlePerksCmd,      SEC_PLAYER,     Console::No },
                { "buy",        HandleBuyCmd,        SEC_PLAYER,     Console::No },
                { "upgrade",    HandleUpgradeCmd,    SEC_PLAYER,     Console::No },
                { "upgrades",   HandleUpgradesCmd,   SEC_PLAYER,     Console::No },
                { "addpx",      HandleAddPxCmd,      SEC_GAMEMASTER, Console::No },
                { "setlevel",   HandleSetLevelCmd,   SEC_GAMEMASTER, Console::No },
                { "coins",      HandleCoinsCmd,      SEC_GAMEMASTER, Console::No },
                { "resetperks", HandleResetPerksCmd, SEC_GAMEMASTER, Console::No },
                { "debug",      HandleDebugCmd,      SEC_GAMEMASTER, Console::No },
                { "reload",     HandleReloadCmd,     SEC_GAMEMASTER, Console::No },
                { "vendor",     HandleVendorCmd,     SEC_GAMEMASTER, Console::No },
            };
            static ChatCommandTable root =
            {
                { "paragon", sub },
            };
            return root;
        }
    };
}

void AddParagonCommandScripts()
{
    new Paragon_CommandScript();
}
