-- ============================================================
-- Mount Progression — Slice 3 — character_mount_active table
-- ============================================================
-- HAND-CURATED. Persists the player's active carrier-buff mount
-- across logout. On login, if (now - last_active_time) is within
-- MountProgression.OfflineGraceSeconds, the carrier aura is
-- reapplied without requiring a re-cast of the mount. Otherwise
-- the row is cleared and the player must remount.
--
-- One row per character. spell_id is the mount spell currently
-- active (matches mount_progression_catalog.spell_id).
-- ============================================================

CREATE TABLE IF NOT EXISTS `character_mount_active` (
  `guid` INT UNSIGNED NOT NULL,
  `spell_id` INT UNSIGNED NOT NULL,
  `last_active_time` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
