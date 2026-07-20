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

- **Item-target** rows (`item_property_override`, keyed by `item_instance.guid`):
  active while that item is equipped. Foundation for item upgrades/mixing.
- **Player-target** rows (`player_property_override`, keyed by character guid +
  `source`): active while the character is online. Foundation for AA and
  mount-progression buffs. `source` ('gm', 'mount', 'aa', ...) namespaces which
  system owns each row — systems clear only their own rows, and the same
  property from different sources stacks additively.

## API for other modules (world-thread only)

```cpp
auto& mgr = mod_property_override::PropertyOverrideMgr::Instance();
mgr.SetPlayerOverride(player, "mount", Property::AttackPower, 120, 0); // 0 = permanent
mgr.ClearPlayerOverrides(player, "mount");
```

Rows persist in the characters DB, survive relogs, and are purged
transactionally on character deletion (`OnPlayerDeleteFromDB`).

## GM commands (stand-in for the future purchase UI)

```
.propover add <slot> <property> <value> [durationSecs]     item-target
.propover clear <slot>
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
- No purchase UI/currency — GM commands only.

No core patches required; all hooks are stock. Tests are pure-logic gtest
suites registered via `mod-property-override.cmake` (build with
`-DBUILD_TESTING=ON`).
