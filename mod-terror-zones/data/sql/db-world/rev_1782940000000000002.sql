-- ============================================================
-- Terror Zones — Teleport-spell unlock — landing points + spell script
-- ============================================================
-- HAND-CURATED. First-pass wiring for ONE tier (Tier 1, spell 1206
-- "Dummy Proc" — a real, currently-unreferenced Blizzard internal
-- SPELL_EFFECT_DUMMY-only spell, confirmed via spell_dbc + a full
-- cross-check against item_template/trainer_spell/quest_template/
-- smart_scripts/spell_proc/spell_required/spell_linked_spell/etc, so
-- reusing it here can't collide with any real content) to validate the
-- approach end-to-end before wiring the remaining 4 tiers + other 21
-- pool zones.
--
-- 1. `terror_zones_pool` gains a per-zone teleport landing point
--    (tp_map/tp_x/tp_y/tp_z/tp_o). tp_map defaults to -1 ("not configured
--    yet" — TeleportPlayerToTier refuses to teleport there and sends a
--    chat error instead of dropping the player somewhere wrong/nowhere).
--    Only Duskwood (zone 10) is populated for this first pass, using the
--    exact live coordinates of the "Deputy Willem" NPC in Darkshire
--    (creature.guid 79942) — a real, currently-standing NPC, so the spot
--    is guaranteed walkable ground.
-- 2. `spell_script_names` binds spell 1206 to the new SpellScript class
--    `spell_tz_teleport_tier` (TerrorZonesTeleportSpells.cpp). Which
--    tier a given spell id resolves to is config-driven
--    (TerrorZones.Teleport.Spell.T1..T5), not hardcoded in SQL.
-- ============================================================

-- Idempotency guard: this ALTER ran against the live DB previously but
-- the run was never recorded in the `updates` tracking table
-- (state/record drift discovered 2026-07-05 while rebuilding the
-- ac-db-import image for an unrelated change). Real MySQL (this
-- deployment runs MySQL 8.4) has no `ADD COLUMN IF NOT EXISTS` --
-- that's a MariaDB-only extension -- so the standard information_schema
-- + dynamic-SQL idiom is used instead to make each ADD COLUMN safe to
-- replay.
SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'terror_zones_pool'
      AND column_name = 'tp_map');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_pool` ADD COLUMN `tp_map` INT NOT NULL DEFAULT -1',
    'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'terror_zones_pool'
      AND column_name = 'tp_x');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_pool` ADD COLUMN `tp_x` FLOAT NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'terror_zones_pool'
      AND column_name = 'tp_y');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_pool` ADD COLUMN `tp_y` FLOAT NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'terror_zones_pool'
      AND column_name = 'tp_z');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_pool` ADD COLUMN `tp_z` FLOAT NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'terror_zones_pool'
      AND column_name = 'tp_o');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_pool` ADD COLUMN `tp_o` FLOAT NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

UPDATE `terror_zones_pool`
SET tp_map = 0, tp_x = -8947.64, tp_y = -132.319, tp_z = 83.7199, tp_o = 3.33358
WHERE zone_id = 10;

DELETE FROM `spell_script_names` WHERE `spell_id` = 1206;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1206, 'spell_tz_teleport_tier');
