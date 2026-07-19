#include "TerrorZonesCombatMgr.h"
#include "TerrorZonesContractMgr.h"
#include "TerrorZonesMgr.h"
#include "TerrorZonesPlayerPrefsMgr.h"
#include "TerrorZonesTeleportMgr.h"
#include "TerrorZonesTierMgr.h"

#include "Log.h"
#include "ScriptMgr.h"

void AddTerrorZonesPlayerScripts();
void AddTerrorZonesCommandScripts();
void AddTerrorZonesCreatureScripts();
void AddTerrorZonesRewardScripts();
void AddTerrorZonesUnitScripts();
void AddTerrorZonesGossipScripts();
void AddTerrorZonesTeleportItemScripts();

namespace
{
    class TerrorZones_WorldScript : public WorldScript
    {
    public:
        TerrorZones_WorldScript()
            : WorldScript("TerrorZones_WorldScript",
                          { WORLDHOOK_ON_AFTER_CONFIG_LOAD,
                            WORLDHOOK_ON_STARTUP,
                            WORLDHOOK_ON_UPDATE }) {}

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            // PlayerPrefsMgr first: TerrorZonesMgr::LoadConfig()'s boot-log
            // line reads back GetGlobalAnnounceCategoryMask() to echo the
            // configured mask, so it needs the value already loaded.
            mod_terror_zones::TerrorZonesPlayerPrefsMgr::Instance().LoadConfig();
            mod_terror_zones::TerrorZonesMgr::Instance().LoadConfig();
            mod_terror_zones::TerrorZonesContractMgr::Instance().LoadConfig();
            mod_terror_zones::TerrorZonesTeleportMgr::Instance().LoadConfig();
            mod_terror_zones::TerrorZonesCombatMgr::Instance().LoadConfig();
            mod_terror_zones::TerrorZonesTierMgr::Instance().LoadConfig();
        }

        void OnStartup() override
        {
            mod_terror_zones::TerrorZonesMgr::Instance().InitializeOnStartup();
        }

        void OnUpdate(uint32 diff) override
        {
            mod_terror_zones::TerrorZonesMgr::Instance().OnUpdate(diff);
        }
    };
}

void Addmod_terror_zonesScripts()
{
    LOG_INFO("module", "mod-terror-zones: registering scripts.");
    new TerrorZones_WorldScript();
    AddTerrorZonesPlayerScripts();
    AddTerrorZonesCommandScripts();
    AddTerrorZonesCreatureScripts();
    AddTerrorZonesRewardScripts();
    AddTerrorZonesUnitScripts();
    AddTerrorZonesGossipScripts();
    AddTerrorZonesTeleportItemScripts();
}
