#include "ItemInfusionMgr.h"
#include "PropertyOverrideDisplay.h"
#include "PropertyOverrideMgr.h"

#include "Bag.h"
#include "Chat.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"

#include <fmt/format.h>

#include <unordered_map>
#include <vector>

using namespace mod_item_infusion;
using mod_property_override::PropertyName;

namespace
{
    // Shared parchment-surface toolkit (same one the Paragon Quartermaster
    // uses — the two NPCs must render stats identically).
    using mod_property_override::display::COL_GRAY;
    using mod_property_override::display::COL_GREEN;
    using mod_property_override::display::COL_COST;
    using mod_property_override::display::COL_END;
    using mod_property_override::display::QualityColor;
    using mod_property_override::display::FormatStatDisplay;

    // Gossip layout. One pending-infusion selection per player, reset on
    // hello; donor identity is held as an item GUID so bag reshuffles
    // between clicks can never redirect an infusion to the wrong item.
    //
    //   SENDER_TARGETS: action = equipment slot (or ACTION_CLOSE)
    //   SENDER_DONORS:  action = ACTION_DONOR_BASE + index into the
    //                   re-enumerated donor list, or paging/back actions
    //   SENDER_CONFIRM: mitigation toggles / INFUSE / back
    enum Senders
    {
        SENDER_TARGETS = 1,
        SENDER_DONORS  = 2,
        SENDER_CONFIRM = 3,
    };

    enum Actions
    {
        ACTION_CLOSE        = GOSSIP_ACTION_INFO_DEF + 1,
        ACTION_BACK_TARGETS = GOSSIP_ACTION_INFO_DEF + 2,
        ACTION_BACK_DONORS  = GOSSIP_ACTION_INFO_DEF + 3,
        ACTION_PAGE_PREV    = GOSSIP_ACTION_INFO_DEF + 4,
        ACTION_PAGE_NEXT    = GOSSIP_ACTION_INFO_DEF + 5,
        ACTION_REFRESH      = GOSSIP_ACTION_INFO_DEF + 6,
        ACTION_COIN_ADD     = GOSSIP_ACTION_INFO_DEF + 7,
        ACTION_COIN_CLEAR   = GOSSIP_ACTION_INFO_DEF + 8,
        ACTION_INFUSE       = GOSSIP_ACTION_INFO_DEF + 9,
        ACTION_SUB_BASE     = GOSSIP_ACTION_INFO_DEF + 20, // + substance index
        ACTION_DONOR_BASE   = GOSSIP_ACTION_INFO_DEF + 100, // + donor index
    };

    constexpr size_t DONORS_PER_PAGE = 15;

    struct Pending
    {
        uint8 targetSlot = 0;
        ObjectGuid donorGuid;
        uint32 coins = 0;
        uint32 substanceMask = 0; // bit i = Substances()[i] selected
        uint32 donorPage = 0;
    };

    std::unordered_map<ObjectGuid::LowType, Pending> PendingByPlayer;

    Pending& StateOf(Player* player)
    {
        return PendingByPlayer[player->GetGUID().GetCounter()];
    }

