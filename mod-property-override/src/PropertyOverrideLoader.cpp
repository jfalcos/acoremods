#include "PropertyOverrideMgr.h"

#include "Log.h"
#include "ScriptMgr.h"

void AddPropertyOverridePlayerScripts();
void AddPropertyOverrideItemScripts();
void AddPropertyOverrideCommandScripts();

namespace
{
    class PO_WorldScript : public WorldScript
    {
    public:
        PO_WorldScript()
            : WorldScript("PO_WorldScript",
                          { WORLDHOOK_ON_AFTER_CONFIG_LOAD,
                            WORLDHOOK_ON_STARTUP }) {}

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            mod_property_override::PropertyOverrideMgr::Instance().LoadConfig();
        }

        void OnStartup() override
        {
            mod_property_override::PropertyOverrideMgr::Instance().StartupCleanup();
        }
    };
}

void Addmod_property_overrideScripts()
{
    LOG_INFO("module", "mod-property-override: registering scripts.");
    new PO_WorldScript();
    AddPropertyOverridePlayerScripts();
    AddPropertyOverrideItemScripts();
    AddPropertyOverrideCommandScripts();
}
