#include "ItemInfusionMgr.h"

#include "Log.h"
#include "ScriptMgr.h"

void AddItemInfusionAlchemistScripts();
void AddItemInfusionAddonScripts();
void AddItemInfusionCommandScripts();

namespace
{
    class ItemInfusion_WorldScript : public WorldScript
    {
    public:
        ItemInfusion_WorldScript()
            : WorldScript("ItemInfusion_WorldScript",
                          { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) {}

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            mod_item_infusion::ItemInfusionMgr::Instance().LoadConfig();
        }
    };
}

void Addmod_item_infusionScripts()
{
    LOG_INFO("module", "mod-item-infusion: registering scripts.");
    new ItemInfusion_WorldScript();
    AddItemInfusionAlchemistScripts();
    AddItemInfusionAddonScripts();
    AddItemInfusionCommandScripts();
}
