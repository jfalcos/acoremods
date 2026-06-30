// Innkeeper gossip surface for mod-terror-zones. Mirrors the read-only
// `.zones` / `.zones next` / `.zones history` commands into an innkeeper
// gossip menu so regular players (not just GMs) can read what's terrorized,
// when the next rotation lands, recent history, and how the system works.
//
// Coexistence: this uses the additive `OnCreatureGossipHelloAppend` hook
// (acoremods core patch) so it stacks alongside other modules' innkeeper
// options (e.g. mod-bag-sorter) instead of first-wins clobbering. The core
// prepares + sends the native menu; we only AddGossipItemFor.

#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

using namespace mod_terror_zones;

namespace
{
    // Distinctive sender so our selections never collide with the
    // innkeeper's native DB options (sender 0) or other modules.
    enum TerrorZonesGossip : uint32
    {
        SENDER_TZ          = 0x7202,   // "TZ"

        ACTION_TZ_OPEN     = 7200,
        ACTION_TZ_INFO     = 7201,
        ACTION_TZ_NEXT     = 7202,
        ACTION_TZ_HISTORY  = 7203,
        ACTION_TZ_HOW      = 7204,
        ACTION_TZ_BACK     = 7205,
        ACTION_TZ_CLOSE    = 7206,
    };

    std::string FormatRemaining(uint32 secs)
    {
        if (secs >= 3600)
        {
            uint32 h = secs / 3600;
            uint32 m = (secs % 3600) / 60;
            if (m == 0)
                return (h == 1) ? "1 hour" : std::to_string(h) + " hours";
            return std::to_string(h) + "h " + std::to_string(m) + "m";
        }
        if (secs >= 60)
        {
            uint32 m = secs / 60;
            return (m == 1) ? "1 minute" : std::to_string(m) + " minutes";
        }
        return std::to_string(secs) + "s";
    }

    std::string RelativeAge(uint64 tickAt, uint64 now)
    {
        if (tickAt > now)
            return "just now";
        uint64 delta = now - tickAt;
        if (delta < 60)
            return std::to_string(delta) + "s ago";
        if (delta < 3600)
            return std::to_string(delta / 60) + "m ago";
        if (delta < 86400)
            return std::to_string(delta / 3600) + "h ago";
        return std::to_string(delta / 86400) + "d ago";
    }

    std::string FormatEventRemaining(uint64 now, uint64 startsAt, uint64 endsAt)
    {
        if (now < startsAt)
        {
            uint64 delta = startsAt - now;
            if (delta < 60)
                return "appears in " + std::to_string(delta) + "s";
            return "appears in " + std::to_string(delta / 60) + "m";
        }
        if (now >= endsAt)
            return "leaving";
        uint64 delta = endsAt - now;
        if (delta < 60)
            return std::to_string(delta) + "s left";
        return std::to_string(delta / 60) + "m left";
    }

    // Re-open the Terror Zones submenu so the player can keep reading
    // after an info action prints to chat.
    void SendTzMenu(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "What zones are terrorized?", SENDER_TZ, ACTION_TZ_INFO);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "When is the next rotation?", SENDER_TZ, ACTION_TZ_NEXT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "Recent rotations", SENDER_TZ, ACTION_TZ_HISTORY);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "How do Terror Zones work?", SENDER_TZ, ACTION_TZ_HOW);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "Nevermind", SENDER_TZ, ACTION_TZ_CLOSE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void PrintInfo(Player* player)
    {
        auto& mgr = TerrorZonesMgr::Instance();
        ChatHandler ch(player->GetSession());
        if (!mgr.IsEnabled())
        {
            ch.SendSysMessage("Terror Zones is disabled.");
            return;
        }
        ActiveRotation rot = mgr.GetActiveRotation();
        if (rot.slots.empty())
        {
            ch.SendSysMessage("No zones are terrorized right now. "
                              "Check back next rotation.");
            return;
        }
        uint32 remaining = mgr.RemainingSeconds();
        ch.PSendSysMessage(
            "|cffff8040Terrorized zones|r ({} active, {} remaining):",
            static_cast<uint32>(rot.slots.size()), FormatRemaining(remaining));

        std::vector<ActiveEvent> events;
        bool haveEvents = mgr.IsEventsEnabled();
        if (haveEvents)
            events = mgr.GetEventsSnapshot();
        uint64 now = static_cast<uint64>(::time(nullptr));

        for (ActiveSlot const& s : rot.slots)
        {
            ch.PSendSysMessage("  |cffffd100{}|r — {} {}",
                               s.displayName,
                               TierDisplayName(s.tier),
                               FlavorDisplayName(s.flavor));
            if (!haveEvents)
                continue;
            for (ActiveEvent const& e : events)
            {
                if (e.zoneId != s.zoneId)
                    continue;
                if (e.state == EVENT_STATE_EXPIRED)
                    continue;
                ch.PSendSysMessage("      * {}: {} — {}",
                    EventTypeDisplayName(e.type),
                    e.displayName.empty() ? "(unnamed)" : e.displayName,
                    FormatEventRemaining(now, e.startsAt, e.endsAt));
            }
        }
    }

    void PrintNext(Player* player)
    {
        auto& mgr = TerrorZonesMgr::Instance();
        ChatHandler ch(player->GetSession());
        if (!mgr.IsEnabled())
        {
            ch.SendSysMessage("Terror Zones is disabled.");
            return;
        }
        uint64 next = mgr.GetNextTickAt();
        uint64 now = static_cast<uint64>(::time(nullptr));
        if (next <= now)
        {
            ch.SendSysMessage("The next rotation is imminent.");
            return;
        }
        uint32 delta = static_cast<uint32>(next - now);
        time_t nextT = static_cast<time_t>(next);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &nextT);
