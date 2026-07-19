#ifndef MOD_MOUNT_PROGRESSION_MGR_H
#define MOD_MOUNT_PROGRESSION_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;
class Creature;
class SpellInfo;

namespace mod_mount_progression
{

enum class MountRarity : uint8
{
    Common = 0,
    Uncommon,
    Rare,
    Epic,
    Legendary,
    MAX
};

enum class MountType : uint8
{
    Stamina = 0,
    Predator,
    Agility,
    Mechanical,
    Arcane,
    MAX
};

struct CatalogEntry
{
    uint32 spellId;
    uint32 displayId;
    std::string displayName;
    MountRarity rarity;
    MountType type;
};

struct MountProgress
{
    uint32 spellId;
    uint16 level;
    uint32 xp;
    bool dirty;
};

char const* RarityName(MountRarity r);
char const* TypeName(MountType t);
char const* XPSourceName(MountType t);

// Human-readable label for a mount type's carrier buff, used in chat
// output (e.g. "Attack Power", "Armor").
char const* BuffEffectLabel(MountType t);

class MountProgressionMgr
{
public:
    static MountProgressionMgr& Instance();

    void LoadConfig();
    void LoadCatalog();

    bool IsEnabled() const { return _enabled; }
    bool IsDebug() const { return _debug; }
    uint16 GetMaxLevel() const { return _maxLevel; }

    CatalogEntry const* GetCatalogEntry(uint32 spellId) const;

    // xp_to_next(level) per rarity. Returns 0 at max level.
    uint32 XPToNextLevel(MountRarity rarity, uint16 level) const;

    // Per-character state. Safe to call on any thread holding the player object.
    void LoadPlayerState(Player* player);
    void SavePlayerState(Player* player);
    void UnloadPlayerState(ObjectGuid guid);

    MountProgress* GetOrCreateProgress(Player* player, uint32 spellId);
    MountProgress const* GetProgress(Player* player, uint32 spellId) const;
    std::vector<MountProgress> GetAllProgress(Player* player) const;

    // Active mount (in-memory for Slice 2; buff persistence is Slice 3).
    void SetActiveMount(Player* player, uint32 spellId);
    uint32 GetActiveMount(Player* player) const;
    void ClearActiveMount(ObjectGuid guid);

    // Events.
    void OnCreatureKill(Player* player, Creature* killed);
    void OnSpellCast(Player* player, SpellInfo const* info);
    void OnPlayerTick(Player* player, uint32 diff);
    void OnPlayerLoot(Player* player, ObjectGuid lootguid);
    void OnPlayerAreaChange(Player* player, uint32 oldArea, uint32 newArea);

    // Set `spellId` as the active mount for `player`, creating a progress row
    // if needed. Announces the change in chat when active mount actually changes.
    // Returns the catalog entry, or nullptr if `spellId` is not in the catalog.
    CatalogEntry const* ActivateMount(Player* player, uint32 spellId);

    // Admin/test helpers. Return false if the player has no active mount
    // or the active mount is not in the catalog.
    bool AwardActiveMountXP(Player* player, uint32 amount);
    bool SetActiveMountLevel(Player* player, uint16 level);

    // Carrier-buff API (Slice 3). ApplyMountBuff removes any existing
    // carrier aura first, then applies the one for `entry->type` with
    // magnitude computed from ceiling × curve(level).
    void ApplyMountBuff(Player* player, CatalogEntry const* entry,
                        uint16 level);
    void RemoveMountBuff(Player* player);
    uint32 ComputeBuffMagnitude(CatalogEntry const* entry,
                                uint16 level) const;

    // Cross-logout persistence of the active mount for the offline-grace
    // reapply window (spec §5).
    void SaveActiveMountToDB(Player* player);
    void LoadActiveMountFromDB(Player* player);

    // Map a real carrier spell ID (80000..80004 by default) to the
    // client-known icon-donor spell ID configured for that mount type.
    // Returns 0 when the input is not one of our carriers or no donor
    // is configured (caller should keep the real ID).
    uint32 GetIconDonor(uint32 realCarrierSpellId) const;

    // One-time starter-mount choice gate (Slice 4). Returns true if this
    // character has already made their starter choice.
    bool HasMadeStarterChoice(Player* player) const;

    // Records the one-time starter choice, learns+activates `spellId` via
    // the normal ActivateMount path, and writes
    // character_mount_starter_choice. Returns the catalog entry on
    // success, nullptr if spellId isn't in the catalog, player is null, or
    // the player already has a recorded choice.
    CatalogEntry const* GrantStarterMount(Player* player, uint32 spellId);

    // Returns the configured starter-mount spell ID for a given type.
    // Only Stamina/Predator/Arcane are meaningful; other types return 0.
    uint32 GetStarterSpell(MountType t) const;

    // GM/test helper: clears the one-time starter-choice gate so the
    // player can talk to the Mount Tamer and pick again. Does NOT
    // unlearn any previously-granted starter spell or touch its XP/level
    // progress. Returns true if a choice was actually on record (false
    // if there was nothing to clear).
    bool ResetStarterChoice(Player* player);

