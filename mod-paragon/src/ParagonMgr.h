#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "ParagonItemUpgrades.h"
#include "ParagonPerks.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

class Item;
class Player;

namespace mod_paragon
{
    // World-thread only (all hooks run there) — lock-free by convention.
    // Account state is cached at login and written through asynchronously,
    // so the XP hook never touches the database.
    class ParagonMgr
    {
    public:
        static ParagonMgr& Instance();

        void LoadConfig();

        bool IsEnabled() const { return _enabled; }
        bool Debug() const { return _debug; }
        void SetDebug(bool d) { _debug = d; }
        uint32 PxPerLevel() const { return _pxPerLevel; }
        uint32 CoinItemId() const { return _coinItemId; }
        uint32 QuartermasterEntry() const { return _qmNpcEntry; }
        bool IsMapBlocked(uint32 mapId) const;
        uint32 MinToggleLevel() const { return _minToggleLevel; }
        uint32 MaxAllocPercentFor(uint8 level) const;
        bool XPGainScale() const { return _xpGainScale; }
        perks::PerkConfig const& PerkCfg() const { return _perkCfg; }

        // Session lifecycle (account state + per-character perk ranks).
        void LoadPlayer(Player* player);
        void UnloadPlayer(Player* player);

        // Cached account state (valid while a character of the account is
        // online; commands and the XP hook only run then).
        uint64 GetLifetimePX(uint32 accountId) const;
        uint32 GetAllocPercent(uint32 accountId) const;
        bool IsOptedOut(uint32 accountId) const;
        void SetAllocPercent(uint32 accountId, uint32 percent);
        void SetOptOut(uint32 accountId, bool state);

        // PX accounting. Handles paragon-level crossings (coins, milestones,
        // broadcasts) via RewardDispatcher.
        void AddPX(Player* player, uint64 px);
        void SetLifetimePX(Player* player, uint64 px, uint32 rewardLevel); // GM

        uint32 ComputePL(uint64 lifetimePx) const;
        uint64 ComputeProgressInLevel(uint64 lifetimePx) const;

        // Perks (per character; ranks cached at login).
        uint32 GetPerkRanks(ObjectGuid::LowType guid, perks::Property prop) const;
        // Buys the next rank of `prop` for coins. Returns false with a chat
        // message already sent on any failure (max rank, coins, disabled).
        bool TryPurchasePerk(Player* player, perks::Property prop);
        void ResetPerks(Player* player); // GM: clears ranks + override rows

        // Item upgrades: buys one chunk of `prop` onto the item instance,
        // capped by the item's ilvl/quality budget. Chat message sent on any
        // failure. The override row is the entire upgrade state.
        bool IsItemUpgradeEnabled() const { return _itemUpgradeEnabled; }
        upgrades::UpgradeConfig const& UpgradeCfg() const { return _upgradeCfg; }
        bool TryPurchaseItemUpgrade(Player* player, Item* item, upgrades::Property prop);

        void OnLogin(Player* player); // splash + pending reward delivery

    private:
        ParagonMgr() = default;

        struct AccountState
        {
            uint64 lifetimePx = 0;
            uint32 lastRewardLevel = 0;
            uint32 allocPercent = 0;
            bool optOut = false;
            bool rowExists = false;
            uint32 chars = 0; // online characters of this account
        };

        AccountState* FindAccount(uint32 accountId);
        AccountState const* FindAccount(uint32 accountId) const;
        void PersistAccount(uint32 accountId, AccountState const& st) const;
        void HandleLevelCrossings(Player* player, uint32 prevLevel, uint32 newLevel);
        void ApplyPerkOverride(Player* player, perks::Property prop, uint32 ranks);

        std::unordered_map<uint32, AccountState> _accounts; // by account id
        std::unordered_map<ObjectGuid::LowType,
                           std::unordered_map<uint8, uint32>> _perkRanks; // guid -> property -> ranks

        // config
        bool _enabled = true;
        bool _loginSplash = true;
        bool _debug = false;
        uint32 _pxPerLevel = 1670800;
        uint32 _coinItemId = 37711;
        uint32 _qmNpcEntry = 96000;
        uint32 _minToggleLevel = 30;
        uint32 _perkMinLevel = 30;
        uint32 _maxAllocLow = 30;
        uint32 _maxAllocHigh = 100;
        uint32 _allocCapBreakLevel = 60;
        bool _xpGainScale = false;
        std::unordered_set<uint32> _blockedMaps;
        std::unordered_set<uint32> _milestoneLevels;
        perks::PerkConfig _perkCfg;
        bool _itemUpgradeEnabled = true;
        upgrades::UpgradeConfig _upgradeCfg;
    };
}
