#include "MountProgressionMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace mod_mount_progression
{

namespace
{
    MountRarity ParseRarity(std::string const& s)
    {
        if (s == "uncommon")  return MountRarity::Uncommon;
        if (s == "rare")      return MountRarity::Rare;
        if (s == "epic")      return MountRarity::Epic;
        if (s == "legendary") return MountRarity::Legendary;
        return MountRarity::Common;
    }

    MountType ParseType(std::string const& s)
    {
        if (s == "predator")   return MountType::Predator;
        if (s == "agility")    return MountType::Agility;
        if (s == "mechanical") return MountType::Mechanical;
        if (s == "arcane")     return MountType::Arcane;
        return MountType::Stamina;
    }
}

char const* RarityName(MountRarity r)
{
    switch (r)
    {
        case MountRarity::Common:    return "common";
        case MountRarity::Uncommon:  return "uncommon";
        case MountRarity::Rare:      return "rare";
        case MountRarity::Epic:      return "epic";
        case MountRarity::Legendary: return "legendary";
        default:                     return "unknown";
    }
}

char const* TypeName(MountType t)
{
    switch (t)
    {
        case MountType::Stamina:    return "stamina";
        case MountType::Predator:   return "predator";
        case MountType::Agility:    return "agility";
        case MountType::Mechanical: return "mechanical";
        case MountType::Arcane:     return "arcane";
        default:                    return "unknown";
    }
}

char const* XPSourceName(MountType t)
{
    switch (t)
    {
        case MountType::Stamina:    return "ride distance";
        case MountType::Predator:   return "kill creatures";
        case MountType::Agility:    return "explore new subzones";
        case MountType::Mechanical: return "gather and craft";
        case MountType::Arcane:     return "cast spells";
        default:                    return "unknown";
    }
}

// Matches the carrier spells defined in the Slice 3 SQL migration
// (mount_progression_carrier_spells, IDs 80000-80004). Purely cosmetic
// -- used only to label the magnitude number in chat, never to decide
// game logic.
char const* BuffEffectLabel(MountType t)
{
    switch (t)
    {
        case MountType::Stamina:    return "Stamina";
        case MountType::Predator:   return "Attack Power";
        case MountType::Agility:    return "Agility";
        case MountType::Mechanical: return "Armor";
        case MountType::Arcane:     return "Spell Power & Healing";
        default:                    return "Unknown";
    }
}

MountProgressionMgr& MountProgressionMgr::Instance()
{
    static MountProgressionMgr inst;
    return inst;
}
void MountProgressionMgr::LoadConfig()
{
    _enabled   = sConfigMgr->GetOption<bool>("MountProgression.Enable", true);
    _debug     = sConfigMgr->GetOption<bool>("MountProgression.Debug", false);
    _maxLevel  = static_cast<uint16>(
        sConfigMgr->GetOption<uint32>("MountProgression.MaxLevel", 60));

    _xpBase[static_cast<size_t>(MountRarity::Common)] =
        sConfigMgr->GetOption<uint32>("MountProgression.XPBase.Common", 10);
    _xpBase[static_cast<size_t>(MountRarity::Uncommon)] =
        sConfigMgr->GetOption<uint32>("MountProgression.XPBase.Uncommon", 40);
    _xpBase[static_cast<size_t>(MountRarity::Rare)] =
        sConfigMgr->GetOption<uint32>("MountProgression.XPBase.Rare", 120);
    _xpBase[static_cast<size_t>(MountRarity::Epic)] =
        sConfigMgr->GetOption<uint32>("MountProgression.XPBase.Epic", 400);
    _xpBase[static_cast<size_t>(MountRarity::Legendary)] =
        sConfigMgr->GetOption<uint32>("MountProgression.XPBase.Legendary", 4000);

    _killXpNormal = sConfigMgr->GetOption<uint32>("MountProgression.KillXP.Normal", 10);
    _killXpElite  = sConfigMgr->GetOption<uint32>("MountProgression.KillXP.Elite", 40);
    _killXpBoss   = sConfigMgr->GetOption<uint32>("MountProgression.KillXP.Boss", 200);
    _killMinLevelDelta =
        sConfigMgr->GetOption<int32>("MountProgression.KillXP.MinLevelDelta", -10);

    _yardsPerXP = sConfigMgr->GetOption<uint32>(
        "MountProgression.DistanceXP.YardsPerXP", 10);
    _maxYardsPerTick = sConfigMgr->GetOption<uint32>(
        "MountProgression.DistanceXP.MaxYardsPerTick", 90);
    _castXPPerCast = sConfigMgr->GetOption<uint32>(
        "MountProgression.CastXP.PerCast", 1);
    _gatherXPPerLoot = sConfigMgr->GetOption<uint32>(
        "MountProgression.GatherXP.PerLoot", 5);
    _craftXPPerCraft = sConfigMgr->GetOption<uint32>(
        "MountProgression.CraftXP.PerCraft", 10);
    _areaXPPerTransition = sConfigMgr->GetOption<uint32>(
        "MountProgression.AreaXP.PerTransition", 25);

    _buffMagnitudeMode = static_cast<uint8>(sConfigMgr->GetOption<uint32>(
        "MountProgression.BuffMagnitudeMode", 1));
    _buffLevelCurve = static_cast<uint8>(sConfigMgr->GetOption<uint32>(
        "MountProgression.BuffLevelCurve", 1));
    _buffCeiling[static_cast<size_t>(MountRarity::Common)] =
        sConfigMgr->GetOption<uint32>("MountProgression.BuffCeiling.Common", 20);
    _buffCeiling[static_cast<size_t>(MountRarity::Uncommon)] =
        sConfigMgr->GetOption<uint32>("MountProgression.BuffCeiling.Uncommon", 60);
    _buffCeiling[static_cast<size_t>(MountRarity::Rare)] =
        sConfigMgr->GetOption<uint32>("MountProgression.BuffCeiling.Rare", 120);
    _buffCeiling[static_cast<size_t>(MountRarity::Epic)] =
        sConfigMgr->GetOption<uint32>("MountProgression.BuffCeiling.Epic", 200);
    _buffCeiling[static_cast<size_t>(MountRarity::Legendary)] =
        sConfigMgr->GetOption<uint32>("MountProgression.BuffCeiling.Legendary", 400);
    _offlineGraceSeconds = sConfigMgr->GetOption<uint32>(
        "MountProgression.OfflineGraceSeconds", 1800);
    _announceOnCast = sConfigMgr->GetOption<bool>(
        "MountProgression.AnnounceActiveOnCast", true);
    _carrierSpell[static_cast<size_t>(MountType::Stamina)] =
        sConfigMgr->GetOption<uint32>("MountProgression.CarrierSpell.Stamina", 80000);
    _carrierSpell[static_cast<size_t>(MountType::Predator)] =
        sConfigMgr->GetOption<uint32>("MountProgression.CarrierSpell.Predator", 80001);
    _carrierSpell[static_cast<size_t>(MountType::Agility)] =
        sConfigMgr->GetOption<uint32>("MountProgression.CarrierSpell.Agility", 80002);
    _carrierSpell[static_cast<size_t>(MountType::Mechanical)] =
        sConfigMgr->GetOption<uint32>("MountProgression.CarrierSpell.Mechanical", 80003);
    _carrierSpell[static_cast<size_t>(MountType::Arcane)] =
        sConfigMgr->GetOption<uint32>("MountProgression.CarrierSpell.Arcane", 80004);

    _iconDonor[static_cast<size_t>(MountType::Stamina)] =
        sConfigMgr->GetOption<uint32>("MountProgression.IconDonor.Stamina", 8099);
    _iconDonor[static_cast<size_t>(MountType::Predator)] =
        sConfigMgr->GetOption<uint32>("MountProgression.IconDonor.Predator", 30848);
    _iconDonor[static_cast<size_t>(MountType::Agility)] =
        sConfigMgr->GetOption<uint32>("MountProgression.IconDonor.Agility", 8115);
    _iconDonor[static_cast<size_t>(MountType::Mechanical)] =
        sConfigMgr->GetOption<uint32>("MountProgression.IconDonor.Mechanical", 77);
    _iconDonor[static_cast<size_t>(MountType::Arcane)] =
        sConfigMgr->GetOption<uint32>("MountProgression.IconDonor.Arcane", 22418);

    _announceXPGain = sConfigMgr->GetOption<bool>(
        "MountProgression.AnnounceXPGain", true);
    _announceXPGainIntervalSeconds = sConfigMgr->GetOption<uint32>(
        "MountProgression.AnnounceXPGainIntervalSeconds", 20);

    _starterQuestEnabled = sConfigMgr->GetOption<bool>(
        "MountProgression.StarterQuest.Enable", true);
    _starterQuestId = sConfigMgr->GetOption<uint32>(
        "MountProgression.StarterQuest.QuestId", 900000);

    _starterSpell[0] = sConfigMgr->GetOption<uint32>(
        "MountProgression.StarterSpell.Stamina", 458);
    _starterSpell[1] = sConfigMgr->GetOption<uint32>(
        "MountProgression.StarterSpell.Predator", 459);
    _starterSpell[2] = sConfigMgr->GetOption<uint32>(
        "MountProgression.StarterSpell.Arcane", 8980);

    LOG_INFO("module",
             "mod-mount-progression: enabled={}, maxLevel={}, xpBase=[{},{},{},{},{}], "
             "killXp=[{}/{}/{}], dist(y/xp={} cap={}), cast={}, gather={}, craft={}, area={}, "
             "debug={}",
             _enabled, _maxLevel,
             _xpBase[0], _xpBase[1], _xpBase[2], _xpBase[3], _xpBase[4],
             _killXpNormal, _killXpElite, _killXpBoss,
             _yardsPerXP, _maxYardsPerTick,
             _castXPPerCast, _gatherXPPerLoot, _craftXPPerCraft, _areaXPPerTransition,
             _debug);
    LOG_INFO("module",
             "mod-mount-progression: buff mode={} curve={} ceil=[{},{},{},{},{}] "
             "grace={}s announce={} carriers=[{},{},{},{},{}] donors=[{},{},{},{},{}]",
             _buffMagnitudeMode, _buffLevelCurve,
             _buffCeiling[0], _buffCeiling[1], _buffCeiling[2],
             _buffCeiling[3], _buffCeiling[4],
             _offlineGraceSeconds, _announceOnCast,
             _carrierSpell[0], _carrierSpell[1], _carrierSpell[2],
             _carrierSpell[3], _carrierSpell[4],
             _iconDonor[0], _iconDonor[1], _iconDonor[2],
             _iconDonor[3], _iconDonor[4]);
    LOG_INFO("module",
             "mod-mount-progression: announceXpGain={} interval={}s "
             "starterSpells=[stamina={},predator={},arcane={}] "
             "starterQuest(enable={},id={})",
             _announceXPGain, _announceXPGainIntervalSeconds,
             _starterSpell[0], _starterSpell[1], _starterSpell[2],
             _starterQuestEnabled, _starterQuestId);
}
uint32 MountProgressionMgr::GetIconDonor(uint32 realCarrierSpellId) const
{
    for (size_t i = 0; i < static_cast<size_t>(MountType::MAX); ++i)
        if (_carrierSpell[i] == realCarrierSpellId)
            return _iconDonor[i];
    return 0;
}
void MountProgressionMgr::LoadCatalog()
{
    _catalog.clear();

    // excluded=1 rows (Blizzard internal test/dev spells, unfinished
    // placeholder entries, temporary quest-loaner mounts -- see Slice 6
    // audit migration) are never loaded, so no feature can select them
    // even by spell_id (GetCatalogEntry/`.mount give` simply won't find
    // them, same as any other unknown spell).
    QueryResult result = WorldDatabase.Query(
        "SELECT spell_id, display_id, display_name, rarity, type "
        "FROM mount_progression_catalog WHERE excluded = 0");

    if (!result)
    {
        LOG_WARN("module",
                 "mod-mount-progression: mount_progression_catalog is empty or missing. "
                 "Slice 1 migrations must be applied before this module is useful.");
        return;
    }

    do
    {
        Field* f = result->Fetch();
        CatalogEntry entry;
        entry.spellId     = f[0].Get<uint32>();
        entry.displayId   = f[1].Get<uint32>();
        entry.displayName = f[2].Get<std::string>();
        entry.rarity      = ParseRarity(f[3].Get<std::string>());
        entry.type        = ParseType(f[4].Get<std::string>());
        _catalog.emplace(entry.spellId, std::move(entry));
    } while (result->NextRow());

    LOG_INFO("module", "mod-mount-progression: loaded {} catalog entries.",
             _catalog.size());
}
CatalogEntry const* MountProgressionMgr::GetCatalogEntry(uint32 spellId) const
{
    auto it = _catalog.find(spellId);
    return it != _catalog.end() ? &it->second : nullptr;
}
uint32 MountProgressionMgr::XPToNextLevel(MountRarity rarity, uint16 level) const
{
    if (level >= _maxLevel)
        return 0;
    uint32 base = _xpBase[static_cast<size_t>(rarity)];
    return base * static_cast<uint32>(level) * static_cast<uint32>(level);
}

}  // namespace mod_mount_progression