    // Slice 5 — starter quest/mail nudge. Call on every login; no-ops if
    // the character already made their choice or was already sent the
    // mail. Sends a flavor letter and grants the starter quest directly
    // into the player's log (no live questgiver needed).
    void MaybeSendStarterQuest(Player* player);

    // Completes + rewards the starter quest, called the moment the
    // player actually picks a mount at the Mount Tamer. No-op if the
    // quest isn't in their log (feature disabled, already turned in, or
    // they never got the mail).
    void CompleteStarterQuest(Player* player);

private:
    MountProgressionMgr() = default;

    void AwardXP(Player* player, MountProgress* progress, uint32 amount,
                 CatalogEntry const* entry);
    uint32 KillXpForRank(Creature const* killed) const;
    // Awards XP to the active mount iff its type matches `required`.
    // Returns true if anything was awarded.
    bool AwardActiveIfType(Player* player, MountType required, uint32 amount);
    void AnnounceActiveMount(Player* player, CatalogEntry const* entry);
    // Throttled "mount is growing" trickle message (Slice 4). Accumulates
    // `amount` and flushes a summed message at most once per
    // _announceXPGainIntervalSeconds.
    void AnnounceXPGain(Player* player, CatalogEntry const* entry, uint32 amount);
    double CurveFraction(uint16 level) const;

    bool _enabled = true;
    bool _debug = false;
    uint16 _maxLevel = 60;
    uint32 _xpBase[static_cast<size_t>(MountRarity::MAX)] = {10, 40, 120, 400, 4000};
    uint32 _killXpNormal = 10;
    uint32 _killXpElite = 40;
    uint32 _killXpBoss = 200;
    int32 _killMinLevelDelta = -10;
    uint32 _yardsPerXP = 10;
    uint32 _maxYardsPerTick = 90;
    uint32 _castXPPerCast = 1;
    uint32 _gatherXPPerLoot = 5;
    uint32 _craftXPPerCraft = 10;
    uint32 _areaXPPerTransition = 25;

    // Slice 3 — buff tunables
    uint8 _buffMagnitudeMode = 1;  // 0=spec-literal %, 1=flat-per-level
    uint8 _buffLevelCurve = 1;     // 0=linear, 1=stepped, 2=quadratic
    uint32 _buffCeiling[static_cast<size_t>(MountRarity::MAX)] =
        {20, 60, 120, 200, 400};
    uint32 _offlineGraceSeconds = 1800;
    bool _announceOnCast = true;
    uint32 _carrierSpell[static_cast<size_t>(MountType::MAX)] =
        {80000, 80001, 80002, 80003, 80004};
    uint32 _iconDonor[static_cast<size_t>(MountType::MAX)] =
        {8099, 30848, 8115, 77, 22418};  // thematic client-visible donors; see conf

    // Slice 4 — throttled XP-trickle announce
    bool _announceXPGain = true;
    uint32 _announceXPGainIntervalSeconds = 20;

    // Slice 5 — starter quest/mail nudge
    bool _starterQuestEnabled = true;
    uint32 _starterQuestId = 900000;

    // Slice 4 — starter mount choice: Stamina (universal), Predator
    // (physical dps), Arcane (caster/healing dps — the only type whose
    // buff benefits casters/healers at all). Agility was dropped in
    // favor of Arcane since its audience (rogue/hunter/feral) already
    // overlapped with Predator's. The Arcane slot uses Skeletal Horse
    // (8980), a common-rarity ordinary ground mount hand-reclassified
    // to type=arcane (see rev_1782866697421426100.sql) rather than the
    // catalog's native arcane picks (all flying mounts) — a Hallow's
    // End "Rickety Magic Broom" was tried first but has a reduced speed
    // bonus vs. the other two starters, breaking speed parity.
    uint32 _starterSpell[3] = {458, 459, 8980};

    std::unordered_map<uint32, CatalogEntry> _catalog;

    struct PlayerTickState
    {
        uint32 accumMs = 0;
        float lastX = 0.f;
        float lastY = 0.f;
        float lastZ = 0.f;
        bool hasLastPos = false;
        float accumYards = 0.f;
        std::unordered_set<uint32> visitedAreas;
    };

    // Slice 4 — per-player throttle state for AnnounceXPGain.
    struct XpAnnounceState
    {
        uint32 accumXp = 0;        // XP accumulated since last flush
        uint64 lastFlushTime = 0;  // unix seconds; 0 = never flushed
    };

    mutable std::mutex _stateMutex;
    std::unordered_map<uint32 /*guidLow*/,
                       std::unordered_map<uint32 /*spellId*/, MountProgress>> _progress;
    std::unordered_map<uint32 /*guidLow*/, uint32 /*active spellId*/> _active;
    std::unordered_map<uint32 /*guidLow*/, PlayerTickState> _tickState;
    std::unordered_map<uint32 /*guidLow*/, XpAnnounceState> _xpAnnounce;
};

}  // namespace mod_mount_progression

#endif