#else
        localtime_r(&nextT, &local);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M", &local);
        ch.PSendSysMessage("Next rotation at {} server time ({}).",
                           buf, FormatRemaining(delta));
    }

    void PrintHistory(Player* player)
    {
        auto& mgr = TerrorZonesMgr::Instance();
        ChatHandler ch(player->GetSession());
        if (!mgr.IsEnabled())
        {
            ch.SendSysMessage("Terror Zones is disabled.");
            return;
        }
        std::vector<HistoryTick> hist = mgr.GetHistory(5);
        if (hist.empty())
        {
            ch.SendSysMessage("No rotation history yet.");
            return;
        }
        uint64 now = static_cast<uint64>(::time(nullptr));
        ch.PSendSysMessage("Last {} rotation(s):",
                           static_cast<uint32>(hist.size()));
        for (HistoryTick const& h : hist)
        {
            std::string names;
            for (size_t i = 0; i < h.slots.size(); ++i)
            {
                if (i) names += ", ";
                names += h.slots[i].displayName;
                names += " (";
                names += TierDisplayName(h.slots[i].tier);
                names += ' ';
                names += FlavorDisplayName(h.slots[i].flavor);
                names += ")";
            }
            ch.PSendSysMessage("  {} — {}", names, RelativeAge(h.tickAt, now));
        }
    }

    void PrintHowItWorks(Player* player)
    {
        ChatHandler ch(player->GetSession());
        ch.SendSysMessage("|cffff8040How Terror Zones work:|r");
        ch.SendSysMessage("  - On a timer, one zone per continent becomes "
                          "\"terrorized\" and its monsters scale up to your "
                          "level.");
        ch.SendSysMessage("  - Terrorized zones grant bonus XP, gold, and "
                          "better loot. Each rotation has a Tier (1-5, "
                          "higher = stronger + richer) and a Flavor that "
                          "biases the rewards.");
        ch.SendSysMessage("  - A world boss prowls each terrorized zone for "
                          "the whole rotation — track it on your minimap and "
                          "kill it for special drops.");
        ch.SendSysMessage("  - Ask any innkeeper, or type |cff00ff00.zones|r, "
                          "to see what's currently terrorized.");
    }

    class TerrorZones_GossipCreature : public AllCreatureScript
    {
    public:
        TerrorZones_GossipCreature()
            : AllCreatureScript("TerrorZones_GossipCreature") {}

        bool OnCreatureGossipHelloAppend(Player* player,
                                         Creature* creature) override
        {
            if (!TerrorZonesMgr::Instance().IsInnkeeperGossipEnabled())
                return false;
            if (!creature->HasNpcFlag(UNIT_NPC_FLAG_INNKEEPER))
                return false;

            // Additive hook — core already prepared + will send the native
            // innkeeper menu. We only append our entry.
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                "Terror Zones - what's empowered?",
                SENDER_TZ, ACTION_TZ_OPEN);
            return true;
        }

        bool CanCreatureGossipSelect(Player* player, Creature* creature,
                                     uint32 sender, uint32 action) override
        {
            if (sender != SENDER_TZ)
                return false;   // not ours — let native handling proceed

            switch (action)
            {
                case ACTION_TZ_OPEN:
                case ACTION_TZ_BACK:
                    SendTzMenu(player, creature);
                    break;
                case ACTION_TZ_INFO:
                    PrintInfo(player);
                    SendTzMenu(player, creature);
                    break;
                case ACTION_TZ_NEXT:
                    PrintNext(player);
                    SendTzMenu(player, creature);
                    break;
                case ACTION_TZ_HISTORY:
                    PrintHistory(player);
                    SendTzMenu(player, creature);
                    break;
                case ACTION_TZ_HOW:
                    PrintHowItWorks(player);
                    SendTzMenu(player, creature);
                    break;
                case ACTION_TZ_CLOSE:
                default:
                    CloseGossipMenuFor(player);
                    break;
            }
            return true;   // we consumed the selection
        }
    };
}

void AddTerrorZonesGossipScripts()
{
    new TerrorZones_GossipCreature();
}
