# mod-bag-sorter

Sort the items in your **carried bags** (backpack + the 4 equipped bag containers) by talking
to any **innkeeper**, or with a `.sortbags` chat command. Talk to the **Bag & Bank Organizer** —
a dedicated custom NPC standing next to a bank — to deposit your carried items into the bank, or
to sort the bank itself.

## Usage

### Innkeeper gossip
Talk to any innkeeper and pick **"Organize my bags"**. The innkeeper's normal options
(make this your home, etc.) are untouched — the sort option is simply appended. Choose a mode:

- **By type, then quality** — groups items by class/subclass/type, then quality, then item level.
- **By quality** — highest rarity first.
- **By item level** — highest item level first.
- **By name (A–Z)** — alphabetical.
- **Type & quality, quest items to last bag** — sorts the same as "by type, then quality", then
  sweeps all quest-class items into the tail of your bags (the last bag), clear of everything else.

The **Hearthstone** (item 6948) is always pinned to the backpack's first slot after any sort
(toggle with `BagSorter.PinHearthstone`).

### Bag & Bank Organizer gossip
Find the **Bag & Bank Organizer** — a small custom NPC that stands next to a bank teller — and
talk to them. Two options are offered:

> **Why a separate NPC instead of the real banker:** the real banker normally opens the bank
> window directly on click, with no gossip box at all. Making our options appear there required
> forcing the `Gossip` NPC-flag onto every banker, which produced a confusing box mixing our
> options with (or standing in for) the "open my bank" click players actually wanted. A dedicated
> NPC sidesteps all of that — it owns its own gossip menu outright, the real banker is never
> touched, and clicking the banker still opens the bank exactly like it always did.
>
> **Coverage:** currently spawned in **Stormwind** only (next to Newton Burnside in the Trade
> District bank), as a first pass. More cities can be added the same way once this is confirmed
> working — see `data/sql/db-world/` for the spawn definition to copy.

- **"Deposit everything into my bank"** — moves everything that will fit and can legally be
  banked from your backpack and every equipped bag (specialized or not) into the bank. Bag
  containers and, if `BagSorter.PinHearthstone` is set, the Hearthstone are never deposited.
  Anything that doesn't fit or can't be banked is simply left where it is — nothing is ever
  forced in or lost.
- **"Organize my bank"** — sorts your bank storage. Choose a mode (same four as bag sorting,
  minus "quest last", which doesn't apply to the bank). What happens, in order:
  1. If `BankSorter.SwapBags` is set, any unequipped ("spare") bag - one sitting loose in your
     backpack or inside an equipped bag, never one of your 4 equipped bags - with more capacity
     than a purchased bank bag slot is moved into the bank (the smaller/empty bag that was there
     comes back to the spare bag's old spot). Your currently equipped bags are never touched.
  2. Partial stacks of the same item are merged, bank-wide.
  3. Specialized bank bags (herb bag, enchanting bag, mining bag, …) greedily claim matching
     items from anywhere else in the bank — e.g. enchanting materials will move into an
     enchanting bag before anything else, up to its capacity.
  4. Whatever is left is grouped/sorted into the main bank slots and any general-purpose bank
     bags using the chosen mode — the same "type, then quality" ordering bag-sorting uses, so
     e.g. all your tailoring-gathered leathers end up together, grouped by quality.

### Command
```
.sortbags            # defaults to "by type, then quality"
.sortbags type
.sortbags quality
.sortbags ilvl
.sortbags name
.sortbags questlast  # type & quality, then quest items to the last bag
```

There is intentionally no equivalent `.depositbank`/`.sortbank` command — banking only works
through the Bag & Bank Organizer's gossip, so a player must actually be at a bank to use it,
matching how the bank itself works.

## What it does

1. (Optional) **Consolidates partial stacks** of the same item.
2. **Reorders** items into the chosen order, packing them toward the start of your bags (or, for
   the bank, the main bank slots then bank bags).

Items are only ever **moved**, never destroyed. All moves go through the core's validated
`Player::SwapItem` / `CanBankItem`/`BankItem`, so they obey every normal inventory rule.

### Bag families
**Carried bags:** specialized profession bags (herb bag, enchanting bag, …) are sorted
**internally** only; the backpack and general-purpose bags are sorted together as one pool. Items
are never moved between those pools, which guarantees every move is valid. A consequence: a loose
herb sitting in your backpack will **not** be relocated into your herb bag. For the same reason,
"quest items to last bag" targets the last **general** bag — quest items can't be placed into a
profession bag, so if your bottom bag is one, they go to the bag above it.

**Bank:** "Organize my bank" *does* redistribute matching items into specialized bank bags from
anywhere else in the bank (see above) — this is the "possible future enhancement" the carried-bag
sorter above doesn't attempt, made safe for the bank because every candidate slot is already
known and validated up front.

## Configuration

`conf/mod_bag_sorter.conf.dist` → copy to your active worldserver config as
`mod_bag_sorter.conf`:

| Key | Default | Meaning |
| --- | --- | --- |
| `BagSorter.Enable` | `1` | Master on/off for the innkeeper gossip option and the `.sortbags` command. |
| `BagSorter.MergeStacks` | `1` | Consolidate partial stacks before sorting (bags and bank). With `0`, reordering may still incidentally merge two adjacent identical stacks. |
| `BagSorter.Announce` | `1` | Whisper a one-line confirmation after sorting/depositing. |
| `BagSorter.PinHearthstone` | `1` | Always move the Hearthstone (6948) to the backpack's first slot when bag-sorting, and never deposit it when banking. |
| `BankSorter.Enable` | `1` | Master on/off for both Bag & Bank Organizer gossip options. |
| `BankSorter.SwapBags` | `1` | Let "Organize my bank" swap higher-capacity equipped bags into purchased bank bag slots. |

> **Docker note:** add these keys to the *active* worldserver config the container loads, not
> only the `.dist` file, or boot logs a missing-config warning.

## Notes / limitations

- **Command permission:** `.sortbags` reuses `RBAC_PERM_COMMAND_DISMOUNT` (held by the default
  player role) so any player may run it without an auth-DB change. Swap in a dedicated RBAC
  permission if you prefer.
- **Scripted innkeepers:** the module takes over `OnGossipHello` for innkeepers, which would skip
  a *custom C++* gossip script on an innkeeper (extremely rare — innkeepers are DB-gossip driven).
  The Bag & Bank Organizer is its own dedicated NPC (`ScriptName = npc_bag_bank_organizer`), so
  this doesn't apply to it.
- **Spawn data:** `data/sql/db-world/` (module-local SQL, applied the same way as
  `mod-mount-progression`'s) creates the NPC (entry 900001, `creature_template` +
  `creature_template_model` + `creature` + `npc_text`/`gossip_menu` for the greeting text). Rebuild
  and rerun `ac-db-import` after pulling this change, or the NPC won't exist in the world DB yet.
