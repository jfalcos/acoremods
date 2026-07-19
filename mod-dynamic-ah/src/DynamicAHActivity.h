#pragma once

#include "Common.h"
#include "SharedDefines.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ModDynamicAH
{
    // Snapshot, rebuilt once per planning cycle, of profession skill values held by real
    // (non-bot) characters who have played within a configurable recent window rather than
    // whoever happens to be online right now. mod-playerbots keeps large numbers of bot
    // characters permanently online, so sourcing skill brackets from currently-online players
    // means the AH's item mix tracks the bot population's professions/levels instead of the
    // real playerbase's.
    class DynamicAHActivity
    {
    public:
        static DynamicAHActivity &Instance();

        // windowDays: how many days back counts as "recently active" (plus anyone online right
        // now). botAccountPrefix: account username prefix used to identify and exclude
        // playerbot accounts. Read independently from config (not linked against
        // mod-playerbots) so this works whether or not that module is present.
        void Refresh(uint32 windowDays, std::string const &botAccountPrefix);

        // Skill values (already excludes untrained/0) held by matching real characters, split
        // by faction. Empty if none found.
        std::vector<uint16> const &SkillValues(uint32 skillId, TeamId team) const;

        uint32 RealCharacterCount() const { return _realCharCount; }

    private:
        std::unordered_map<uint64, std::vector<uint16>> _bySkillTeam; // (skillId<<8)|team -> values
        uint32 _realCharCount = 0;
    };
} // namespace ModDynamicAH
