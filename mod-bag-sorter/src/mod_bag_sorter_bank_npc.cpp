/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Dedicated "Bag & Bank Organizer" NPC (see data/sql/db-world for the spawn).
// Deposit/organize-bank used to be appended to the real banker's gossip, but
// that required forcing the Gossip npcflag onto every banker just to make a
// gossip box appear at all - confusing, since the box then mixed our options
// with (or replaced) the plain "open my bank" click players expected. A
// dedicated NPC standing next to the banker avoids all of that: it owns its
// own gossip menu outright, no flag hacks, no native menu to coexist with.

#include "mod_bag_sorter.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedGossip.h"

namespace
{
    enum BankOrganizerGossip : uint32
    {
        SENDER_BANK_ORGANIZER   = 1,

        ACTION_DEPOSIT          = 100,
        ACTION_SORT_OPEN        = 101,
        ACTION_SORT_TYPE        = 102,
        ACTION_SORT_QUALITY     = 103,
        ACTION_SORT_ILVL        = 104,
        ACTION_SORT_NAME        = 105,
        ACTION_CANCEL           = 106,

        NPC_TEXT_GREETING       = 900001, // npc_text/gossip_menu id, see SQL
    };

    void RunDepositAndReport(Player* player)
    {
        uint32 const count = BagSorter::DepositToBank(player);

        if (BagSorter::settings.Announce)
        {
            ChatHandler handler(player->GetSession());
            if (count > 0)
                handler.PSendSysMessage("Deposited {} item(s) into your bank.", count);
            else
                handler.SendSysMessage("Nothing could be deposited.");
        }

        CloseGossipMenuFor(player);
    }

    void RunBankSortAndReport(Player* player, BagSorter::SortMode mode)
    {
        uint32 const count = BagSorter::SortBank(player, mode);

        if (BagSorter::settings.Announce)
        {
            ChatHandler handler(player->GetSession());
            if (count > 0)
                handler.PSendSysMessage("Your bank has been organized ({} items).", count);
            else
                handler.SendSysMessage("There was nothing to organize.");
        }

        CloseGossipMenuFor(player);
    }

    class npc_bag_bank_organizer : public CreatureScript
    {
    public:
        npc_bag_bank_organizer() : CreatureScript("npc_bag_bank_organizer") { }

        bool OnGossipHello(Player* player, Creature* creature) override
        {
            ClearGossipMenuFor(player);

            if (!BagSorter::settings.BankEnable)
            {
                ChatHandler(player->GetSession()).SendNotification("Bank organizing is currently disabled.");
                SendGossipMenuFor(player, NPC_TEXT_GREETING, creature->GetGUID());
                return true;
            }

            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Deposit everything into my bank",
                SENDER_BANK_ORGANIZER, ACTION_DEPOSIT);
            AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Organize my bank", SENDER_BANK_ORGANIZER, ACTION_SORT_OPEN);

            SendGossipMenuFor(player, NPC_TEXT_GREETING, creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
        {
            if (sender != SENDER_BANK_ORGANIZER)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            switch (action)
            {
                case ACTION_DEPOSIT:
                    RunDepositAndReport(player);
                    break;
                case ACTION_SORT_OPEN:
                    ClearGossipMenuFor(player);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "By type, then quality",
                        SENDER_BANK_ORGANIZER, ACTION_SORT_TYPE);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "By quality", SENDER_BANK_ORGANIZER, ACTION_SORT_QUALITY);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "By item level", SENDER_BANK_ORGANIZER, ACTION_SORT_ILVL);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "By name (A-Z)", SENDER_BANK_ORGANIZER, ACTION_SORT_NAME);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind", SENDER_BANK_ORGANIZER, ACTION_CANCEL);
                    SendGossipMenuFor(player, NPC_TEXT_GREETING, creature->GetGUID());
                    break;
                case ACTION_SORT_TYPE:
                    RunBankSortAndReport(player, BagSorter::SortMode::TypeQuality);
                    break;
                case ACTION_SORT_QUALITY:
                    RunBankSortAndReport(player, BagSorter::SortMode::Quality);
                    break;
                case ACTION_SORT_ILVL:
                    RunBankSortAndReport(player, BagSorter::SortMode::ItemLevel);
                    break;
                case ACTION_SORT_NAME:
                    RunBankSortAndReport(player, BagSorter::SortMode::Name);
                    break;
                case ACTION_CANCEL:
                default:
                    CloseGossipMenuFor(player);
                    break;
            }

            return true;
        }
    };
}

void AddSC_mod_bag_sorter_bank_npc()
{
    new npc_bag_bank_organizer();
}
