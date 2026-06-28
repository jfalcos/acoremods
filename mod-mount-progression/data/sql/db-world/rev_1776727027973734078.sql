-- ============================================================
-- Mount Progression — Slice 3 — Carrier buff spells (hand-curated)
-- ============================================================
-- HAND-CURATED. Module-owned table; does NOT touch the native
-- `spell_dbc` overlay. The mod-mount-progression module loads
-- this table at boot via the OnAfterLoadDBCStores core hook
-- (added in src/server/game/...; see git log for the small core
-- patch labeled "Mount Progression — Slice 3"), calling
-- sSpellStore.LoadFromDB("mount_progression_carrier_spells",
-- SpellEntryfmt) so these rows get merged into sSpellStore
-- before SpellMgr::LoadSpellInfoStore() builds the spell map.
--
-- Schema mirrors `spell_dbc` exactly via CREATE TABLE LIKE so
-- DBCDatabaseLoader (which positionally maps SpellEntryfmt to
-- the SQL row) can read it unchanged. If a future AC release
-- changes `spell_dbc`'s schema, this table must follow — the
-- LoadFromDB call will assert on a column-count mismatch at
-- worldserver boot, surfacing the drift loudly rather than
-- silently dropping carrier rows.
--
-- Reserved spell ID range: 80000..80004. These IDs do NOT exist
-- in the client's Spell.dbc (so the buff tray will render as
-- "Unknown Spell" until a client patch MPQ ships) but the server
-- mechanics work end-to-end.
--
-- Per-type effect mapping (verified against SharedDefines.h and
-- SpellAuraEffects.cpp HandleAuraMod* implementations):
--   80000 Stamina     MOD_STAT(29)          misc=STAT_STAMINA(2)
--   80001 Predator    MOD_ATTACK_POWER(99)  misc=0
--   80002 Agility     MOD_STAT(29)          misc=STAT_AGILITY(1)
--   80003 Mechanical  MOD_RESISTANCE(22)    misc=MASK_NORMAL(1)
--   80004 Arcane      MOD_DAMAGE_DONE(13)   misc=MASK_MAGIC(126)
--                   + MOD_HEALING_DONE(135) misc=0
--
-- Common attributes for all five rows:
--   Attributes    = 0x81000000
--     ALLOW_WHILE_MOUNTED | NO_AURA_CANCEL
--     (PASSIVE is intentionally NOT set — Aura::CanBeSentToClient at
--      SpellAuras.cpp:1085 returns false for passive non-area auras,
--      which skips visible-slot assignment and never builds the client
--      packet. Non-passive still applies identically via AddAura().)
--   AttributesEx3 = 0x00100000
--     ALLOW_AURA_WHILE_DEAD
--   DurationIndex = 21 (permanent / infinite)
--   ImplicitTargetA_1 = 1 (TARGET_UNIT_CASTER — server AddAura)
--   EffectBasePoints = 0 (placeholder; runtime ChangeAmount sets
--                          the real magnitude)
-- ============================================================

CREATE TABLE IF NOT EXISTS `mount_progression_carrier_spells` LIKE `spell_dbc`;

DELETE FROM `mount_progression_carrier_spells` WHERE `ID` BETWEEN 80000 AND 80004;

-- Stamina carrier (MOD_STAT → STAT_STAMINA)
INSERT INTO `mount_progression_carrier_spells`
  (`ID`, `Attributes`, `AttributesEx3`, `DurationIndex`, `RangeIndex`,
   `Effect_1`, `EffectAura_1`, `EffectBasePoints_1`, `EffectMiscValue_1`,
   `ImplicitTargetA_1`, `SpellIconID`,
   `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
  (80000, 2164260864, 1048576, 21, 1,
   6, 29, 0, 2,
   1, 0,
   'Mount Bond: Stamina', 0);

-- Predator carrier (MOD_ATTACK_POWER)
INSERT INTO `mount_progression_carrier_spells`
  (`ID`, `Attributes`, `AttributesEx3`, `DurationIndex`, `RangeIndex`,
   `Effect_1`, `EffectAura_1`, `EffectBasePoints_1`, `EffectMiscValue_1`,
   `ImplicitTargetA_1`, `SpellIconID`,
   `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
  (80001, 2164260864, 1048576, 21, 1,
   6, 99, 0, 0,
   1, 0,
   'Mount Bond: Predator', 0);

-- Agility carrier (MOD_STAT → STAT_AGILITY)
INSERT INTO `mount_progression_carrier_spells`
  (`ID`, `Attributes`, `AttributesEx3`, `DurationIndex`, `RangeIndex`,
   `Effect_1`, `EffectAura_1`, `EffectBasePoints_1`, `EffectMiscValue_1`,
   `ImplicitTargetA_1`, `SpellIconID`,
   `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
  (80002, 2164260864, 1048576, 21, 1,
   6, 29, 0, 1,
   1, 0,
   'Mount Bond: Agility', 0);

-- Mechanical carrier (MOD_RESISTANCE → physical armor)
INSERT INTO `mount_progression_carrier_spells`
  (`ID`, `Attributes`, `AttributesEx3`, `DurationIndex`, `RangeIndex`,
   `Effect_1`, `EffectAura_1`, `EffectBasePoints_1`, `EffectMiscValue_1`,
   `ImplicitTargetA_1`, `SpellIconID`,
   `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
  (80003, 2164260864, 1048576, 21, 1,
   6, 22, 0, 1,
   1, 0,
   'Mount Bond: Mechanical', 0);

-- Arcane carrier (MOD_DAMAGE_DONE all magic schools + MOD_HEALING_DONE)
INSERT INTO `mount_progression_carrier_spells`
  (`ID`, `Attributes`, `AttributesEx3`, `DurationIndex`, `RangeIndex`,
   `Effect_1`, `EffectAura_1`, `EffectBasePoints_1`, `EffectMiscValue_1`,
   `ImplicitTargetA_1`,
   `Effect_2`, `EffectAura_2`, `EffectBasePoints_2`, `EffectMiscValue_2`,
   `ImplicitTargetA_2`,
   `SpellIconID`, `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
  (80004, 2164260864, 1048576, 21, 1,
   6, 13, 0, 126,
   1,
   6, 135, 0, 0,
   1,
   0, 'Mount Bond: Arcane', 0);
