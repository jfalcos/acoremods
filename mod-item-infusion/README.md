# mod-item-infusion

Sacrifice gear to empower gear — at the risk of losing it.

Infusion transfers a configurable fraction (default 35%) of a **donor**
item's native stats onto a **target** item as permanent per-instance bonuses.
The donor is always destroyed. Every attempt rolls against a destruction
chance for the *target* that grows the more the target has already been
infused: risk is the balance mechanism instead of a hard cap.

Part of the property-override family:

- **mod-property-override** (required): the bonuses are item-target override
  rows under source `'mix'`; tooltips come from the PropertyOverlay addon;
  the itemization math (per-property weights, corpus-fitted native budgets,
  per-source budget accounting) and the shared parchment-gossip display
  toolkit live there.
- **mod-paragon** (soft dependency): Paragon Coins are the optional
  risk-mitigation currency (`ParagonMgr::CoinItemId()` is the single source
  of truth). With paragon disabled, infusion still works — risk just can't
  be bought down with coins. Paragon's coin upgrades keep their hard 30%
  budget cap and account their budget separately (source `'paragon'`), so
  the two systems never eat each other's headroom.

## The gamble

```
f    = target's accumulated mix points / native (Quality, ItemLevel) budget
risk = clamp(5% + 45% * (f / 30%)^1.6, 5%, 90%)
```

- Fresh item: **5%** destruction chance.
- Mix fill at 30% of the native budget (the same fill paragon's coin cap
  stops at): **50%**.
- Beyond that it climbs toward the **90%** ceiling — you can outgrow the
  coin path, if you keep winning.

Mitigation is chosen per attempt and consumed **win or lose**:

- each pledged Paragon Coin: −5% risk,
- each stabilizing substance (defaults: Runic Healing/Mana Potion −5%,
  Frost Lotus −10%, Eternal Life −20%),
- floored at 2% — no attempt is ever fully safe.

Transfers are deterministic (per-stat `ceil(native × efficiency)`, duplicate
template slots merged before scaling); the dice only decide survival.

## In game

- **Arcane Alchemist** (creature 96010, spawn once with `.npc add 96010`):
  pick an equipped target → pick a bag donor (only items with mappable
  stats are offered) → confirm screen with WoW-style `Total (base+bonus)`
  previews for every transferred stat, the current destruction risk, and
  coin/substance toggles that re-price the risk live → INFUSE.
- `.infuse <targetSlot 0-18> <bag 0-4> <bagSlot> [coins]` — quick path
  (substances only at the Alchemist).
- `.infuse list <slot>` — an item's mix rows, weighted points, next risk.
- `.infuse risk <0-100|-1>` — GM: force the roll for testing (−1 = live).

## Config (`mod_item_infusion.conf.dist`)

All knobs of the model above: `ItemInfusion.EfficiencyPercent`,
`Risk.{Base,Slope,Pivot,Max,Floor}Percent`, `Risk.Exponent`,
`CoinReductionPercent`, `Substances` (`"itemId:pct,..."`), `MinLevel`,
`AlchemistEntry`, `Enable`, `Debug`.

## Design notes

- Rules: donor always consumed; target destruction fires the same
  `CanItemRemove`/`OnItemDelFromDB` cleanup path every other destruction
  uses, so override rows are purged transactionally.
- Trade a mixed item to a friend and the rows follow at their next login
  (platform behavior).
- Prior-art survey behind the risk model (destruction vs curse vs
  Vaal-locking) is recorded in the repo's PROPOSALS.md, proposal 3.
