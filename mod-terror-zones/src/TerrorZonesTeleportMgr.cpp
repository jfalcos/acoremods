// Teleport unlock — cumulative (not rotation-scoped) credit earned
// fighting in a zone empowered at a given tier unlocks that tier as a
// permanent teleport destination, selectable from a single multi-tier
// beacon item's gossip menu (TerrorZonesTeleportItem.cpp) — granted once,
// on the first tier ever unlocked. Independent of, but fed by, the same
// per-kill credit AccrueContractCredit already computes for the mailed
// contract reward (see TerrorZonesContractMgr.cpp).
#include "TerrorZonesTeleportMgr.h"
#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "QueryResult.h"
#include "StringFormat.h"
#include "WorldSession.h"

namespace mod_terror_zones
{

TerrorZonesTeleportMgr& TerrorZonesTeleportMgr::Instance()
{
    static TerrorZonesTeleportMgr inst;
    return inst;
}

void TerrorZonesTeleportMgr::LoadConfig()
{
    _teleportEnabled = sConfigMgr->GetOption<bool>(
        "TerrorZones.Teleport.Enable", true);
    _teleportUnlockThreshold[TIER_1] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.UnlockThreshold.T1", 800);
    _teleportUnlockThreshold[TIER_2] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.UnlockThreshold.T2", 1200);
    _teleportUnlockThreshold[TIER_3] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.UnlockThreshold.T3", 1800);
    _teleportUnlockThreshold[TIER_4] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.UnlockThreshold.T4", 2600);
    _teleportUnlockThreshold[TIER_5] = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.UnlockThreshold.T5", 4000);
    // 0 (default) means "not configured" — no item entry assigned yet, so
    // an unlock has nothing to grant.
    _teleportItemEntry = sConfigMgr->GetOption<uint32>(
        "TerrorZones.Teleport.ItemEntry", 0);
}

bool TerrorZonesTeleportMgr::IsEnabled() const
{
    return TerrorZonesMgr::Instance().IsEnabled() && _teleportEnabled;
}

void TerrorZonesTeleportMgr::LoadTierTeleportProgress(Player* player)
{
    if (!player)
        return;
    uint32 guidLow = player->GetGUID().GetCounter();

    std::array<TierProgress, TIER_MAX + 1> progress{};

    QueryResult r = CharacterDatabase.Query(
        "SELECT tier, lifetime_credit, unlocked "
        "FROM character_terror_zones_tier_progress WHERE guid = {}",
        guidLow);
    if (r)
    {
        do
        {
            Field* f = r->Fetch();
            uint8 tier = f[0].Get<uint8>();
            if (tier < 1 || tier > TIER_MAX)
                continue;
            progress[tier].lifetimeCredit = f[1].Get<uint32>();
            progress[tier].unlocked = f[2].Get<uint8>() != 0;
        } while (r->NextRow());
    }

    _tierProgress[guidLow] = progress;
}

void TerrorZonesTeleportMgr::UnloadTierTeleportProgress(ObjectGuid guid)
{
    _tierProgress.erase(guid.GetCounter());
}

void TerrorZonesTeleportMgr::AccrueTierTeleportCredit(Player* player, uint8 tier,
                                                       uint32 addCredit)
{
    if (!IsEnabled() || !player || !player->IsInWorld())
        return;
    if (tier < 1 || tier > TIER_MAX || addCredit == 0)
        return;

    uint32 guidLow = player->GetGUID().GetCounter();
    auto& progress = _tierProgress[guidLow][tier];
    if (progress.unlocked)
        return;  // already unlocked — nothing left to accrue

    uint32 before = progress.lifetimeCredit;
    uint32 after = before + addCredit;
    progress.lifetimeCredit = after;

    CharacterDatabase.Execute(
        "INSERT INTO character_terror_zones_tier_progress "
        "(guid, tier, lifetime_credit, unlocked) VALUES ({}, {}, {}, 0) "
        "ON DUPLICATE KEY UPDATE lifetime_credit = {}",
        guidLow, static_cast<uint32>(tier), after, after);

    uint32 threshold = _teleportUnlockThreshold[tier];
    if (threshold == 0 || after < threshold)
        return;

    // Crossed the threshold — unlock for good.
    progress.unlocked = true;
    CharacterDatabase.Execute(
        "UPDATE character_terror_zones_tier_progress SET unlocked = 1 "
        "WHERE guid = {} AND tier = {}",
        guidLow, static_cast<uint32>(tier));

    // The beacon covers every tier via its gossip menu — only grant it
    // once, on whichever tier happens to unlock first. Fall back to
    // mailing it if bags are full: AddItem() silently drops the item
    // in that case (no mailbox fallback of its own — see its own
    // "TODO: Send to mailbox if no space" — and progress.unlocked is
    // already permanently set above, so a bag-space failure here would
    // otherwise lose the beacon for good).
    if (_teleportItemEntry != 0 && !player->HasItemCount(_teleportItemEntry, 1, true))
    {
        if (!player->AddItem(_teleportItemEntry, 1))
        {
            if (Item* mailItem = Item::CreateItem(_teleportItemEntry, 1))
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                mailItem->SaveToDB(trans);
                MailDraft(
                    "Terror Zone Beacon",
                    Acore::StringFormat(
                        "Your bags were full when you proved yourself "
                        "against Tier {} terror, so your beacon is "
                        "enclosed here instead.",
                        static_cast<uint32>(tier)))
                    .AddItem(mailItem)
                    .SendMailTo(trans, player, MailSender(MAIL_NORMAL, 0));
                CharacterDatabase.CommitTransaction(trans);
            }
        }
    }

