-- ============================================================
-- Mount Progression — Slice 2 — character_mount_progress table
-- ============================================================
-- HAND-CURATED. Per-character per-mount XP/level state for the
-- mod-mount-progression module. One row per (character, mount spell)
-- that the character has ever ridden / earned XP on.
--
-- Active-mount tracking lives in memory only for Slice 2. It is
-- re-established on first mount cast after login. Buff persistence
-- (spec §5) lands with Slice 3 and may add a sibling table for the
-- 30-minute offline grace.
-- ============================================================

CREATE TABLE IF NOT EXISTS `character_mount_progress` (
  `guid` INT UNSIGNED NOT NULL,
  `spell_id` INT UNSIGNED NOT NULL,
  `mount_level` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  `mount_xp` INT UNSIGNED NOT NULL DEFAULT 0,
  `updated_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `spell_id`),
  INDEX `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
