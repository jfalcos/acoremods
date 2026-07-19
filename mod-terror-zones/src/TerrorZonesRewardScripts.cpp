#include "TerrorZonesCombatMgr.h"
#include "TerrorZonesContractMgr.h"
#include "TerrorZonesMgr.h"

#include "Creature.h"
#include "LootMgr.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace mod_terror_zones;

namespace
{
    class TerrorZones_RewardPlayerScript : public PlayerScript
    {
    public:
        TerrorZones_RewardPlayerScript()
            : PlayerScript("TerrorZones_RewardPlayerScript") {}

        void OnPlayerGiveXP(Player* player, uint32& amount,
                            Unit* /*victim*/, uint8 /*xpSource*/) override
        {
            TerrorZonesMgr::Instance().ApplyXpMultiplier(amount, player);
        }

        void OnPlayerBeforeLootMoney(Player* player, Loot* loot) override
        {
            TerrorZonesMgr::Instance().ApplyGoldMultiplier(loot, player);
        }

        // Event-boss gold uplift fires here. OnPlayerCreatureKill
        // runs at Unit.cpp:14309 — well AFTER both FillLoot
        // (Unit.cpp:14081) and generateMoneyLoot (Unit.cpp:14084),
        // so our mutation of creature->loot.gold sticks for the
        // loot window opening. The earlier OnPlayerBeforeLootMoney
        // path was unreliable because if the gold pile showed a
        // small native amount, players ignored it instead of
        // clicking — and our uplift only fired on click.
        void OnPlayerCreatureKill(Player* killer,
                                   Creature* killed) override
        {
            if (!killer || !killed)
                return;
            auto& mgr = TerrorZonesMgr::Instance();
            // Slice 10 — kill-time loot-gold floor. Runs before the
            // event-boss uplift so a boss kill takes the larger of the
            // two (the uplift's max() is idempotent over the floor).
            mgr.ApplyKillGoldFloor(killed, killer);
            mgr.ApplyEventBossGoldUplift(killed->loot, killer);
            // Slice 10 Pass 2 — accrue per-TZ contract credit (write-through).
            TerrorZonesContractMgr::Instance().AccrueContractCredit(killed, killer);
            // Slice 10 Pass 3 — release group-scale tracking so a respawn
            // of this creature re-scales for groups within the rotation.
            TerrorZonesCombatMgr::Instance().OnCreatureKilled(killed);
        }

        void OnPlayerQuestComputeMoney(Player* player, Quest const* /*quest*/,
                                       int32& moneyRew) override
        {
            TerrorZonesMgr::Instance().ApplyQuestGoldMultiplier(moneyRew,
                                                                 player);
        }
    };

    class TerrorZones_RewardGlobalScript : public GlobalScript
    {
    public:
        TerrorZones_RewardGlobalScript()
            : GlobalScript("TerrorZones_RewardGlobalScript") {}

        void OnBeforeDropAddItem(Player const* player, Loot& loot,
                                 bool /*canRate*/, uint16 /*lootMode*/,
                                 LootStoreItem* item,
                                 LootStore const& store) override
        {
            if (!player)
                return;
            auto& mgr = TerrorZonesMgr::Instance();
            // Slice 3 — tier-bump substitution.
            mgr.TryTierBump(player, item);
            // Slice 4 — Prospector's gathering yield overlay.
            mgr.TryGatheringYieldBump(player, loot, item, store.GetName());
            // Slice 4 — additive unique drop (per-bundle, not per-item).
            mgr.TryUniqueDrop(player, &loot, player->GetZoneId());
            // Slice 6 — additive event-boss bonus drop (no-op in MVP,
            // Pass-2 seam).
            mgr.TryEventBossDrop(player, loot);
        }
    };

    // Audit F5 fix — reverts TryTierBump's shared-template mutations.
    // `tab->Process(...)` (LootMgr.cpp) runs every OnBeforeDropAddItem
    // call for the WHOLE loot-template tree (including nested reference
    // templates), then fires OnAfterLootTemplateProcess exactly once,
    // right after, with the fully-built Loot* -- the correct "this pass
    // is done, clean up the shared template now" signal. Separate
    // MiscScript (not GlobalScript, which doesn't declare this hook).
    class TerrorZones_LootCleanupScript : public MiscScript
    {
    public:
        TerrorZones_LootCleanupScript()
            : MiscScript("TerrorZones_LootCleanupScript") {}

        void OnAfterLootTemplateProcess(Loot* /*loot*/,
                                        LootTemplate const* /*tab*/,
                                        LootStore const& /*store*/,
                                        Player* /*lootOwner*/,
                                        bool /*personal*/,
                                        bool /*noEmptyError*/,
                                        uint16 /*lootMode*/) override
        {
            TerrorZonesMgr::Instance().RevertTierBumps();
        }
    };
}

void AddTerrorZonesRewardScripts()
{
    new TerrorZones_RewardPlayerScript();
    new TerrorZones_RewardGlobalScript();
    new TerrorZones_LootCleanupScript();
}
