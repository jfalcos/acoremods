#include "ParagonMgr.h"
#include "RewardDispatcher.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"

#include <fmt/format.h>

using namespace mod_paragon;
using mod_property_override::PropertyName;

namespace
{
    // Cosmetic stock rotates over a 4-week cycle (week_id 0-3 in
    // paragon_vendor_stock), reloaded hourly and on config load.
    struct Stock
    {
        uint32 item;
        uint8 cost;
    };

    std::vector<Stock> StockCache;
    uint16 CachedCycleWeek = 0xFFFF;

    uint16 CurCycleWeek() { return static_cast<uint16>((time(nullptr) / 604800) % 4); }

    void ReloadStock()
    {
        CachedCycleWeek = CurCycleWeek();
        StockCache.clear();
        if (QueryResult qr = WorldDatabase.Query(
                "SELECT item, cost FROM paragon_vendor_stock "
                "WHERE week_id = {} ORDER BY slot", CachedCycleWeek))
        {
            do
            {
                Field* f = qr->Fetch();
                StockCache.push_back({ f[0].Get<uint32>(), f[1].Get<uint8>() });
            } while (qr->NextRow());
        }
    }

    enum QmSenders
    {
        SENDER_MAIN  = 1,
        SENDER_PERKS = 2,
        SENDER_STOCK = 3,
    };

    enum QmActions
    {
        ACTION_SHOW_PERKS = GOSSIP_ACTION_INFO_DEF + 300,
        ACTION_SHOW_STOCK = GOSSIP_ACTION_INFO_DEF + 301,
        ACTION_CLOSE      = GOSSIP_ACTION_INFO_DEF + 302,
        // SENDER_PERKS actions: GOSSIP_ACTION_INFO_DEF + 310 + perkIndex
        ACTION_PERK_BASE  = GOSSIP_ACTION_INFO_DEF + 310,
        // SENDER_STOCK actions: GOSSIP_ACTION_INFO_DEF + 350 + slotIndex
        ACTION_STOCK_BASE = GOSSIP_ACTION_INFO_DEF + 350,
    };

    void SendMainMenu(Player* player, Creature* creature)
    {
        auto& pm = ParagonMgr::Instance();
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                         fmt::format("Spend Paragon Coins — Perks ({} coins)",
                                     player->GetItemCount(pm.CoinItemId(), false)),
                         SENDER_MAIN, ACTION_SHOW_PERKS);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                         "Browse this week's collection (pets & mounts)",
                         SENDER_MAIN, ACTION_SHOW_STOCK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendPerksMenu(Player* player, Creature* creature)
    {
        auto& pm = ParagonMgr::Instance();
        auto const& cfg = pm.PerkCfg();
        ClearGossipMenuFor(player);

        for (size_t i = 0; i < perks::PERK_SET.size(); ++i)
        {
            auto prop = perks::PERK_SET[i];
            uint32 ranks = pm.GetPerkRanks(player->GetGUID().GetCounter(), prop);
            uint32 cost = perks::CostForNextRank(cfg, ranks);
            std::string label = cost
                ? fmt::format("{} — rank {}/{} (+{}) — {} coin(s)",
                              PropertyName(prop), ranks, cfg.maxRanks,
                              perks::TotalValue(cfg, i, ranks), cost)
                : fmt::format("{} — rank {}/{} (+{}) — MAX",
                              PropertyName(prop), ranks, cfg.maxRanks,
                              perks::TotalValue(cfg, i, ranks));
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, label,
                             SENDER_PERKS, ACTION_PERK_BASE + i);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_MAIN, ACTION_CLOSE + 1); // back to main
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendStockMenu(Player* player, Creature* creature)
    {
        if (CurCycleWeek() != CachedCycleWeek)
            ReloadStock();

        ClearGossipMenuFor(player);
        for (size_t i = 0; i < StockCache.size(); ++i)
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(StockCache[i].item);
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                             fmt::format("{} — {} coin(s)",
                                         tmpl ? tmpl->Name1 : "Unknown item",
                                         StockCache[i].cost),
                             SENDER_STOCK, ACTION_STOCK_BASE + i);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_MAIN, ACTION_CLOSE + 1);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    class Paragon_QM_NPC : public CreatureScript
    {
    public:
        Paragon_QM_NPC() : CreatureScript("Paragon_QM_NPC") {}

        bool OnGossipHello(Player* player, Creature* creature) override
        {
            SendMainMenu(player, creature);
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature,
                            uint32 sender, uint32 action) override
        {
            auto& pm = ParagonMgr::Instance();

            if (sender == SENDER_MAIN)
            {
                if (action == ACTION_SHOW_PERKS)
                    SendPerksMenu(player, creature);
                else if (action == ACTION_SHOW_STOCK)
                    SendStockMenu(player, creature);
                else if (action == ACTION_CLOSE)
                    CloseGossipMenuFor(player);
                else
                    SendMainMenu(player, creature); // "Back"
                return true;
            }

            if (sender == SENDER_PERKS)
            {
                size_t idx = action - ACTION_PERK_BASE;
                if (idx < perks::PERK_SET.size())
                    pm.TryPurchasePerk(player, perks::PERK_SET[idx]);
                SendPerksMenu(player, creature); // re-show with updated ranks
                return true;
            }

            if (sender == SENDER_STOCK)
            {
                size_t idx = action - ACTION_STOCK_BASE;
                if (idx < StockCache.size())
                {
                    Stock const& st = StockCache[idx];
                    ChatHandler ch(player->GetSession());
                    if (player->GetItemCount(pm.CoinItemId(), false) < st.cost)
                        ch.SendSysMessage("|cffffd100[Paragon]|r Not enough Paragon Coins.");
                    else
                    {
                        player->DestroyItemCount(pm.CoinItemId(), st.cost, true);
                        player->AddItem(st.item, 1);
                        ch.SendSysMessage("|cffffd100[Paragon]|r Thank you for your purchase!");
                    }
                }
                SendStockMenu(player, creature);
                return true;
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    class Paragon_VendorRotation : public WorldScript
    {
    public:
        Paragon_VendorRotation()
            : WorldScript("Paragon_VendorRotation",
                          { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_UPDATE }) {}

        void OnAfterConfigLoad(bool /*reload*/) override { ReloadStock(); }

        void OnUpdate(uint32 diff) override
        {
            _timerMs += diff;
            if (_timerMs < 3600000) // hourly staleness check
                return;
            _timerMs = 0;
            if (CurCycleWeek() != CachedCycleWeek)
                ReloadStock();
        }

    private:
        uint32 _timerMs = 0;
    };
}

void AddParagonVendorScripts()
{
    new Paragon_QM_NPC();
    new Paragon_VendorRotation();
}
