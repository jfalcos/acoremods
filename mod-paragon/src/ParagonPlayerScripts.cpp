#include "ParagonMgr.h"

#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

using namespace mod_paragon;

namespace
{
    // Playerbot sessions skip the login DB load and the XP hook unless
    // opted in (Paragon.ProcessBots). Logout stays ungated: UnloadPlayer
    // no-ops safely for never-loaded players.
    bool SkipBot(Player* player)
    {
        return !ParagonMgr::Instance().ProcessBots() &&
               player && player->GetSession() && player->GetSession()->IsBot();
    }

    class Paragon_PlayerScript : public PlayerScript
    {
    public:
        Paragon_PlayerScript() : PlayerScript("Paragon_PlayerScript") {}

        // XP -> PX divert. Runs on every XP grant: must stay allocation-free
        // and DB-free — all state is cached in ParagonMgr.
        void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/,
                            uint8 /*xpSource*/) override
        {
            auto& pm = ParagonMgr::Instance();
            if (!pm.IsEnabled() || !player || !amount || !player->GetSession())
                return;
            if (SkipBot(player))
                return;

            uint8 level = player->GetLevel();
            if (level < pm.MinToggleLevel())
                return;

            uint32 accountId = player->GetSession()->GetAccountId();
            if (pm.IsOptedOut(accountId))
                return;

            Map const* map = player->GetMap();
            if (map && map->IsBattlegroundOrArena())
                return;
            if (pm.IsMapBlocked(player->GetMapId()))
                return;

            uint32 allocPercent = std::min(pm.GetAllocPercent(accountId),
                                           pm.MaxAllocPercentFor(level));
            if (!allocPercent)
                return;

            uint32 diverted = static_cast<uint32>(
                static_cast<uint64>(amount) * allocPercent / 100);
            if (!diverted)
                return;

            uint64 pxGain = diverted;
            if (pm.XPGainScale())
                pxGain = pxGain * level / 80;

            pm.AddPX(player, pxGain);
            amount -= diverted;
        }

        void OnPlayerLogin(Player* player) override
        {
            if (SkipBot(player))
                return;
            auto& pm = ParagonMgr::Instance();
            pm.LoadPlayer(player);
            pm.OnLogin(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            ParagonMgr::Instance().UnloadPlayer(player);
        }
    };
}

void AddParagonPlayerScripts()
{
    new Paragon_PlayerScript();
}