    // All bag items with a non-empty infusion yield, in stable bag order.
    std::vector<Item*> EnumerateDonors(Player* player)
    {
        auto const& cfg = ItemInfusionMgr::Instance().Cfg();
        std::vector<Item*> donors;
        auto consider = [&](Item* item)
        {
            if (item && !DonorYield(ItemInfusionMgr::CollectDonorStats(item->GetTemplate()),
                                    cfg.efficiency).empty())
                donors.push_back(item);
        };
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, i));
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
            if (Bag* b = player->GetBagByPos(bag))
                for (uint32 s = 0; s < b->GetBagSize(); ++s)
                    consider(b->GetItemByPos(s));
        return donors;
    }

    // Substances the player can actually use right now: configured, with a
    // live template, and at least one in the bags.
    struct UsableSubstance
    {
        uint32 index; // position in Substances() — stable for the mask
        uint32 itemId;
        float reduction;
        char const* name;
        uint32 owned;
    };

    std::vector<UsableSubstance> UsableSubstances(Player* player)
    {
        auto const& subs = ItemInfusionMgr::Instance().Substances();
        std::vector<UsableSubstance> out;
        for (uint32 i = 0; i < subs.size(); ++i)
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(subs[i].first);
            if (!tmpl)
                continue;
            uint32 owned = player->GetItemCount(subs[i].first, false);
            if (!owned)
                continue;
            out.push_back({ i, subs[i].first, subs[i].second, tmpl->Name1.c_str(), owned });
        }
        return out;
    }

    // Live risk for the player's current pending selection.
    float PendingRisk(Player* player, Pending const& st, Item* target)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        ItemTemplate const* proto = target->GetTemplate();
        float f = MixFraction(props.GetActiveOverrides(player, target->GetGUID().GetCounter()),
                              proto->Quality, proto->ItemLevel);
        std::vector<float> reductions;
        for (UsableSubstance const& s : UsableSubstances(player))
            if (st.substanceMask & (1u << s.index))
                reductions.push_back(s.reduction);
        return MitigatedRisk(mgr.Cfg(), RiskFor(mgr.Cfg(), f), st.coins, reductions);
    }

    void SendTargetMenu(Player* player, Creature* creature)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        ClearGossipMenuFor(player);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            ItemTemplate const* proto = item->GetTemplate();
            std::string label;
            if (mod_property_override::NativeBudget(proto->Quality, proto->ItemLevel) <= 0.f)
                label = fmt::format("{}{}{}  {}too basic to infuse{}",
                                    QualityColor(proto->Quality), proto->Name1, COL_END,
                                    COL_GRAY, COL_END);
            else
            {
                auto rows = props.GetActiveOverrides(player, item->GetGUID().GetCounter());
                float f = MixFraction(rows, proto->Quality, proto->ItemLevel);
                label = fmt::format("{}{}{}  risk {}{:.0f}%{}",
                                    QualityColor(proto->Quality), proto->Name1, COL_END,
                                    COL_COST, RiskFor(mgr.Cfg(), f) * 100.f, COL_END);
            }
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, label, SENDER_TARGETS, slot);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.", SENDER_TARGETS, ACTION_CLOSE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendDonorMenu(Player* player, Creature* creature)
    {
        Pending& st = StateOf(player);
        Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, st.targetSlot);
        if (!target)
        {
            SendTargetMenu(player, creature);
            return;
        }

        std::vector<Item*> donors = EnumerateDonors(player);
        if (donors.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cffffd100[Infusion]|r You carry nothing with essence worth "
                "transferring - donors need native stats.");
            SendTargetMenu(player, creature);
            return;
        }

        size_t pages = (donors.size() + DONORS_PER_PAGE - 1) / DONORS_PER_PAGE;
        if (st.donorPage >= pages)
            st.donorPage = 0;

        ClearGossipMenuFor(player);
        // Header: the chosen target (clicking refreshes this menu).
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         fmt::format("Infusing: {}{}{}",
                                     QualityColor(target->GetTemplate()->Quality),
                                     target->GetTemplate()->Name1, COL_END),
                         SENDER_DONORS, ACTION_REFRESH);
        size_t begin = st.donorPage * DONORS_PER_PAGE;
        size_t end = std::min(begin + DONORS_PER_PAGE, donors.size());
        for (size_t i = begin; i < end; ++i)
        {
            ItemTemplate const* proto = donors[i]->GetTemplate();
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                             fmt::format("{}{}{}",
                                         QualityColor(proto->Quality), proto->Name1, COL_END),
                             SENDER_DONORS, ACTION_DONOR_BASE + i);
        }
        if (st.donorPage > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous page",
                             SENDER_DONORS, ACTION_PAGE_PREV);
        if (end < donors.size())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next page",
                             SENDER_DONORS, ACTION_PAGE_NEXT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_DONORS, ACTION_BACK_TARGETS);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendConfirmMenu(Player* player, Creature* creature)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        Pending& st = StateOf(player);

        Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, st.targetSlot);
        Item* donor = player->GetItemByGuid(st.donorGuid);
        if (!target || !donor)
        {
            SendTargetMenu(player, creature);
            return;
        }

        ItemTemplate const* tproto = target->GetTemplate();
        ItemTemplate const* dproto = donor->GetTemplate();
        auto yield = DonorYield(ItemInfusionMgr::CollectDonorStats(dproto),
                                mgr.Cfg().efficiency);
        auto rows = props.GetActiveOverrides(player, target->GetGUID().GetCounter());
        float risk = PendingRisk(player, st, target);

        ClearGossipMenuFor(player);
        // Headers (clicking refreshes).
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         fmt::format("Target: {}{}{}",
                                     QualityColor(tproto->Quality), tproto->Name1, COL_END),
                         SENDER_CONFIRM, ACTION_REFRESH);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         fmt::format("Sacrifice: {}{}{}",
                                     QualityColor(dproto->Quality), dproto->Name1, COL_END),
                         SENDER_CONFIRM, ACTION_REFRESH);
        // The transfer, in the WoW "Total (base+bonus)" grammar.
        for (auto const& e : yield)
        {
            int32 current = 0;
            for (auto const& row : rows)
                if (row.source == "mix" && row.property == static_cast<uint8>(e.prop))
                {
                    current = row.value;
                    break;
                }
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                             fmt::format("{}: {}",
                                         PropertyName(e.prop),
                                         FormatStatDisplay(player, e.prop,
                                                           static_cast<uint32>(e.amount),
                                                           current)),
                             SENDER_CONFIRM, ACTION_REFRESH);
        }

        // Mitigation. Coins first, then owned substances.
        uint32 coinsOwned = player->GetItemCount(mgr.CoinItemId(), false);
        if (st.coins < coinsOwned && risk > mgr.Cfg().riskFloor + 0.001f)
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
                             fmt::format("Pledge a Paragon Coin: pledged {}{}{} of {}",
                                         COL_COST, st.coins, COL_END, coinsOwned),
                             SENDER_CONFIRM, ACTION_COIN_ADD);
        else if (st.coins)
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
                             fmt::format("Pledged {}{}{} coins",
                                         COL_COST, st.coins, COL_END),
                             SENDER_CONFIRM, ACTION_REFRESH);
        if (st.coins)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Withdraw pledged coins",
                             SENDER_CONFIRM, ACTION_COIN_CLEAR);
        for (UsableSubstance const& s : UsableSubstances(player))
        {
            bool on = st.substanceMask & (1u << s.index);
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                             fmt::format("{} {} ({} owned)",
                                         on ? "Remove:" : "Add:", s.name, s.owned),
                             SENDER_CONFIRM, ACTION_SUB_BASE + s.index);
        }

        AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                         fmt::format("INFUSE - destruction risk {}{:.0f}%{}",
                                     COL_COST, risk * 100.f, COL_END),
                         SENDER_CONFIRM, ACTION_INFUSE);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_CONFIRM, ACTION_BACK_DONORS);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    class ItemInfusion_Alchemist : public CreatureScript
    {
    public:
        ItemInfusion_Alchemist() : CreatureScript("ItemInfusion_Alchemist") {}

        bool OnGossipHello(Player* player, Creature* creature) override
        {
            PendingByPlayer.erase(player->GetGUID().GetCounter());
            if (!ItemInfusionMgr::Instance().IsEnabled())
            {
                CloseGossipMenuFor(player);
                return true;
            }
            SendTargetMenu(player, creature);
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature,
                            uint32 sender, uint32 action) override
        {
            Pending& st = StateOf(player);

            if (sender == SENDER_TARGETS)
            {
                if (action < EQUIPMENT_SLOT_END)
                {
                    st = Pending{};
                    st.targetSlot = static_cast<uint8>(action);
                    SendDonorMenu(player, creature);
                }
                else
                    CloseGossipMenuFor(player);
                return true;
            }

            if (sender == SENDER_DONORS)
            {
                if (action == ACTION_BACK_TARGETS)
                    SendTargetMenu(player, creature);
                else if (action == ACTION_PAGE_PREV && st.donorPage > 0)
                {
                    --st.donorPage;
                    SendDonorMenu(player, creature);
                }
                else if (action == ACTION_PAGE_NEXT)
                {
                    ++st.donorPage;
                    SendDonorMenu(player, creature);
                }
                else if (action >= ACTION_DONOR_BASE)
                {
                    std::vector<Item*> donors = EnumerateDonors(player);
                    size_t idx = action - ACTION_DONOR_BASE;
                    if (idx < donors.size())
                    {
                        st.donorGuid = donors[idx]->GetGUID();
                        st.coins = 0;
                        st.substanceMask = 0;
                        SendConfirmMenu(player, creature);
                    }
                    else
                        SendDonorMenu(player, creature);
                }
                else
                    SendDonorMenu(player, creature); // header/refresh
                return true;
            }

            if (sender == SENDER_CONFIRM)
            {
                if (action == ACTION_BACK_DONORS)
                    SendDonorMenu(player, creature);
                else if (action == ACTION_COIN_ADD)
                {
                    ++st.coins;
                    SendConfirmMenu(player, creature);
                }
                else if (action == ACTION_COIN_CLEAR)
                {
                    st.coins = 0;
                    SendConfirmMenu(player, creature);
                }
                else if (action >= ACTION_SUB_BASE && action < ACTION_DONOR_BASE)
                {
                    st.substanceMask ^= 1u << (action - ACTION_SUB_BASE);
                    SendConfirmMenu(player, creature);
                }
                else if (action == ACTION_INFUSE)
                {
                    Item* target = player->GetItemByPos(INVENTORY_SLOT_BAG_0, st.targetSlot);
                    Item* donor = player->GetItemByGuid(st.donorGuid);
                    if (target && donor)
                    {
                        std::vector<uint32> subs;
                        for (UsableSubstance const& s : UsableSubstances(player))
                            if (st.substanceMask & (1u << s.index))
                                subs.push_back(s.itemId);
                        ItemInfusionMgr::Instance().TryInfuse(player, target, donor,
                                                              st.coins, subs);
                    }
                    // Whatever happened (success, destruction, refusal),
                    // start over from the target list with a clean selection.
                    PendingByPlayer.erase(player->GetGUID().GetCounter());
                    SendTargetMenu(player, creature);
                }
                else
                    SendConfirmMenu(player, creature); // header/refresh
                return true;
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    class ItemInfusion_PlayerCleanup : public PlayerScript
    {
    public:
        ItemInfusion_PlayerCleanup() : PlayerScript("ItemInfusion_PlayerCleanup") {}

        void OnPlayerLogout(Player* player) override
        {
            PendingByPlayer.erase(player->GetGUID().GetCounter());
        }
    };
}

void AddItemInfusionAlchemistScripts()
{
    new ItemInfusion_Alchemist();
    new ItemInfusion_PlayerCleanup();
}
