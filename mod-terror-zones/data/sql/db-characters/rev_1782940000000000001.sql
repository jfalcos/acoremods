-- ============================================================
-- Terror Zones — Teleport-spell unlock — lifetime tier progress
-- ============================================================
-- HAND-CURATED. Module-owned runtime-mutable state in `acore_characters`.
-- Written exclusively by mod-terror-zones.
--
-- `character_terror_zones_tier_progress` — one row per (character, tier
--     1-5). Unlike `character_terror_zones_progress` (rotation-scoped,
--     deleted once mailed), this is a lifetime, never-reset counter: any
--     credit earned fighting in ANY zone while it happens to be
--     empowered at that tier feeds the same bucket. Once
--     `lifetime_credit` crosses TerrorZones.Teleport.UnlockThreshold.T<n>,
--     `unlocked` is set and the character permanently keeps the Tier-n
--     teleport spell (TerrorZones.Teleport.Spell.T<n>).

CREATE TABLE IF NOT EXISTS `character_terror_zones_tier_progress` (
  `guid`            INT UNSIGNED NOT NULL,
  `tier`            TINYINT UNSIGNED NOT NULL,
  `lifetime_credit` INT UNSIGNED NOT NULL DEFAULT 0,
  `unlocked`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
