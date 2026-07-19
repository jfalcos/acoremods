#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"
#include "TerrorZonesTeleportMgr.h"

#include "Player.h"
#include "ScriptMgr.h"

using namespace mod_terror_zones;

namespace
{
    class TerrorZones_PlayerScript : public PlayerScript
    {
    public:
        TerrorZones_PlayerScript() : PlayerScript("TerrorZones_PlayerScript") {}

        void OnPlayerLogin(Player* player) override
        {
            TerrorZonesMgr::Instance().OnPlayerLogin(player);
            TerrorZonesTeleportMgr::Instance().LoadTierTeleportProgress(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            if (!player)
                return;
            auto& prefs = TerrorZonesPlayerPrefsMgr::Instance();
            prefs.FlushPlayerPref(player);
            prefs.UnloadPlayerPref(player->GetGUID());
            TerrorZonesTeleportMgr::Instance().UnloadTierTeleportProgress(player->GetGUID());
        }

        void OnPlayerSave(Player* player) override
        {
            TerrorZonesPlayerPrefsMgr::Instance().FlushPlayerPref(player);
        }

        void OnPlayerUpdateZone(Player* player, uint32 newZone,
                                uint32 /*newArea*/) override
        {
            TerrorZonesMgr::Instance().OnPlayerUpdateZone(player, newZone);
        }
    };
}

void AddTerrorZonesPlayerScripts()
{
    new TerrorZones_PlayerScript();
}
