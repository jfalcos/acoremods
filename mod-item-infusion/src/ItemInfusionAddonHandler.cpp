#include "ItemInfusionAddonMsg.h"
#include "ItemInfusionMgr.h"
#include "PropertyOverrideMgr.h"

#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

#include <algorithm>
#include <cmath>

using namespace mod_item_infusion;

namespace
{
    // Client conventions, identical to the IPROP handler: equipped inv-slot
    // ids are 1-based (1 = head ... 19 = tabard); bag 0 = backpack,
    // 1-4 = equipped bags, bag slots 1-based.
    Item* ResolveTarget(Player* player, uint32 invSlot)
    {
        if (invSlot < 1 || invSlot > EQUIPMENT_SLOT_END)
            return nullptr;
        return player->GetItemByPos(INVENTORY_SLOT_BAG_0,
                                    static_cast<uint8>(invSlot - 1));
    }

    Item* ResolveDonor(Player* player, uint32 bag, uint32 slot)
    {
        if (slot < 1)
            return nullptr;
        if (bag == 0)
        {
            uint32 pos = INVENTORY_SLOT_ITEM_START + (slot - 1);
            if (pos >= INVENTORY_SLOT_ITEM_END)
                return nullptr;
            return player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(pos));
        }
        if (bag > 4)
            return nullptr;
        return player->GetItemByPos(
            static_cast<uint8>(INVENTORY_SLOT_BAG_START + (bag - 1)),
            static_cast<uint8>(slot - 1));
    }

    void Send(Player* player, std::string const& msg)
    {
        mod_property_override::PropertyOverrideMgr::Instance().SendAddonMessage(player, msg);
    }

    uint32 Pct(float f) { return static_cast<uint32>(std::lround(f * 100.f)); }

    // Substances usable RIGHT NOW for this gear: configured, live template,
    // owned, and potent enough. `reductions` gets the mask's active ones.
    std::vector<addon::SubstanceView> SubstanceState(
        Player* player, uint32 gearReqLevel, uint32 mask, std::vector<float>& reductions)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        std::vector<addon::SubstanceView> views;
        auto const& subs = mgr.Substances();
        for (uint32 i = 0; i < subs.size(); ++i)
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(subs[i].first);
            if (!tmpl || !player->GetItemCount(subs[i].first, false))
                continue;
            bool eligible = SubstanceEffective(mgr.Cfg(), tmpl->ItemLevel, gearReqLevel);
            views.push_back({ i, subs[i].first, Pct(subs[i].second), eligible });
            if (eligible && (mask & (1u << i)))
                reductions.push_back(subs[i].second);
        }
        return views;
    }

    void HandlePreview(Player* player, addon::Request const& req)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        if (!mgr.IsEnabled())
        {
            Send(player, addon::BuildRefusal("OFF"));
            return;
        }
        Item* target = ResolveTarget(player, req.targetInvSlot);
        Item* donor = ResolveDonor(player, req.donorBag, req.donorSlot);
        if (!target || !donor || target->GetGUID() == donor->GetGUID())
        {
            Send(player, addon::BuildRefusal("NOITEM"));
            return;
        }
        if (player->GetLevel() < mgr.MinLevel())
        {
            Send(player, addon::BuildRefusal("LEVEL"));
            return;
        }

        ItemTemplate const* tproto = target->GetTemplate();
        ItemTemplate const* dproto = donor->GetTemplate();
        if (mod_property_override::NativeBudget(tproto->Quality, tproto->ItemLevel) <= 0.f)
        {
            Send(player, addon::BuildRefusal("BASIC"));
            return;
        }
        auto yield = DonorYield(ItemInfusionMgr::CollectDonorStats(dproto),
                                mgr.Cfg().efficiency);
        if (yield.empty())
        {
            Send(player, addon::BuildRefusal("NOYIELD"));
            return;
        }

        auto& props = mod_property_override::PropertyOverrideMgr::Instance();
        auto rows = props.GetActiveOverrides(player, target->GetGUID().GetCounter());
        float f = MixFraction(rows, tproto->Quality, tproto->ItemLevel);
        float penalty = mgr.MasteryPenaltyFor(player, tproto, dproto);
        uint32 gearReq = std::max(tproto->RequiredLevel, dproto->RequiredLevel);

        std::vector<float> reductions;
        auto subViews = SubstanceState(player, gearReq, req.substanceMask, reductions);

        float base = RiskFor(mgr.Cfg(), f, penalty);
        float risk = MitigatedRisk(mgr.Cfg(), base, req.coins, reductions);

        float native = mod_property_override::NativeBudget(tproto->Quality, tproto->ItemLevel);
        float mixPts = mod_property_override::BudgetSpent(rows, "mix");

        Send(player, addon::BuildHeader(Pct(risk), Pct(base), Pct(penalty),
                                        static_cast<uint32>(std::lround(mixPts)),
                                        static_cast<uint32>(std::lround(native))));
        std::vector<addon::YieldView> yv;
        for (auto const& e : yield)
            yv.push_back({ static_cast<uint8>(e.prop), e.amount });
        for (std::string const& m : addon::BuildYield(yv))
            Send(player, m);
        for (std::string const& m : addon::BuildSubstances(subViews))
            Send(player, m);
    }

    void HandleExecute(Player* player, addon::Request const& req)
    {
        auto& mgr = ItemInfusionMgr::Instance();
        Item* target = ResolveTarget(player, req.targetInvSlot);
        Item* donor = ResolveDonor(player, req.donorBag, req.donorSlot);
        if (!target || !donor)
        {
            Send(player, addon::BuildExecuteResult("ERR\tNOITEM"));
            return;
        }

        // Mask -> eligible substance item ids (drop stale toggles, same
        // stance as the gossip INFUSE click).
        uint32 gearReq = std::max(target->GetTemplate()->RequiredLevel,
                                  donor->GetTemplate()->RequiredLevel);
        std::vector<float> unusedReductions;
        std::vector<uint32> subIds;
        for (addon::SubstanceView const& v :
             SubstanceState(player, gearReq, req.substanceMask, unusedReductions))
            if (v.eligible && (req.substanceMask & (1u << v.index)))
                subIds.push_back(v.itemId);

        switch (mgr.TryInfuse(player, target, donor, req.coins, subIds))
        {
            case ItemInfusionMgr::InfuseResult::Survived:
                Send(player, addon::BuildExecuteResult("OK"));
                break;
            case ItemInfusionMgr::InfuseResult::Destroyed:
                Send(player, addon::BuildExecuteResult("DEAD"));
                break;
            default:
                Send(player, addon::BuildExecuteResult("ERR\tREJ"));
                break;
        }
    }

    class ItemInfusion_AddonScript : public PlayerScript
    {
    public:
        ItemInfusion_AddonScript() : PlayerScript("ItemInfusion_AddonScript") {}

        [[nodiscard]] bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang,
                                              std::string& msg, Player* receiver) override
        {
            if (lang != LANG_ADDON || receiver != player || !addon::IsAddonMessage(msg))
                return true;

            addon::Request req = addon::ParseRequest(msg);
            if (req.kind == addon::Request::Kind::Preview)
                HandlePreview(player, req);
            else if (req.kind == addon::Request::Kind::Execute)
                HandleExecute(player, req);
            return false; // malformed IFUSE traffic is swallowed silently
        }
    };
}

void AddItemInfusionAddonScripts()
{
    new ItemInfusion_AddonScript();
}
