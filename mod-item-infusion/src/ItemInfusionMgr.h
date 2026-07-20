#pragma once

#include "Define.h"
#include "ItemInfusions.h"
#include <utility>
#include <vector>

class Item;
class Player;
struct ItemTemplate;

namespace mod_item_infusion
{
    // World-thread only (all hooks run there) — lock-free by convention,
    // same as the sibling modules.
    class ItemInfusionMgr
    {
    public:
        static ItemInfusionMgr& Instance();

        void LoadConfig();

        bool IsEnabled() const { return _enabled; }
        bool Debug() const { return _debug; }
        uint32 MinLevel() const { return _minLevel; }
        uint32 AlchemistEntry() const { return _alchemistEntry; }
        InfusionConfig const& Cfg() const { return _cfg; }
        // (itemId, riskReduction) pairs from config, unvalidated — callers
        // skip ids without a live ItemTemplate.
        std::vector<std::pair<uint32, float>> const& Substances() const
        { return _substances; }
        // Mitigation currency, resolved from mod-paragon (single source of
        // truth for what a Paragon Coin is).
        uint32 CoinItemId() const;

        // Sacrifice `donor` to transfer a fraction of its native stats onto
        // `target` as source='mix' rows. The donor, pledged coins, and
        // substances are always consumed; on a failed roll the TARGET is
        // destroyed too. Chat messages are sent for every outcome
        // (including gate refusals).
        enum class InfuseResult : uint8
        {
            Rejected,  // gate/validation failure, nothing consumed
            Survived,  // stats transferred
            Destroyed, // roll failed, target gone
        };
        InfuseResult TryInfuse(Player* player, Item* target, Item* donor,
                               uint32 coins, std::vector<uint32> const& substanceIds);

        // Donor template stats in the pure-math vocabulary (array stats +
        // armor/resistances as the platform's 100-106 ids).
        static std::vector<DonorStat> CollectDonorStats(ItemTemplate const* proto);

        // GM: force the roll's risk (0-100), -1 = live math. For the
        // in-game verification runbook.
        void SetRiskOverride(int32 percent) { _riskOverridePct = percent; }
        int32 RiskOverride() const { return _riskOverridePct; }

    private:
        ItemInfusionMgr() = default;

        bool _enabled = true;
        bool _debug = false;
        uint32 _minLevel = 30;
        uint32 _alchemistEntry = 96010;
        InfusionConfig _cfg;
        std::vector<std::pair<uint32, float>> _substances;
        int32 _riskOverridePct = -1;
    };
}
