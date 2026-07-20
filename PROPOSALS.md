# Feature Proposals

Brainstormed ideas from a 2026-07-19 design conversation. None of these are started —
this is a record of what was discussed and the technical findings so far, not a
committed roadmap.

## 1. Pet battles

Pokémon-style turn-based battles for character pets.

- WotLK 3.3.5a's client has no UI for this at all (Pet Battles didn't exist until
  Mists of Pandaria) — needs a custom addon (Lua/XML) for the battle screen.
- The addon-message channel (`CHAT_MSG_ADDON`) already flows through the core
  (`src/server/game/Handlers/ChatHandler.cpp`), so client↔server comms work without
  new dependencies.
- Eluna/AIO is not installed in this stack, so it'd be a static, player-installed
  addon talking to a `PlayerScript` hook rather than server-pushed dynamic UI.
- Scope: a full species/moveset/type-advantage system is a multi-week build; a
  lightweight version (existing pets, scripted duel-like exchange) is more realistic
  as a first cut.

## 2. Post-80 Alternate Advancement (AA)

> **Status (2026-07-19): shipped as the revived `mod-paragon`** — configurable
> XP divert at any level (account-wide PX pool), paragon levels mailing
> Paragon Coins (commandeered native currency-tab item 37711), and ranked
> stat perks on the Property Override System (source 'paragon'). Verified
> in-game. See the module README for the design decisions.

EverQuest-style AA: XP earned past level 80 converts to spendable points instead of
being wasted.

- Feasible via a `PlayerScript::OnGiveXP`-style hook redirecting post-cap XP into a
  custom point pool (new characters-DB table).
- Spending UI can be a gossip-menu NPC — no addon required.
- Stat/spell boosts are straightforward (hook + aura). Talent-tree boosts hit a real
  wall: Blizzard's talent ranks are capped client-side in DBC data, so points can't
  just be dumped into already-maxed talents without a client patch. A separate
  custom "perk track" (gossip-menu list, not the real talent UI) sidesteps this.

## 3. Item mixing module

Combine multiple items into one with blended stats.

- `mod-custom-items` (this repo) is the right structural base — same
  "take input, produce a new item" flow it already provides.
- The hard part isn't plumbing, it's the balance formula (sum-with-diminishing-
  returns vs. item-level-budget-based) so combined items don't power-creep past
  normal gear.

## 4. Item upgrade system (ties into AA points)

> **Status (2026-07-20): shipped inside `mod-paragon`** — Paragon Coins buy
> flat-stat chunks onto specific item instances (override rows ARE the state),
> capped by an item-level budget fitted against the real item corpus, via the
> Quartermaster gossip (WoW-convention "Total (base+bonus)" rows, dark
> parchment palette) or `.paragon upgrade`. Verified in-game. Native vendor
> frames / coin extended-costs remain an MPQ-patch unlock (client
> ItemExtendedCost.dbc has no rows for commandeered items — verified).

Spend points to add stat bonuses (Stamina, Attack Power, etc.) to a *specific* item
instance, without interfering with real enchants/gems/sockets, supporting multiple
stacked/arbitrary stats.

Three approaches were evaluated:

1. **Enchant slot reuse (`PRISMATIC_ENCHANTMENT_SLOT`)** — a second "permanent" slot
   distinct from the profession-enchant slot (`Item.h`, `Player.cpp:5037` confirms it's
   included in the stat-application loop). Native tooltip display, but limited to
   enchant IDs that already exist in the client's DBC, and it's a single slot — can't
   cleanly stack arbitrary upgrades, only swap in a bigger pre-made combo.
