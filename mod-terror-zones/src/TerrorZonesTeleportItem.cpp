// Terror Zone Beacon — the single item that delivers every tier's
// teleport unlock. Using it opens a gossip menu listing only the tiers
// TerrorZonesMgr::IsTierUnlockedFor says this character has earned;
// picking one calls TeleportPlayerToTier. One item covers all 5 tiers,
// so there's no per-tier item (or spell) to hijack/maintain.

#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Item.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "StringFormat.h"

using namespace mod_terror_zones;

namespace
{
    // Arbitrary, module-owned gossip text id (npc_text 900100) — the
    // beacon's greeting line. Options themselves are built dynamically
    // per-player in OnUse, not stored in gossip_menu_option.
    constexpr uint32 TZ_BEACON_TEXT_ID = 900100;

    class item_tz_teleport_beacon : public ItemScript
    {
    public:
        item_tz_teleport_beacon() : ItemScript("item_tz_teleport_beacon") { }

        bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
        {
            if (!player)
                return false;

            // We never call CastItemUseSpell (we always return true below), so the
            // client's optimistic cast bar / item cooldown swipe for the donor spell
            // is left dangling with no server confirmation. Cancel it explicitly —
            // see the "prevent stuck item in gray state" note in
            // WorldSession::HandleUseItemOpcode (SpellHandler.cpp) — and clear any
            // cooldown the client may have already applied for that spell so using
            // the beacon never eats into (or gets blocked by) the real donor item's
            // own cooldown.
            if (SpellInfo const* donorSpell = sSpellMgr->GetSpellInfo(item->GetTemplate()->Spells[0].SpellId))
            {
                Spell::SendCastResult(player, donorSpell, 1, SPELL_FAILED_DONT_REPORT);
                player->RemoveSpellCooldown(donorSpell->Id, true);
            }

            auto& mgr = TerrorZonesMgr::Instance();
            ClearGossipMenuFor(player);

            bool any = false;
            for (uint8 tier = TIER_1; tier <= TIER_MAX; ++tier)
            {
                if (!mgr.IsTierUnlockedFor(player, tier))
                    continue;
                any = true;
                AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                                  Acore::StringFormat("Teleport to Tier {} terror", tier),
                                  GOSSIP_SENDER_MAIN, tier);
            }

            if (!any)
            {
                ChatHandler(player->GetSession())
                    .PSendSysMessage(
                        "|cff33ff99[Terror Zone]|r You haven't unlocked any "
                        "terror zone tiers yet.");
                return true;
            }

            SendGossipMenuFor(player, TZ_BEACON_TEXT_ID, item->GetGUID());
            return true;
        }

        void OnGossipSelect(Player* player, Item* /*item*/, uint32 /*sender*/, uint32 action) override
        {
            CloseGossipMenuFor(player);
            TerrorZonesMgr::Instance().TeleportPlayerToTier(
                player, static_cast<uint8>(action));
        }
    };
}

void AddTerrorZonesTeleportItemScripts()
{
    new item_tz_teleport_beacon();
}
