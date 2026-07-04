// Slice 10 Pass 2 — per-TZ "contract" credit accrual + mailed reward.
//
// Credit accrues write-through to character_terror_zones_progress on each
// eligible kill while a player is in an empowered zone (AccrueContractCredit,
// from OnPlayerCreatureKill). When a rotation ends, RunRotation calls
// MailContractRewards, which reads that table, mails a credit-scaled reward
// (gold + an optional archetype gear piece) to each character by guid —
// offline-safe, since the table (not live player state) is the source of
// truth — and deletes the settled rows.

#include "TerrorZonesMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Group.h"
#include "GroupReference.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "StringFormat.h"
#include "Util.h"
#include "WorldSession.h"

#include <algorithm>
#include <string>

namespace mod_terror_zones
{

void TerrorZonesMgr::AccrueContractCredit(Creature* killed, Player* killer)
{
    if (!IsContractEnabled() || !killed || !killer || !killer->IsInWorld())
        return;

    // Bots farm empowered zones constantly — they must not bank contract
    // credit or receive bounty mail. Exclude the killer here; bot group
    // members are filtered inside accrueOne below.
    if (WorldSession* ks = killer->GetSession())
        if (ks->IsBot())
            return;

    uint32 zoneId = killer->GetZoneId();
    ActiveSlot slot;
    if (!TryGetSlotForZone(zoneId, slot))
        return;  // not an empowered zone — no contract

    // Event bosses (huge HP) count too; otherwise require the same
    // eligibility the level-scaler uses so critters / pets / friendlies
    // don't accrue credit.
    bool isEventBoss =
        _eventBossSpawnIndex.count(killed->GetGUID().GetRawValue()) > 0;
    if (!isEventBoss && !IsScalingEligible(killed))
        return;

    uint64 tickAt = _rotation.tickAt;
    if (tickAt == 0)
        return;

    uint32 credit = KillCredit(killed->GetMaxHealth(),
                               _contractCreditPerKillDivisor);
    if (credit == 0)
        return;

    uint32 capVal = _contractCreditCapPerZone
                  ? _contractCreditCapPerZone : 0xFFFFFFFFu;
    uint32 addCredit = std::min(credit, capVal);
    uint8 tierVal = static_cast<uint8>(slot.tier);

    // Zone display name for the player-facing progress lines.
    std::string zoneName;
    {
        auto pit = _poolIndex.find(zoneId);
        if (pit != _poolIndex.end() && pit->second < _pool.size())
            zoneName = _pool[pit->second].displayName;
        if (zoneName.empty())
            zoneName = std::to_string(zoneId);
    }

    // Upsert one recipient's row, capturing class / spec / level / tier so
    // the offline mail-out can resolve the gear entry without the player.
    auto accrueOne = [&](Player* p)
    {
        if (!p || !p->IsInWorld() || !p->IsAlive())
            return;
        if (WorldSession* s = p->GetSession(); s && s->IsBot())
            return;  // bots don't bank contract credit
        if (p->GetZoneId() != zoneId)
            return;  // the contract is per-zone
        uint32 guidLow = p->GetGUID().GetCounter();
        uint32 level   = p->GetLevel();
        uint32 classId = p->getClass();
        uint32 spec    = p->GetMostPointsTalentTree();
        CharacterDatabase.Execute(
            "INSERT INTO character_terror_zones_progress "
            "(guid, tick_at, zone_id, credit, player_level, player_class, "
            "spec_index, tier, mailed) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, 0) "
            "ON DUPLICATE KEY UPDATE credit = LEAST(credit + {}, {}), "
            "player_level = {}, player_class = {}, spec_index = {}, tier = {}",
            guidLow, tickAt, zoneId, addCredit, level, classId, spec, tierVal,
            addCredit, capVal, level, classId, spec, tierVal);

        // Independent, cumulative (not rotation-scoped) bucket feeding the
        // permanent Tier-N teleport-spell unlock — same credit, separate
        // ledger, never capped/reset by the rotation.
        AccrueTierTeleportCredit(p, tierVal, addCredit);

        // Best-effort session-running total for progress chat (the DB row
        // is the reward source of truth; this drives the player-facing
        // lines only and resets each rotation tick).
        uint64 key = (static_cast<uint64>(guidLow) << 32)
                   | static_cast<uint64>(zoneId);
        uint32 before = _contractMsgCredit[key];
        uint32 after = std::min<uint32>(before + addCredit, capVal);
        _contractMsgCredit[key] = after;

        if (_debug)
            LOG_INFO("module",
                     "mod-terror-zones: contract credit +{} (now {} / cap {}) "
                     "guid={} zone={} tier={}",
                     addCredit, after, capVal, guidLow, zoneId,
                     static_cast<uint32>(tierVal));

        if (!_contractAnnounceProgress || !IsAnnounceEnabled(p))
            return;
        ChatHandler ch(p->GetSession());
        // First credit this rotation in this zone — explain the contract.
        if (before == 0)
            ch.PSendSysMessage(
                "|cff33ff99[Terror Zone]|r You've taken up the terror of {}. "
                "Slay its denizens — your bounty is mailed when the terror "
                "lifts.", zoneName);
        // Crossed the gear threshold — the mail now carries a gear piece.
        if (_contractGearCreditThreshold > 0
            && before < _contractGearCreditThreshold
            && after >= _contractGearCreditThreshold)
            ch.PSendSysMessage(
                "|cff33ff99[Terror Zone]|r Your {} bounty now includes a "
                "spoil of war.", zoneName);
        // Periodic milestone line.
        if (_contractProgressEveryCredit > 0
            && (before / _contractProgressEveryCredit)
                   != (after / _contractProgressEveryCredit))
            ch.PSendSysMessage(
                "|cff33ff99[Terror Zone]|r {} bounty: {} credit banked.",
                zoneName, after);
    };

    if (Group* grp = killer->GetGroup())
    {
        for (GroupReference* itr = grp->GetFirstMember(); itr;
             itr = itr->next())
            accrueOne(itr->GetSource());
    }
    else
    {
        accrueOne(killer);
    }
}

void TerrorZonesMgr::MailContractRewards(uint64 beforeTickAt)
{
    if (!IsContractEnabled() || beforeTickAt == 0)
        return;

    QueryResult r = CharacterDatabase.Query(
        "SELECT guid, zone_id, credit, player_level, player_class, "
        "spec_index, tier FROM character_terror_zones_progress "
        "WHERE tick_at < {} AND mailed = 0 AND credit > 0",
        beforeTickAt);
    if (!r)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    uint32 mailed = 0;
    uint32 withGear = 0;

    do
    {
        Field* f       = r->Fetch();
        uint32 guidLow = f[0].Get<uint32>();
        uint32 zoneId  = f[1].Get<uint32>();
        uint32 credit  = f[2].Get<uint32>();
        uint8  level   = f[3].Get<uint8>();
        uint8  classId = f[4].Get<uint8>();
        uint8  spec    = f[5].Get<uint8>();
        uint8  tierRaw = f[6].Get<uint8>();

        Tier tier = (tierRaw >= TIER_1 && tierRaw <= TIER_5)
                  ? static_cast<Tier>(tierRaw) : TIER_1;
        float tierMult = _contractTierGoldMult[tier];

        uint32 gold = ContractGoldCopper(credit, _contractGoldPerCreditCopper,
                                         tierMult, _contractGoldCapCopper);

        // Resolve the zone display name from the pool (write-once).
        std::string zoneName;
        auto it = _poolIndex.find(zoneId);
        if (it != _poolIndex.end() && it->second < _pool.size())
            zoneName = _pool[it->second].displayName;
        if (zoneName.empty())
            zoneName = std::to_string(zoneId);

        // Optional archetype gear piece when credit clears the threshold
        // and the (band, tier, archetype, slot) cell is populated.
        bool gaveGear = false;
        Item* gearItem = nullptr;
        if (_contractGearCreditThreshold > 0
            && credit >= _contractGearCreditThreshold)
        {
            Archetype arch = ArchetypeForClassSpec(classId, spec);
            if (arch != ARCHETYPE_NONE)
            {
                uint8 band = ContractBandIndexForLevel(level);
                ArmorSlot armorSlot = static_cast<ArmorSlot>(
                    urand(0, ARMOR_SLOT_COUNT - 1));
                uint32 entry = EncodeClassDropEntry(band, tier, arch,
                                                    armorSlot);
                if (entry != 0 && _classDropEntries.count(entry)
                    && sObjectMgr->GetItemTemplate(entry))
                {
                    gearItem = Item::CreateItem(entry, 1, nullptr);
                    if (gearItem)
                    {
                        gearItem->SaveToDB(trans);
                        gaveGear = true;
                    }
                }
            }
        }

        if (gold == 0 && !gaveGear)
            continue;  // nothing to send; row is removed by the bulk delete

        std::string subject = "Terror Zone Bounty";
        std::string body = Acore::StringFormat(
            "Your campaign in {} has been tallied. Credit earned: {}.{}",
            zoneName, credit,
            gaveGear ? " A spoil of war is enclosed." : "");

        MailDraft draft(subject, body);
        if (gold > 0)
            draft.AddMoney(gold);
        if (gearItem)
            draft.AddItem(gearItem);

        ObjectGuid pguid = ObjectGuid::Create<HighGuid::Player>(guidLow);
        Player* online = ObjectAccessor::FindConnectedPlayer(pguid);
        draft.SendMailTo(trans, MailReceiver(online, guidLow),
                         MailSender(MAIL_NORMAL, 0));

        ++mailed;
        if (gaveGear)
            ++withGear;

        if (_debug)
            LOG_INFO("module",
                     "mod-terror-zones: contract mail guid={} zone={} ({}) "
                     "credit={} tier={} gold={}c gear={}",
                     guidLow, zoneId, zoneName, credit,
                     static_cast<uint32>(tier), gold,
                     gaveGear ? "yes" : "no");
    } while (r->NextRow());

    // Settle: remove every row for the ended rotation(s) in the same
    // transaction as the mail writes, so a crash can't double-send.
    trans->Append(
        "DELETE FROM character_terror_zones_progress WHERE tick_at < {}",
        beforeTickAt);
    CharacterDatabase.CommitTransaction(trans);

    LOG_INFO("module",
             "mod-terror-zones: contract mail-out before_tick={} mailed={} "
             "(with_gear={})", beforeTickAt, mailed, withGear);
}

uint32 TerrorZonesMgr::GetContractCreditFor(uint32 guidLow,
                                            uint32 zoneId) const
{
    uint64 tickAt = _rotation.tickAt;
    if (tickAt == 0 || guidLow == 0 || zoneId == 0)
        return 0;
    QueryResult r = CharacterDatabase.Query(
        "SELECT credit FROM character_terror_zones_progress "
        "WHERE guid = {} AND tick_at = {} AND zone_id = {}",
        guidLow, tickAt, zoneId);
    if (!r)
        return 0;
    return r->Fetch()[0].Get<uint32>();
}

} // namespace mod_terror_zones
