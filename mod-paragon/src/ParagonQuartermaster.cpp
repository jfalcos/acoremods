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
#include <optional>

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

    // Gossip color palette. The gossip frame is LIGHT parchment with dark
    // default text, so accents must be DARK to stay readable (bright chat
    // colors wash out here). Before/after is a muted-gray -> dark-green pair.
    // User-validated on the parchment: dark green reads well, epic purple
    // ok; mid grays and goldenrod wash out. So: "now" values take the plain
    // default dark text (color marks only what changes), dead rows use a
    // near-black gray, costs a deep bronze.
    constexpr char const* COL_GRAY  = "|cff333333"; // dead rows only
    constexpr char const* COL_GREEN = "|cff0a4a0a"; // "after" values
    constexpr char const* COL_COST  = "|cff4b0082"; // costs, budgets: deep violet (user pick)
    constexpr char const* COL_END   = "|r";

    // Item quality colors, darkened where the canonical shade is too light
    // for parchment. Common quality inherits the default dark text.
    char const* QualityColor(uint32 quality)
    {
        switch (quality)
        {
            case 0: return "|cff333333";
            case 1: return "";           // default dark text
            case 2: return "|cff0f5c0f"; // uncommon, darkened
            case 3: return "|cff004a99";
            case 4: return "|cff5c1a99";
            case 5: return "|cff8a4a00"; // legendary, darkened
            default: return "";
        }
    }

    struct LiveValue
    {
        double now;
        double delta;
        bool percent;
    };

    // Best-effort live character value + the delta this purchase adds.
    // Rating percents are derived from the player's own current
    // rating->bonus ratio, so no private APIs needed. std::nullopt when no
    // clean single value exists (tri-ratings, regen, zero ratings).
    std::optional<LiveValue> LiveValueFor(Player* player, upgrades::Property prop, uint32 chunk)
    {
        using P = upgrades::Property;
        auto flat = [&](double now) -> std::optional<LiveValue>
        { return LiveValue{ now, static_cast<double>(chunk), false }; };
        auto stat = [&](Stats s) { return flat(player->GetStat(s)); };
        auto rating = [&](CombatRating cr) -> std::optional<LiveValue>
        {
            uint32 cur = player->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr);
            float bonus = player->GetRatingBonusValue(cr);
            if (!cur || bonus <= 0.f)
                return std::nullopt;
            double delta = bonus * static_cast<double>(chunk) / static_cast<double>(cur);
            return LiveValue{ bonus, delta, true };
        };

        switch (prop)
        {
            case P::Strength:  return stat(STAT_STRENGTH);
            case P::Agility:   return stat(STAT_AGILITY);
            case P::Stamina:   return stat(STAT_STAMINA);
            case P::Intellect: return stat(STAT_INTELLECT);
            case P::Spirit:    return stat(STAT_SPIRIT);
            case P::Health:            return flat(player->GetMaxHealth());
            case P::Mana:              return flat(player->GetMaxPower(POWER_MANA));
            case P::AttackPower:       return flat(player->GetTotalAttackPowerValue(BASE_ATTACK));
            case P::RangedAttackPower: return flat(player->GetTotalAttackPowerValue(RANGED_ATTACK));
            case P::SpellPower:        return flat(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL));
            case P::Armor:             return flat(player->GetArmor());
            case P::HolyResistance:    return flat(player->GetResistance(SPELL_SCHOOL_HOLY));
            case P::FireResistance:    return flat(player->GetResistance(SPELL_SCHOOL_FIRE));
            case P::NatureResistance:  return flat(player->GetResistance(SPELL_SCHOOL_NATURE));
            case P::FrostResistance:   return flat(player->GetResistance(SPELL_SCHOOL_FROST));
            case P::ShadowResistance:  return flat(player->GetResistance(SPELL_SCHOOL_SHADOW));
            case P::ArcaneResistance:  return flat(player->GetResistance(SPELL_SCHOOL_ARCANE));
            case P::DefenseRating:     return rating(CR_DEFENSE_SKILL);
            case P::DodgeRating:       return rating(CR_DODGE);
            case P::ParryRating:       return rating(CR_PARRY);
            case P::BlockRating:       return rating(CR_BLOCK);
            case P::HitMeleeRating:    return rating(CR_HIT_MELEE);
            case P::HitRangedRating:   return rating(CR_HIT_RANGED);
            case P::HitSpellRating:    return rating(CR_HIT_SPELL);
            case P::CritMeleeRating:   return rating(CR_CRIT_MELEE);
            case P::CritRangedRating:  return rating(CR_CRIT_RANGED);
            case P::CritSpellRating:   return rating(CR_CRIT_SPELL);
            case P::HasteMeleeRating:  return rating(CR_HASTE_MELEE);
            case P::HasteRangedRating: return rating(CR_HASTE_RANGED);
            case P::HasteSpellRating:  return rating(CR_HASTE_SPELL);
            case P::ExpertiseRating:   return rating(CR_EXPERTISE);
            case P::ArmorPenetrationRating: return rating(CR_ARMOR_PENETRATION);
            default:
                return std::nullopt; // tri-ratings, regen, spell pen: no clean single value
        }
    }

    // WoW-convention stat display: "Total (base+bonus)" with the modified
    // total and the bonus in green, e.g. "Agility: 161 (156+5)". Falls back
    // to the item-bonus form "+15 (+10+5)" when no live value exists.
    std::string FormatStatDisplay(Player* player, upgrades::Property prop,
                                  uint32 chunk, int32 currentBonus)
    {
        if (auto live = LiveValueFor(player, prop, chunk))
        {
            if (live->percent)
                return fmt::format("{}{:.1f}%{} ({:.1f}%+{}{:.1f}%{})",
                                   COL_GREEN, live->now + live->delta, COL_END,
                                   live->now, COL_GREEN, live->delta, COL_END);
            return fmt::format("{}{}{} ({}+{}{}{})",
                               COL_GREEN, static_cast<int64>(live->now + live->delta), COL_END,
                               static_cast<int64>(live->now),
                               COL_GREEN, static_cast<int64>(live->delta), COL_END);
        }
        return fmt::format("{}+{}{} (+{}+{}{}{})",
                           COL_GREEN, currentBonus + static_cast<int32>(chunk), COL_END,
                           currentBonus, COL_GREEN, chunk, COL_END);
    }

    enum QmSenders
    {
        SENDER_MAIN  = 1,
        SENDER_PERKS = 2,
        SENDER_STOCK = 3,
        SENDER_UPG_SLOTS = 4,           // action = equipment slot -> category menu
        // Category pick for a slot: sender = SENDER_UPG_CAT_BASE + equipSlot,
        // action = category index (or ACTION_UPG_BACK).
        SENDER_UPG_CAT_BASE = 10,       // 10..28
        // Purchase: sender = SENDER_UPG_BUY_BASE + equipSlot, action = property id.
        SENDER_UPG_BUY_BASE = 40,       // 40..58
    };

    enum QmActions
    {
        ACTION_SHOW_PERKS    = GOSSIP_ACTION_INFO_DEF + 300,
        ACTION_SHOW_STOCK    = GOSSIP_ACTION_INFO_DEF + 301,
        ACTION_CLOSE         = GOSSIP_ACTION_INFO_DEF + 302,
        ACTION_SHOW_UPGRADES = GOSSIP_ACTION_INFO_DEF + 303,
        ACTION_UPG_BACK      = GOSSIP_ACTION_INFO_DEF + 304,
        ACTION_BACK_MAIN     = GOSSIP_ACTION_INFO_DEF + 305,
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
                         fmt::format("Character perks  {}{} coins{}",
                                     COL_COST,
                                     player->GetItemCount(pm.CoinItemId(), false), COL_END),
                         SENDER_MAIN, ACTION_SHOW_PERKS);
        if (pm.IsItemUpgradeEnabled())
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
                             "Upgrade an equipped item",
                             SENDER_MAIN, ACTION_SHOW_UPGRADES);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                         "Browse this week's collection (pets & mounts)",
                         SENDER_MAIN, ACTION_SHOW_STOCK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendUpgradeSlotMenu(Player* player, Creature* creature)
    {
        auto& pm = ParagonMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        ClearGossipMenuFor(player);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            ItemTemplate const* proto = item->GetTemplate();
            float budget = upgrades::UpgradeBudget(pm.UpgradeCfg(),
                                                   proto->Quality, proto->ItemLevel);
            float spent = upgrades::BudgetSpent(
                props.GetActiveOverrides(player, item->GetGUID().GetCounter()));
            // Keep the quality-colored name readable in all cases; annotate
            // WHY an item can't take upgrades instead of graying the row out.
            std::string label;
            if (budget < 5.f) // below the smallest chunk: item too weak
                label = fmt::format("{}{}{}  {}too basic to upgrade{}",
                                    QualityColor(proto->Quality), proto->Name1, COL_END,
                                    COL_GRAY, COL_END);
            else if (spent + 4.9f >= budget)
                label = fmt::format("{}{}{}  {}fully upgraded {:.0f}/{:.0f}{}",
                                    QualityColor(proto->Quality), proto->Name1, COL_END,
                                    COL_GRAY, spent, budget, COL_END);
            else
                label = fmt::format("{}{}{}  budget {}{:.0f}/{:.0f}{}",
                                    QualityColor(proto->Quality), proto->Name1, COL_END,
                                    COL_COST, spent, budget, COL_END);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, label,
                             SENDER_UPG_SLOTS, slot);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_MAIN, ACTION_UPG_BACK);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendUpgradeCategoryMenu(Player* player, Creature* creature, uint8 slot)
    {
        auto& pm = ParagonMgr::Instance();
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
        {
            SendUpgradeSlotMenu(player, creature);
            return;
        }
        // Items too weak for even one chunk get a clear message instead of
        // five categories of gray rows.
        ItemTemplate const* proto = item->GetTemplate();
        if (upgrades::UpgradeBudget(pm.UpgradeCfg(), proto->Quality, proto->ItemLevel) < 5.f)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffd100[Paragon]|r {} is too basic to hold upgrades - "
                "higher level and rarer items have larger budgets.", proto->Name1);
            SendUpgradeSlotMenu(player, creature);
            return;
        }
        ClearGossipMenuFor(player);
        for (uint8 c = 0; c < static_cast<uint8>(upgrades::Category::Max); ++c)
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                             upgrades::CategoryName(static_cast<upgrades::Category>(c)),
                             SENDER_UPG_CAT_BASE + slot, c);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_UPG_CAT_BASE + slot, ACTION_UPG_BACK);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void SendUpgradePropertyMenu(Player* player, Creature* creature, uint8 slot,
                                 upgrades::Category cat)
    {
        auto& pm = ParagonMgr::Instance();
        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
        {
            SendUpgradeSlotMenu(player, creature);
            return;
        }

        ItemTemplate const* proto = item->GetTemplate();
        auto rows = props.GetActiveOverrides(player, item->GetGUID().GetCounter());
        float budget = upgrades::UpgradeBudget(pm.UpgradeCfg(),
                                               proto->Quality, proto->ItemLevel);
        float spent = upgrades::BudgetSpent(rows);
        uint32 coins = upgrades::CostForNextChunk(budget > 0.f ? spent / budget : 1.f);

        ClearGossipMenuFor(player);
        // Info header (clicking harmlessly refreshes this menu).
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         fmt::format("{}{}{}  budget {}{:.0f}/{:.0f}{}",
                                     QualityColor(proto->Quality), proto->Name1, COL_END,
                                     COL_COST, spent, budget, COL_END),
                         SENDER_UPG_CAT_BASE + slot, static_cast<uint32>(cat));
        for (auto const& def : upgrades::DefsInCategory(cat))
        {
            int32 current = 0;
            for (auto const& row : rows)
                if (row.property == static_cast<uint8>(def.prop))
                {
                    current = row.value;
                    break;
                }
            // Row grammar: name, item bonus gray(now) green(after), your
            // character total in parens same pairing, cost in gold. Rows the
            // budget can't fit go entirely gray.
            float chunkCost = def.weight * static_cast<float>(def.chunk);
            bool fits = spent + chunkCost <= budget + 0.001f;
            std::string label;
            if (!fits)
                label = fmt::format("{}{} +{} (budget full){}",
                                    COL_GRAY, def.label, current, COL_END);
            else
                label = fmt::format("{}: {}  {}{} coins{}",
                                    def.label,
                                    FormatStatDisplay(player, def.prop, def.chunk, current),
                                    COL_COST, coins, COL_END);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, label,
                             SENDER_UPG_BUY_BASE + slot,
                             static_cast<uint32>(def.prop));
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_UPG_BUY_BASE + slot, ACTION_UPG_BACK);
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
            uint32 total = perks::TotalValue(cfg, i, ranks);
            // Same grammar as item upgrades: gray now, green after, gold cost;
            // maxed rows entirely gray.
            uint32 chunk = perks::TotalValue(cfg, i, ranks + 1) - total;
            std::string label = cost
                ? fmt::format("{}: {}  rank {}/{}, {}{} coins{}",
                              PropertyName(prop),
                              FormatStatDisplay(player, prop, chunk, static_cast<int32>(total)),
                              ranks, cfg.maxRanks,
                              COL_COST, cost, COL_END)
                : fmt::format("{}{} +{} (rank {}/{} MAX){}",
                              COL_GRAY, PropertyName(prop), total,
                              ranks, cfg.maxRanks, COL_END);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, label,
                             SENDER_PERKS, ACTION_PERK_BASE + i);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_MAIN, ACTION_BACK_MAIN);
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
                             fmt::format("{}{}{}  {}{} coins{}",
                                         QualityColor(tmpl ? tmpl->Quality : 1),
                                         tmpl ? tmpl->Name1 : "Unknown item", COL_END,
                                         COL_COST, StockCache[i].cost, COL_END),
                             SENDER_STOCK, ACTION_STOCK_BASE + i);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                         SENDER_MAIN, ACTION_BACK_MAIN);
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
                else if (action == ACTION_SHOW_UPGRADES)
                    SendUpgradeSlotMenu(player, creature);
                else if (action == ACTION_CLOSE)
                    CloseGossipMenuFor(player);
                else
                    SendMainMenu(player, creature); // "Back"
                return true;
            }

            if (sender == SENDER_UPG_SLOTS)
            {
                if (action < EQUIPMENT_SLOT_END)
                    SendUpgradeCategoryMenu(player, creature, static_cast<uint8>(action));
                else
                    SendMainMenu(player, creature);
                return true;
            }

            if (sender >= SENDER_UPG_CAT_BASE && sender < SENDER_UPG_CAT_BASE + EQUIPMENT_SLOT_END)
            {
                uint8 slot = static_cast<uint8>(sender - SENDER_UPG_CAT_BASE);
                if (action == ACTION_UPG_BACK)
                    SendUpgradeSlotMenu(player, creature);
                else if (action < static_cast<uint32>(upgrades::Category::Max))
                    SendUpgradePropertyMenu(player, creature, slot,
                                            static_cast<upgrades::Category>(action));
                else
                    SendUpgradeCategoryMenu(player, creature, slot);
                return true;
            }

            if (sender >= SENDER_UPG_BUY_BASE && sender < SENDER_UPG_BUY_BASE + EQUIPMENT_SLOT_END)
            {
                uint8 slot = static_cast<uint8>(sender - SENDER_UPG_BUY_BASE);
                if (action == ACTION_UPG_BACK)
                {
                    SendUpgradeCategoryMenu(player, creature, slot);
                    return true;
                }
                auto prop = static_cast<upgrades::Property>(action);
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    pm.TryPurchaseItemUpgrade(player, item, prop);
                    SendUpgradePropertyMenu(player, creature, slot,
                                            upgrades::CategoryOf(prop));
                }
                else
                    SendUpgradeSlotMenu(player, creature);
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
