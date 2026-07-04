-- ============================================================
-- Terror Zones — Slice 10 Pass 2 — per-TZ contract progress
-- ============================================================
-- HAND-CURATED. Module-owned runtime-mutable state in
-- `acore_characters`. Written exclusively by mod-terror-zones.
--
-- `character_terror_zones_progress` — one row per
--     (character, rotation tick, empowered zone). Credit accrues
--     write-through on each eligible kill in that zone during the
--     rotation; when the rotation rotates off, RunRotation mails a
--     scaled reward (gold + an optional archetype gear piece) to the
--     character (works offline — the row is the source of truth) and
--     deletes the settled rows. The captured class / spec / level /
--     tier let the mail-out resolve the gear entry without the player
--     online. `mailed` guards against a double send if a row survives.
--

CREATE TABLE IF NOT EXISTS `character_terror_zones_progress` (
  `guid`         INT UNSIGNED NOT NULL,
  `tick_at`      BIGINT UNSIGNED NOT NULL,
  `zone_id`      INT UNSIGNED NOT NULL,
  `credit`       INT UNSIGNED NOT NULL DEFAULT 0,
  `player_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `player_class` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `spec_index`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `tier`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `mailed`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `tick_at`, `zone_id`),
  INDEX `idx_tick_mailed` (`tick_at`, `mailed`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
