#include "MountProgressionMgr.h"

#include "Creature.h"
#include "Item.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "WorldSession.h"

using namespace mod_mount_progression;

namespace
{
    // Playerbot sessions skip every hook unless opted in
    // (MountProgression.ProcessBots): bots mount constantly, and without
    // this gate the random-bot population accrues mount XP, writes DB
    // state, and gains bond buffs. Logout stays ungated (save/unload
    // no-op for never-loaded state, self-healing on config flips).
    bool SkipBot(Player* player)
    {
        return !MountProgressionMgr::Instance().ProcessBots() &&
               player && player->GetSession() && player->GetSession()->IsBot();
    }

    class Mount_PlayerScript : public PlayerScript
    {
    public:
        Mount_PlayerScript() : PlayerScript("Mount_PlayerScript") {}

        void OnPlayerLogin(Player* player) override
        {
            if (SkipBot(player))
                return;
            auto& mgr = MountProgressionMgr::Instance();
            mgr.LoadPlayerState(player);
            mgr.LoadActiveMountFromDB(player);
            mgr.MaybeSendStarterQuest(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            if (!player)
                return;
            auto& mgr = MountProgressionMgr::Instance();
            mgr.SavePlayerState(player);
            mgr.SaveActiveMountToDB(player);
            mgr.UnloadPlayerState(player->GetGUID());
        }

        void OnPlayerReleasedGhost(Player* player) override
        {
            if (!player || SkipBot(player))
                return;
            auto& mgr = MountProgressionMgr::Instance();
            uint32 spellId = mgr.GetActiveMount(player);
            if (!spellId)
                return;
            CatalogEntry const* entry = mgr.GetCatalogEntry(spellId);
            if (!entry)
                return;
            // Aura only: the stat rows never dropped through death, so a full
            // reapply would just churn the override table.
            mgr.ApplyMountBuffAura(player, entry);
        }

        void OnPlayerSave(Player* player) override
        {
            MountProgressionMgr::Instance().SavePlayerState(player);
        }

        void OnPlayerCreatureKill(Player* player, Creature* killed) override
        {
            if (SkipBot(player))
                return;
            MountProgressionMgr::Instance().OnCreatureKill(player, killed);
        }

        void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
        {
            if (!player || !spell || SkipBot(player))
                return;
            SpellInfo const* info = spell->GetSpellInfo();
            if (!info)
                return;
            MountProgressionMgr::Instance().OnSpellCast(player, info);
        }

        void OnPlayerUpdate(Player* player, uint32 p_time) override
        {
            if (SkipBot(player))
                return;
            MountProgressionMgr::Instance().OnPlayerTick(player, p_time);
        }

        void OnPlayerUpdateArea(Player* player, uint32 oldArea, uint32 newArea) override
        {
            if (SkipBot(player))
                return;
            MountProgressionMgr::Instance().OnPlayerAreaChange(player, oldArea, newArea);
        }

        void OnPlayerLootItem(Player* player, Item* /*item*/, uint32 /*count*/,
                              ObjectGuid lootguid) override
        {
            if (SkipBot(player))
                return;
            MountProgressionMgr::Instance().OnPlayerLoot(player, lootguid);
        }
    };
}

void AddMountProgressionPlayerScripts()
{
    new Mount_PlayerScript();
}