2. **Custom item templates (`mod-custom-items`)** — clone the item into a custom
   template entry with arbitrary native stat fields (`item_template`'s 10 stat slots),
   donor-mapped back to the original for identical appearance. Fully arbitrary stats,
   zero collision with enchants/gems. **Real risk found:** the WotLK client persistently
   caches `SMSG_ITEM_QUERY_SINGLE_RESPONSE` to disk keyed by entry ID
   (`ItemHandler.cpp:415` — response echoes the *queried* entry even when the
   substituted template's data is different). Since the donor entry is the item's own
   original entry (already seen/cached by the player before any upgrade), tooltip
   updates after a purchase may not show without a client cache clear. Not fully
   verified empirically — flagged as needing a test in a dev client.
3. **Equip-triggered aura (recommended)** — items already support
   `ITEM_SPELLTRIGGER_ON_EQUIP` (`Player::ApplyItemEquipSpell`, `Player.cpp:7191`),
   which casts a spell aura on equip and removes it on unequip — this is how native
   "Equip: +X stat" items work. Buff/aura tooltip numbers are transmitted live per
   aura-update (not disk-cached like item tooltips), so this avoids the caching problem
   entirely. A custom `AuraScript::CalculateAmount` override reads the player's
   purchased-upgrade total from our own table at apply time. One vessel spell ID per
   stat type (reusing an existing, otherwise-unused spell ID for the icon/name shell,
   scripting its real effect) — no client patch needed. `mod-custom-items`' template
   layer only needs to declare "casts vessel-spell X on equip," a fact that rarely
   changes, so it barely touches the cache-sensitive part of the item template at all.

## 5. Raid Finder

Automated raid queue/matchmaker, like the Dungeon Finder but for raids.

- Not explored in depth yet. Key finding: WotLK's client never shipped an automated
  raid-queue UI at all (that's Cataclysm's Raid Finder) — only the older manual
  "Looking For Group" listing tool supports raids, with no auto-matchmaking or
  teleport.
- Would mean extending AzerothCore's existing LFG system — which is architected
  around 5-man party logic (group size, role balancing, instance locks) — to raid
  sizes. Open question: whether the client's Dungeon Finder panel can even
  queue/display raid-sized content without hitting a hardcoded assumption. Needs
  investigation into the LFG code before scoping further.

## 6. Account-bound mounts

Mounts unlocked on one character become usable by all characters on the account.
Added 2026-07-19, not yet investigated in depth.

- Mounts are per-character learned spells (`character_spell`); account-wide means
  a shared account-scoped table plus an on-login sync that teaches missing mount
  spells. Riding-skill requirements stay per character (share *ownership*, not
  *usability*).
- Needs filtering rules: faction-variant mounts (teach the mirror version or
  skip), class-restricted mounts (paladin/warlock/DK), and quest/achievement-
  reward mounts that assume their unlock path.
- Interacts directly with the **Mount Progression** initiative (mount XP/buffs in
  `mod-mount-progression`) — decide whether mount XP/progression state is per
  character or account-wide at the same time, or the two systems will fight.
- A community module for this likely already exists in the AzerothCore catalogue
  (worth evaluating before building).

## 7. Professions grant XP

Gathering and crafting award character XP. Added 2026-07-19, not yet
investigated in depth.

- Feasible via player-script hooks on skill gain; reward **skill-ups** rather
  than each craft/gather action — skill-ups are naturally rate-limited by recipe
  color, which kills the spam-craft-cheap-recipes exploit for free.
- Tuning knobs: XP per skill-up scaled by character level and skill tier, config
  toggle per profession type (primary/secondary/gathering).
- Ties into **proposal 2 (AA)**: at level cap, profession XP would flow into the
  same post-cap redirect and become AA points.

## 8. Account-wide achievements

Achievements earned on one character are credited to all characters on the
account. Added 2026-07-19, not yet investigated in depth.

- Simplest sound scope: sync *completed* achievements account-wide on login;
  leave in-progress criteria per character (criteria state is much hairier to
  merge than completion rows).
- Follow-on decisions: whether achievement *rewards* (titles, mounts, tabards)
  propagate too — mount rewards would ride on proposal 6's machinery.
- A known community module (`mod-account-achievements`-style) exists for
  AzerothCore — evaluate it first; this may be an adopt-and-configure rather
  than a build.

Note: 6–8 don't sit on the Property Override System below — 6 and 8 share an
"account-scope sync" pattern (account-scoped table + on-login reconciliation)
that could itself become a small shared foundation; 7 is an XP-source hook that
feeds the AA pipeline.

---

# Shared foundation for 2/3/4: Property Override System

**Status (2026-07-19): prototype implemented and verified in-game** as
`mod-property-override` — full lifecycle pass (equip/unequip/swap/relog/
`.reset stats`/expiry/destroy) with no drift, addon tooltip round-trip
working, property set extended to the full flat+ratings tier on `ITEM_MOD_*`
ids. See the module README. Percent/proc properties remain future work.

Design landed in a 2026-07-19 follow-up conversation. Proposals 2 (AA), 3 (item
mixing), and 4 (item upgrades) all reduce to one system: **override N properties
of a target (character or item instance), with exact values, for any duration,
without colliding with existing systems or fighting the client's caches.**
Nothing below is implemented yet.

## Client constraints established (3.3.5a)

These findings drove the architecture; each was checked against the core source
where possible:

1. **Item tooltips are entry-keyed and disk-cached** (`Cache/WDB/itemcache.wdb`).
   The client only re-queries entries it hasn't seen. Mutating a definition the
   client already cached is a losing game — and the cache is per *entry*, so one
   entry can never show two different per-instance tooltips. (This is the real
   reason the original donor idea in approach 2 of proposal 4 fails: it reuses
   the item's own, already-cached entry.)
2. **Icons/models come from the client's own Item.dbc**, which ignores the
   `displayInfoID` in the query response. Entry IDs missing from the client's
   Item.dbc render as a red question mark permanently. So freshly minted entry
   IDs fix tooltips but break icons unless the entry exists client-side.
3. **No per-instance spell/buff channel exists on items.** `EItemFields`
   (`UpdateFields.h`) is the complete per-instance state the client can know:
   owner/creator, stack, duration, spell charges, flags, 12 enchantment slots,
   random-property seed+ID, durability, played-time. The enchant slots are the
   only stat channel and every slot is owned (0 profession, 1 temp imbues/
   poisons, 2–4 gems, 5 socket bonus, 6 prismatic/buckle, 7–11 random suffix) —
   and all resolve against client `SpellItemEnchantment.dbc`, so only pre-baked
   magnitudes can display. Auras attach to units only.
4. **Aura amounts are not transmitted in 3.3.5** (`SMSG_AURA_UPDATE` carries no
   effect amounts). A server-side `CalculateAmount` override applies real stats,
   but buff tooltips render Spell.dbc base numbers. This corrects the premise of
   proposal 4's approach 3 ("aura tooltip numbers are transmitted live") — auras
   give correct mechanics with wrong displayed numbers.
5. **The 2D UI is Lua/XML and fully addon-moddable; the data layer and 3D
   rendering are hardcoded.** An addon (plain `Interface/AddOns` folder, no MPQ)
   can hook `GameTooltip`, replace bag icons/names for unknown entries, and build
   custom windows — fed by server↔client addon messages (`CHAT_MSG_ADDON`,
   already flows through `ChatHandler.cpp`; ~255-byte message limit). Item links
   carry a per-instance uniqueID (low GUID), so an addon *can* distinguish two
   copies of the same item — the per-instance display the native pipeline can't
   do. What Lua cannot touch: the itemcache/`GetItemInfo` data layer, and
   equipped 3D models (Item.dbc → M2 in the binary).

## Architecture: two layers

**Layer 1 — Truth (server; the shared foundation).** A generic override store:
`(target: player GUID or item GUID, property, value, expiry)` plus application
hooks on equip/unequip/login/stat-calculation (direct stat application; no
vessel spells needed for mechanics). Item entries are never touched, so nothing
the client caches ever changes. N properties, exact values, permanent or timed,
zero collisions. All three mods sit on this.

**Layer 2 — Display (companion addon + addon messages).** Paints what the data
layer can't know: per-instance tooltip lines (addon resolves the item's
uniqueID and asks the server), red-question-mark/name fixes for any custom
entries, and custom UIs (AA spender, upgrade/mixing windows). Request/reply
protocol with client-side caching given the message size limit. Bundle with the
distributed client; optional server-side handshake to verify presence. Players
without the addon still get correct *mechanics* (stats are server-authoritative
everywhere, including combat against them) — they just see vanilla tooltips.

**Demoted to optional — entry synthesis.** Two schemes were designed for
native (addon-less) display and remain on the shelf if ever needed:
*donor siblings* (synthesize upgrade templates onto existing unobtainable
Item.dbc entries sharing the base item's displayInfoID+inventoryType; no client
patch; pool depth varies per model) and *pre-provisioned ID pools* (one-time MPQ
patch appending K reserved Item.dbc rows per distinct appearance tuple; runtime
minting stays fully dynamic). Core principle either way: **never mutate an entry
the client has seen — only mint new, immutable ones.** Only worth revisiting for
addon-less display fidelity or if mixing should mint standalone new-appearance
items.

## How the three mods map onto this

- **AA (proposal 2):** player-GUID overrides + XP-redirect hook + spend UI
  (gossip first, addon later). No item/cache issues at all.
- **Item upgrades (proposal 4):** item-GUID overrides + addon tooltip
  decoration. Entry untouched → no cache, Item.dbc, or enchant-slot collisions.
- **Item mixing (proposal 3):** same machinery applied at creation time (or new
  templates via `mod-custom-items` if mixing outputs standalone items). The open
  problem is the balance formula, not plumbing.

## First prototype slice (validates both layers)

Smallest end-to-end proof, verified in a dev client:

1. Override table + equip/unequip/login hooks applying one stat from it —
   confirm no double-application across equip cycles and relogs.
2. Companion addon: extract uniqueID from bag/equipped tooltips, round-trip an
   addon message, paint one tooltip line.
3. (Only if entry synthesis returns) entry-swap refresh behavior and whether the
   client accepts unsolicited `SMSG_ITEM_QUERY_SINGLE_RESPONSE`.
