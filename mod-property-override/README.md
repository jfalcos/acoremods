# mod-property-override

Prototype slice of the **Property Override System** — the shared foundation for
post-cap Alternate Advancement, item mixing, and item upgrades (proposals 2/3/4
in the repo-level `PROPOSALS.md`, which also records the design rationale and
the client-constraint findings behind it).

## What it does

Attaches extra flat stats to a **specific item instance** (`item_instance.guid`).
The stats are real server-side stats (applied through the same accumulators as
enchants) that turn on while the item is equipped and off when it leaves the
slot — with no changes to the item's entry, template, enchant slots, or anything
the client caches.

- **Truth layer (server):** `item_property_override` rows → in-memory per-player
  cache → an idempotent diff-sync (`PropertyOverrideMgr::Sync`) that reconciles
  applied stats against equipped items on every relevant hook plus a 2s safety
  net. Expiry is lazy; DB purge rides the engine's own item-delete transaction.
- **Display layer (addon):** `addon/PropertyOverlay` paints "Upgrade: +N Stat"
  tooltip lines via a whisper-to-self `LANG_ADDON` request/reply protocol
  (queries by slot coordinates; the server swallows the request client→client).
  Install by copying `addon/PropertyOverlay/` into the client's
  `Interface/AddOns/`. Without the addon, stats still work — tooltips just
  don't show them.

## Two target kinds

- **Item-target** rows (`item_property_override`, keyed by `item_instance.guid`
  + `source`): active while that item is equipped. Foundation for item
  upgrades (source `'paragon'`) and infusions (source `'mix'`).
- **Player-target** rows (`player_property_override`, keyed by character guid +
  `source`): active while the character is online. Foundation for AA and
  mount-progression buffs.

Both kinds are `source`-namespaced ('gm', 'mount', 'paragon', 'mix', ...):
systems budget and clear only their own rows, and the same property from
different sources stacks additively.

## API for other modules (world-thread only)

```cpp
auto& mgr = mod_property_override::PropertyOverrideMgr::Instance();
mgr.SetPlayerOverride(player, "mount", Property::AttackPower, 120, 0); // 0 = permanent
mgr.ClearPlayerOverrides(player, "mount");
mgr.AddOverride(player, item, "paragon", Property::Stamina, 15, 0);    // item-target
```

Rows persist in the characters DB, survive relogs, and are purged
transactionally on character deletion (`OnPlayerDeleteFromDB`).

Two shared consumer-facing units also live here:

- `PropertyOverrideItemization.h` — pure itemization facts:
  `PropertyWeight` (Blizzard stat costs, tri-ratings 3x), `NativeBudget`
  (corpus-fitted budget per Quality/ItemLevel), `BudgetSpent(rows, source)`.
- `PropertyOverrideDisplay.h` — the parchment-gossip toolkit (dark palette,
  `QualityColor`, `FormatStatDisplay` "Total (base+bonus)" rows with live
  rating-% previews) so every consumer NPC UI renders stats identically.

Known consumers: `mod-mount-progression` (source `'mount'`), `mod-paragon`
(player source `'paragon'` for perks, item source `'paragon'` for coin
upgrades), `mod-item-infusion` (item source `'mix'`). Modules that include
these headers require this module to be present in the build.

## GM commands (stand-in for the future purchase UI)

```
.propover add <slot> <property> <value> [durationSecs]     item-target (source 'gm')
.propover clear <slot> [source]                            omit source = all systems' rows
.propover list <slot>
.propover padd <property> <value> [durationSecs]           player-target (selected player or self, source 'gm')
.propover pclear [source]
.propover plist
.propover props                                            list all properties
```

`<slot>` = server equipment slot 0-18 (15 = main hand). `<property>` = a name
from `.propover props`, a unique case-insensitive prefix ("sta", "spellpo"),
or the numeric id. Property ids reuse the game's `ITEM_MOD_*` values
(`ItemTemplate.h`), with ids >= 100 as module-custom extensions
(armor/resistances). Supported: primary stats, health/mana, all combat
ratings, attack/spell power, MP5/HP5, spell penetration, block value, armor,
resistances — the apply path mirrors `Player::_ApplyItemBonuses`.

## Known prototype limitations

- Percent-based stats (crit %, haste %) and proc/on-hit effects are not
  supported — those use different modifier channels (aura territory) and
  need their own design pass. Use ratings instead.
- Items traded/mailed to another character activate for the new owner at their
  next login (the load joins `item_instance` for live ownership).

No core patches required; all hooks are stock. Tests are pure-logic gtest
suites registered via `mod-property-override.cmake` (build with
`-DBUILD_TESTING=ON`).

## Testing runbook (in game, as GM)

```
.propover padd sta 50            -- +50 Stamina on yourself (source 'gm')
.propover plist                  -- shows [gm] Stamina +50
.reset stats                     -- character sheet keeps the +50 (accumulators survive)
.propover pclear                 -- bonus gone

.propover add 15 agi 25          -- +25 Agility on your main-hand item
.propover list 15                -- shows [gm] Agility +25
```

Then verify the display + lifecycle behaviors:

- With PropertyOverlay installed, the item tooltip gains "Upgrade: +25
  Agility" (green line). Without it, stats still apply — only the line is
  missing.
- Unequip the item -> character sheet drops the bonus; re-equip -> back
  (2s reconcile at worst). Relog -> persists.
- Trade the item to another character -> bonus activates at the receiver's
  NEXT login (ownership joins live at load).
- Destroy the item (vendor/delete) -> `.propover list` on a fresh copy is
  clean; rows purge with the engine's item-delete transaction.
- Same property from two sources stacks: `.propover padd sta 10` while a
  mount bond or paragon perk also grants Stamina -> `.propover plist`
  shows both rows, sheet shows the sum.
