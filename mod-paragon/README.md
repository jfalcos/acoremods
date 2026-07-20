# mod-paragon

**Alternate Advancement** for AzerothCore (WotLK 3.3.5a) — proposal 2 in the
repo-level `PROPOSALS.md`, revived and modernized from an early WIP skeleton.

Players divert a chosen percentage of **all XP they earn** (any level, not just
cap) into an account-wide **Paragon XP** pool. Each paragon level mails a
**Paragon Coin** plus milestone cosmetics; coins buy **ranked stat perks** and
rotating pets/mounts at the Paragon Quartermaster.

**Depends on [`mod-property-override`](../mod-property-override/)**: perk stats
are permanent player-target override rows (source `'paragon'`), so they persist
across relogs with no reapply logic, survive `.reset stats`, and stack cleanly
with mount bonds and item overrides.

## Player commands

```
.paragon info           PL, progress, coins, current XP allocation
.paragon setxp <0-100>  set your XP divert percentage (level-capped)
.paragon toggle         opt in/out entirely
.paragon perks          list perk ranks, totals, next-rank costs
.paragon buy <perk>     buy the next rank (Strength/Agility/Stamina/
                        Intellect/Spirit/AttackPower/SpellPower)
.paragon upgrade <slot> <property>   buy an item-upgrade chunk (also via NPC)
.paragon upgrades <slot>             show an item's upgrades + budget
```

GM: `.paragon addpx / setlevel / coins / resetperks / debug / reload / vendor`.

## Design notes

- **Earning**: `OnPlayerGiveXP` hook; diverted XP is *subtracted* from leveling
  XP (a real trade-off). All account state is cached at login — the XP hook is
  DB-free and allocation-free. Battlegrounds/arenas and configured maps never
  feed PX. At the level cap, kill/quest/BG XP still reaches the hook (the
  cap discard happens later in `Player::GiveXP`), so capped characters convert
  at their divert rate; exploration XP is zeroed before the hook at cap.
- **Paragon Coin** = commandeered native item entry **37711** ("Currency
  Token Test Token 1"), a Blizzard dev token wired end-to-end for the
  client's **currency tab** (CurrencyTypes.dbc id 1, visible category
  "Miscellaneous") — coins take no bag space, render with a real token icon,
  no client patch, no red question mark. Zero acquisition paths and zero
  owned instances verified before commandeering; retemplated epic/BoP by the
  world migration. (43949 was evaluated first but its CurrencyTypes.dbc row
  has a corrupt category id and the client cannot display it.)
- **Perks**: per-character, ranked; cost = `1 + ranks/CostStepEvery` coins;
  values and caps fully config-tunable. Rank truth in `paragon_perk_ranks`;
  applied stat lives in the override row.
- **Item upgrades** (proposal 4): coins buy flat-stat chunks onto a SPECIFIC
  equipped item instance — any of the 40 supported properties, capped by an
  item-level budget (`BudgetPercent`% of the item's native stat budget; the
  quality slopes are fitted against the real item corpus — see the conf).
  The mod-property-override row IS the entire upgrade state (source
  'paragon'): no extra table, upgrades ride the instance through trades/mail
  (activating at the new owner's next login) and show in tooltips via
  PropertyOverlay. Cost per chunk rises 1→4 coins as the item's budget fills.
  The itemization facts (per-property weights, native budget curve, budget
  accounting) and the parchment-gossip display toolkit were extracted to
  mod-property-override (`PropertyOverrideItemization.h` /
  `PropertyOverrideDisplay.h`) when `mod-item-infusion` became a second
  consumer — this module keeps only its shop policy (chunks, categories,
  labels, coin cost curve, the 30% cap). Paragon Coins also serve as
  mod-item-infusion's optional risk-mitigation currency (read via
  `ParagonMgr::CoinItemId()`).
- **Quartermaster** (creature 96000) is seeded but not spawned — place one
  with `.npc add 96000` wherever fits your world. Gossip: perks, item
  upgrades (slot → category → stat), and the cosmetics stock, which rotates
  on a 4-week cycle (`paragon_vendor_stock.week_id` 0-3).
- Dormant: `season*` columns and the old leaderboard concept (future work).

Tests: `tests/ParagonPerksTests.cpp` (gtest, via `mod-paragon.cmake`,
`-DBUILD_TESTING=ON`).
