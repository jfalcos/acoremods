#include "ParagonMgr.h"

#include "Log.h"
#include "ScriptMgr.h"

void AddParagonPlayerScripts();
void AddParagonCommandScripts();
void AddParagonVendorScripts();

namespace
{
    class Paragon_WorldScript : public WorldScript
    {
    public:
        Paragon_WorldScript()
            : WorldScript("Paragon_WorldScript",
                          { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) {}

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            mod_paragon::ParagonMgr::Instance().LoadConfig();
        }
    };
}

void Addmod_paragonScripts()
{
    LOG_INFO("module", "mod-paragon: registering scripts.");
    new Paragon_WorldScript();
    AddParagonPlayerScripts();
    AddParagonCommandScripts();
    AddParagonVendorScripts();
}
