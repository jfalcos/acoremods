-- ============================================================
-- Mount Progression — Slice 1 — Rarity enrichment
-- ============================================================
-- Auto-generated (SQL, not Python). Hand-edit is fine since this file has
-- no external generator — but keep it auto-gen-flavored:
-- rules-based UPDATE only, no spell-id-specific overrides (those go in a
-- separate hand-curated migration).
--
-- Strategy: name-match mount_progression_catalog.display_name (spell name)
-- against item_template.name (item name) across the 5 common item-name
-- patterns in 3.3.5a. Pulls rarity from item_template.Quality.
--
-- Coverage baseline (2026-04-20): ~286/396 spells matched. Unmatched mounts
-- stay at 'common' default — correct for racial trainer mounts (majority of
-- unmatched). Class-quest mounts (DK/paladin/warlock) need hand override.
-- ============================================================

-- Fix collation mismatch: the catalog was created with MySQL 8.4's default
-- utf8mb4_0900_ai_ci, but acore_world tables use utf8mb4_unicode_ci.
-- Normalize to AC convention so JOINs work without explicit COLLATE.
ALTER TABLE `mount_progression_catalog`
  CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Rarity from item Quality, via 5 name-match patterns.
UPDATE `mount_progression_catalog` c
JOIN `item_template` i
  ON i.class = 15 AND i.subclass = 5
 AND (
      i.name = c.display_name
   OR i.name = CONCAT('Reins of the ', c.display_name)
   OR i.name = CONCAT(c.display_name, "'s Reins")
   OR i.name = CONCAT('Horn of the ', c.display_name)
   OR i.name = CONCAT('Whistle of the ', c.display_name)
 )
SET c.rarity = CASE i.Quality
  WHEN 5 THEN 'legendary'
  WHEN 4 THEN 'epic'
  WHEN 3 THEN 'rare'
  WHEN 2 THEN 'uncommon'
  ELSE 'common'
END;
