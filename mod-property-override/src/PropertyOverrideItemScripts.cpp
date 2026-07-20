#include "PropertyOverrideMgr.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace mod_property_override;

namespace
{
    class PO_AllItemScript : public AllItemScript
    {
    public:
        PO_AllItemScript() : AllItemScript("PO_AllItemScript") {}

        // Fires from Player::DestroyItem while the item is still in its slot
        // (before _ApplyItemMods(false)); the return value is ignored at that
        // call site, so this is a pre-removal notification.
        [[nodiscard]] bool CanItemRemove(Player* player, Item* item) override
        {
            PropertyOverrideMgr::Instance().HandleItemDestroyed(player, item);
            return true;
        }
    };

    class PO_GlobalScript : public GlobalScript
    {
    public:
        PO_GlobalScript() : GlobalScript("PO_GlobalScript",
                                         { GLOBALHOOK_ON_ITEM_DEL_FROM_DB }) {}

        // Runs inside the transaction that deletes the item_instance row —
        // atomic purge. May fire off the world thread, so touch only the
        // transaction, never manager state.
        void OnItemDelFromDB(CharacterDatabaseTransaction trans,
                             ObjectGuid::LowType itemGuid) override
        {
            trans->Append("DELETE FROM item_property_override WHERE item_guid = {}",
                          itemGuid);
        }
    };
}

void AddPropertyOverrideItemScripts()
{
    new PO_AllItemScript();
    new PO_GlobalScript();
}
