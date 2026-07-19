#include "DynamicAHActivity.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "Log.h"
#include <algorithm>
#include <unordered_set>

namespace ModDynamicAH
{
    static inline uint64 SkillTeamKey(uint32 skillId, TeamId team)
    {
        return (uint64(skillId) << 8) | uint64(uint8(team));
    }

    DynamicAHActivity &DynamicAHActivity::Instance()
    {
        static DynamicAHActivity instance;
        return instance;
    }

    void DynamicAHActivity::Refresh(uint32 windowDays, std::string const &botAccountPrefix)
    {
        _bySkillTeam.clear();
        _realCharCount = 0;

        // Bot accounts follow mod-playerbots' own naming convention (default prefix "rndbot",
        // covers both RNDbot and AddClass account types). Looked up on the auth DB, then
        // excluded in-process below rather than joined cross-schema.
        std::unordered_set<uint32> botAccounts;
        if (!botAccountPrefix.empty())
        {
            if (QueryResult r = LoginDatabase.Query("SELECT id FROM account WHERE username LIKE '{}%%'", botAccountPrefix))
            {
                do
                    botAccounts.insert((*r)[0].Get<uint32>());
                while (r->NextRow());
            }
        }

        uint32 windowSec = std::max<uint32>(windowDays, 1u) * DAY;

        // Every profession skill the context planner draws brackets from.
        static constexpr uint32 kSkills[] = {
            SKILL_TAILORING, SKILL_FIRST_AID, SKILL_ALCHEMY, SKILL_HERBALISM, SKILL_INSCRIPTION,
            SKILL_MINING, SKILL_BLACKSMITHING, SKILL_LEATHERWORKING, SKILL_SKINNING,
            SKILL_ENCHANTING, SKILL_JEWELCRAFTING, SKILL_COOKING, SKILL_FISHING, SKILL_ENGINEERING,
        };
        std::string skillCsv;
        for (uint32 s : kSkills)
        {
            if (!skillCsv.empty())
                skillCsv += ",";
            skillCsv += std::to_string(s);
        }

        QueryResult res = CharacterDatabase.Query(
            "SELECT c.guid, c.account, c.race, cs.skill, cs.value "
            "FROM characters c JOIN character_skills cs ON cs.guid = c.guid "
            "WHERE c.deleteDate IS NULL AND (c.online = 1 OR c.logout_time >= UNIX_TIMESTAMP(NOW()) - {}) "
            "AND cs.skill IN ({})",
            windowSec, skillCsv);

        if (!res)
            return;

        std::unordered_set<uint32> seenChars;
        do
        {
            Field *f = res->Fetch();
            uint32 account = f[1].Get<uint32>();
            if (botAccounts.count(account))
                continue;

            uint8 race = f[2].Get<uint8>();
            uint32 skill = f[3].Get<uint16>();
            uint16 value = f[4].Get<uint16>();
            if (!value)
                continue;

            TeamId team = Player::TeamIdForRace(race);
            if (team != TEAM_ALLIANCE && team != TEAM_HORDE)
                continue;

            seenChars.insert(f[0].Get<uint32>());
            _bySkillTeam[SkillTeamKey(skill, team)].push_back(value);
        } while (res->NextRow());

        _realCharCount = uint32(seenChars.size());

        LOG_DEBUG("mod.dynamicah", "activity: {} real (non-bot) characters active in last {}d, {} bot accounts excluded",
                  _realCharCount, windowDays, botAccounts.size());
    }

    std::vector<uint16> const &DynamicAHActivity::SkillValues(uint32 skillId, TeamId team) const
    {
        static std::vector<uint16> const empty;
        auto it = _bySkillTeam.find(SkillTeamKey(skillId, team));
        return it != _bySkillTeam.end() ? it->second : empty;
    }
} // namespace ModDynamicAH
