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