    if (TerrorZonesMgr::Instance().IsDebug())
        LOG_INFO("module",
                 "mod-terror-zones: guid={} unlocked Tier {} teleport "
                 "(credit {}/{}).",
                 guidLow, static_cast<uint32>(tier), after, threshold);

    ChatHandler(player->GetSession())
        .PSendSysMessage(
            "|cff33ff99[Terror Zone]|r You have proven yourself against "
            "Tier {} terror — use your Terror Zone Beacon to teleport "
            "there at will.",
            static_cast<uint32>(tier));
}

bool TerrorZonesTeleportMgr::IsTierUnlockedFor(Player const* player, uint8 tier) const
{
    if (!player || tier < 1 || tier > TIER_MAX)
        return false;
    auto it = _tierProgress.find(player->GetGUID().GetCounter());
    if (it == _tierProgress.end())
        return false;
    return it->second[tier].unlocked;
}

bool TerrorZonesTeleportMgr::TeleportPlayerToTier(Player* player, uint8 tier)
{
    if (!player)
        return false;
    if (tier < 1 || tier > TIER_MAX)
        return false;

    TerrorZonesMgr& core = TerrorZonesMgr::Instance();
    ActiveRotation rot = core.GetActiveRotation();
    for (ActiveSlot const& slot : rot.slots)
    {
        if (static_cast<uint8>(slot.tier) != tier)
            continue;

        for (PoolEntry const& zone : core.GetPool())
        {
            if (zone.zoneId != slot.zoneId)
                continue;
            if (zone.tpMap < 0)
            {
                ChatHandler(player->GetSession())
                    .PSendSysMessage(
                        "|cff33ff99[Terror Zone]|r {} (Tier {}) has no "
                        "teleport landing point configured yet.",
                        zone.displayName, static_cast<uint32>(tier));
                return false;
            }

            player->TeleportTo(static_cast<uint32>(zone.tpMap), zone.tpX,
                                zone.tpY, zone.tpZ, zone.tpO);
            return true;
        }
    }

    ChatHandler(player->GetSession())
        .PSendSysMessage(
            "|cff33ff99[Terror Zone]|r No zone is currently empowered at "
            "Tier {}.",
            static_cast<uint32>(tier));
    return false;
}

}  // namespace mod_terror_zones
