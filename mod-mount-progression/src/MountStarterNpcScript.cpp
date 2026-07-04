#include "MountProgressionMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

using namespace mod_mount_progression;

namespace
{
    enum MountStarterMisc
    {
        GOSSIP_SENDER_STARTER_CHOICE = 1,
        GOSSIP_ACTION_STAMINA        = GOSSIP_ACTION_INFO_DEF + 1,
        GOSSIP_ACTION_PREDATOR       = GOSSIP_ACTION_INFO_DEF + 2,
        GOSSIP_ACTION_ARCANE         = GOSSIP_ACTION_INFO_DEF + 3,

        NPC_TEXT_MOUNT_TAMER_GREETING = 900000,  // npc_text/gossip_menu id, see SQL
    };

    class npc_mount_tamer : public CreatureScript
    {
    public:
        npc_mount_tamer() : CreatureScript("npc_mount_tamer") { }

        bool OnGossipHello(Player* player, Creature* creature) override
        {
            ClearGossipMenuFor(player);
            auto& mgr = MountProgressionMgr::Instance();

            if (!mgr.IsEnabled())
            {
                ChatHandler(player->GetSession()).SendNotification(
                    "Mount progression is currently disabled.");
                return true;
            }

            if (mgr.HasMadeStarterChoice(player))
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                    "You've already chosen your first mount. I have nothing more for you.",
                    GOSSIP_SENDER_STARTER_CHOICE, GOSSIP_ACTION_INFO_DEF);
            }
            else
            {
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                    "I'll take the steady one. (Stamina - sturdy, built to endure.)",
                    GOSSIP_SENDER_STARTER_CHOICE, GOSSIP_ACTION_STAMINA);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                    "I'll take the fierce one. (Predator - rewards the hunter's instinct.)",
                    GOSSIP_SENDER_STARTER_CHOICE, GOSSIP_ACTION_PREDATOR);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                    "I'll take the curious one. (Arcane - its bones hum with old magic.)",
                    GOSSIP_SENDER_STARTER_CHOICE, GOSSIP_ACTION_ARCANE);
            }

            SendGossipMenuFor(player, NPC_TEXT_MOUNT_TAMER_GREETING, creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 sender, uint32 action) override
        {
            ClearGossipMenuFor(player);
            if (sender != GOSSIP_SENDER_STARTER_CHOICE)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            auto& mgr = MountProgressionMgr::Instance();
            uint32 spellId = 0;
            switch (action)
            {
                case GOSSIP_ACTION_STAMINA:  spellId = mgr.GetStarterSpell(MountType::Stamina);  break;
                case GOSSIP_ACTION_PREDATOR: spellId = mgr.GetStarterSpell(MountType::Predator); break;
                case GOSSIP_ACTION_ARCANE:   spellId = mgr.GetStarterSpell(MountType::Arcane);   break;
                default:
                    CloseGossipMenuFor(player);
                    return true;
            }

            CatalogEntry const* entry = spellId ? mgr.GrantStarterMount(player, spellId) : nullptr;
            if (entry)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff40ff80[Mount]|r {} has bonded with your |cffffd100{}|r!",
                    player->GetName(), entry->displayName);
                mgr.CompleteStarterQuest(player);
            }
            else
            {
                ChatHandler(player->GetSession()).SendNotification(
                    "You've already made your choice, or mount progression is disabled.");
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };
}

void AddMountProgressionStarterNpcScript()
{
    new npc_mount_tamer();
}
