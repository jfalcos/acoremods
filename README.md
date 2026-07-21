# acoremods

A collection of custom [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) modules authored by jfalcos.

## Modules

| Module | Description |
| ------ | ----------- |
| `mod-bag-sorter` | Sort your carried bags from any innkeeper's gossip, or with `.sortbags`. |
| `mod-custom-items` | Custom items with no client patch (donor-appearance framework). Requires [core patch](core-patches/). |
| `mod-dynamic-ah` | Dynamic auction house. |
| `mod-item-infusion` | Sacrifice gear to empower gear at the risk of destroying it (proposal 3). Consumes `mod-property-override` + `mod-paragon`. Ships the `InfusionForge` addon. |
| `mod-living-world` | Dynamic world / living-world behaviors. |
| `mod-mount-progression` | Per-mount XP / leveling, with permanent mount-bond stat buffs on `mod-property-override`. Requires [core patch](core-patches/). |
| `mod-paragon` | Post-cap/any-level Alternate Advancement: XP divert, Paragon Coins (native currency-tab item), ranked perks, per-instance item upgrades (proposals 2 & 4). |
| `mod-property-override` | **Platform**: per-item-instance and per-character stat overrides, shared itemization math, gossip display toolkit. Ships the `PropertyOverlay` tooltip addon. |
| `mod-terror-zones` | Rotating "terror" zones with empowered open-world content. Requires [core patch](core-patches/). |

## Installation

AzerothCore discovers modules as direct children of its `modules/` directory.
Each module in this repo must therefore appear at `azerothcore-wotlk/modules/<module-name>`.

### Step 1 — Place the modules

Pick one of the following:

#### Option A — Directory junctions (keep the monorepo separate, recommended on Windows)

Clone this repo anywhere, then junction each module into your AzerothCore `modules/` folder:

```powershell
$src = "<path-to>\azerothcore-wotlk\modules"
$dst = "<path-to>\acoremods"
foreach ($m in @("mod-bag-sorter","mod-custom-items","mod-dynamic-ah","mod-item-infusion","mod-living-world","mod-mount-progression","mod-paragon","mod-property-override","mod-terror-zones")) {
  New-Item -ItemType Junction -Path (Join-Path $src $m) -Target (Join-Path $dst $m)
}
```

On Linux/macOS use symlinks instead: `ln -s <path>/acoremods/<module> <path>/azerothcore-wotlk/modules/<module>`.

#### Option B — Copy

Copy each `mod-*` folder directly into `azerothcore-wotlk/modules/`.

### Step 2 — Apply the required core patch

`mod-custom-items`, `mod-mount-progression`, and `mod-terror-zones` add `ScriptMgr` hooks
that stock AzerothCore does not have. **Without this patch they will not compile.** From
your AzerothCore checkout root:

```bash
git apply <path-to>/acoremods/core-patches/acore-core-hooks.patch
# on a newer/drifted core, retry with: git apply --3way <...>/acore-core-hooks.patch
```

See [core-patches/](core-patches/) for the full per-module breakdown of what the patch
changes. `mod-bag-sorter`, `mod-living-world`, and `mod-dynamic-ah` need no core changes.

### Step 3 — Build & database

Re-run CMake and rebuild AzerothCore.

Each module carries its own SQL under `mod-*/data/sql/db-world/` and `db-characters/`.
AzerothCore's DB updater applies these automatically on worldserver startup (with the
updater enabled — `Updates.EnableDatabases` / `AutoSetup`), so there's no manual import
step. `mod-custom-items`, `mod-mount-progression`, `mod-terror-zones`, `mod-living-world`,
`mod-property-override`, `mod-paragon`, and `mod-item-infusion` all ship schema;
`mod-dynamic-ah` and `mod-bag-sorter` use only core tables.

### Step 4 — Post-install checklist (progression stack)

One-time, in game as a GM, after the first boot with the new modules:

1. **Spawn the two NPCs** wherever fits your world (they persist):
   ```
   .npc add 96000     -- Paragon Quartermaster (perks, item upgrades, vendor)
   .npc add 96010     -- Arcane Alchemist (item infusions)
   ```
2. **Install the client addons** — copy both folders into each player's
   `Interface/AddOns/` and fully restart the client once:
   - `mod-property-override/addon/PropertyOverlay/` (tooltip lines for
     upgraded/infused items; everything works without it, tooltips just
     won't show the bonuses)
   - `mod-item-infusion/addon/InfusionForge/` (drag-and-drop infusion
     window; the Alchemist gossip is the addon-less fallback)
3. **Config**: all knobs live in each module's `conf/mod_*.conf.dist` with
   derivation comments; defaults are the tuned values — nothing to edit
   unless you want different balance.

Each of the three progression modules ends its README with a **Testing
runbook**: exact GM commands to walk every feature and its edge cases.
