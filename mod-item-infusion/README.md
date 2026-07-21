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
- each stabilizing substance — **tier-banded like every native consumable**
  (a reagent only stabilizes gear up to its own ItemLevel + 15) and
  **multiplicative**: it removes its percent *of the current risk*, one of
  each per attempt (quantities cannot stack), so farmable reagents soften
  shallow gambles but can never trivialize deep ones (defaults span
  Minor/Lesser/Superior Healing Potion −5% through Frost Lotus −8% and
  Eternal Life −10%),
- floored at 2% — no attempt is ever fully safe.
- Coins are deliberately *not* banded: they're account-pooled endgame
  currency, so spending them is a real price at any level.

**Mastery** makes the system fun at every level, not just endgame: gear
beyond the character's level is riskier to infuse ("you don't know how to
mix what you can't yet wield"). An item is mastered once
`charLevel >= RequiredLevel + 10`; below that each missing level adds +2%
risk, capped at +30%, taking the *worse* of target and donor — so a
level-25 feeding their own level-20 quest gear pays nothing extra, while a
twink fed a level-40 BoE donor pays the full +30%. Characters at level 80
count as fully mastered (configurable), leaving the endgame math exactly
as tuned. The surcharge is always shown explicitly ("Beyond your mastery:
+N% risk") in the gossip and chat.

Transfers are deterministic (per-stat `ceil(native × efficiency)`, duplicate
template slots merged before scaling); the dice only decide survival.

## In game

- **Infusion Window** (`addon/InfusionForge/`, hand-copied into each
  player's `Interface/AddOns/` like PropertyOverlay): drag your equipped
  target and a bag sacrifice into the two wells, watch the transferred
  stats and the server-computed risk bar update live, pledge coins /
  check substances (ineligible ones show disabled), confirm through a
  native popup. Open with `/infusion` or the Alchemist's "Open the
  Infusion Window" gossip row. Wire protocol `IFUSE` mirrors
  PropertyOverlay's IPROP (whisper-to-self, 255-byte chunked replies,
  fully server-authoritative — the addon never computes anything).
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
`CoinReductionPercent`, `Substances` (`"itemId:pct,..."` — defaults span
all level bands, Minor Healing Potion through Eternal Life),
`Mastery.{GraceLevels,PenaltyPerLevelPercent,MaxPenaltyPercent,
MasteredAtLevel}`, `MinLevel` (default 10 — the mastery term, not this
gate, is the low-level protection), `AlchemistEntry`, `Enable`, `Debug`.

## Testing runbook (in game, as GM)

Setup:

```
.npc add 96010                   -- spawn the Alchemist (once)
.additem 37711 20                -- Paragon Coins (mitigation)
.additem 35625 3                 -- Eternal Life (endgame substance)
.additem 858 5                   -- Lesser Healing Potion (low-band substance)
.additem 37644                   -- a stat-rich donor for your bags
```

Core loop (gossip): Alchemist -> pick an equipped target (row shows its
current risk; fresh items read `risk 5%`) -> pick a donor -> confirm
screen shows every transferred stat in the `Total (base+bonus)` grammar
plus `INFUSE - destruction risk N%`. Infuse: chat reports the absorbed
stats and next risk; PropertyOverlay tooltip and character sheet update.
`.propover list <slot>` shows the `[mix]` rows; `.infuse list <slot>`
shows infused points + next risk. Repeat infusions on one item -> risk
climbs the curve (5% -> ~50% at paragon-cap-equivalent fill -> 90%).

Window (addon): `/infusion` (or the Alchemist's "Open the Infusion
Window" row). Drag the equipped target from the character sheet into the
left well, a bag donor into the right; stats + risk bar appear and match
the gossip's numbers. Coins/substances re-price the bar live; the Infuse
popup quotes the exact risk; success clears the donor well, destruction
clears both.

Mastery (`.character level 15` on a test character): own-level gear
infuses at plain 5%; an above-level donor adds the red "Beyond your
mastery: +N%" line (cap +30%); at `.character level 80` all penalties
vanish (exemption). Low-band substances (Lesser Healing Potion) work on
starter gear but show "too weak for this gear" against high-req pairs;
Eternal Life works everywhere but only shaves its percent OF the current
risk.

Destruction: `.infuse risk 100` -> infuse a junk target -> target and
donor vanish, rows purge, relog clean. `.infuse risk -1` restores live
math. Cross-checks: `.paragon upgrades <slot>` on a mixed item ignores
`[mix]` rows (separate budgets); `.reset stats` unaffected; mount +
perks + upgrades + infusions all stack in `.propover plist`/`list`.

## Design notes

- Rules: donor always consumed; target destruction fires the same
  `CanItemRemove`/`OnItemDelFromDB` cleanup path every other destruction
  uses, so override rows are purged transactionally.
- Trade a mixed item to a friend and the rows follow at their next login
  (platform behavior).
- Prior-art survey behind the risk model (destruction vs curse vs
  Vaal-locking) is recorded in the repo's PROPOSALS.md, proposal 3.
