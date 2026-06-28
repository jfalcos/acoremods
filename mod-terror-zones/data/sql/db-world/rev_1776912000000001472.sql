-- ============================================================
-- Terror Zones — Slice 4 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned curation table of empowered-only
-- unique drops per spec §4.3. Loaded by the module's
-- BuildUniqueDropIndex() at OnStartup and consulted by
-- TryUniqueDrop() on every loot bundle in an empowered zone.
-- Drops are ADDITIVE — they do not substitute for normal loot.
--
-- Schema:
--   flavor    — 0 (wildcard; any active flavor) or 1..5 per
--               the Flavor enum in TerrorZonesMgr.h.
--   level_min — minimum scaled mob level to qualify for this
--               item. Defaults to 0 (any).
--   level_max — maximum scaled mob level. Defaults to 80.
--   weight    — relative weight within the flavor's pool.
--   enabled   — tinyint gate. 0 = pulled from rotation without
--               deleting the row (reversible tuning).
--
-- Curation is a two-pass process (see SLICE_4_PLAN §8.2):
--   Pass 1 (this file) — skeleton + safe rows for Prospector's
--     and Merchant's using items from the AH bot allow-list.
--     Bloodbath / Warlord's / Arcane pools left empty because
--     curation there needs in-game review and the flavors each
--     already deliver via their overlay (XP boost / tier-bump
--     add / XP boost respectively) — an empty unique pool means
--     TryUniqueDrop returns early, nothing drops, no regression.
--   Pass 2 — expand curation after first in-game verification
--     pass per SLICE_4_PLAN §11.4 step 7.
--
-- Anti-pressure: these items must be cosmetic / reinforcing,
-- never mandatory BiS gear. See spec §8.
--

CREATE TABLE IF NOT EXISTS `terror_zones_unique_drops` (
  `id`        INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `item_id`   INT UNSIGNED NOT NULL,
  `flavor`    TINYINT UNSIGNED NOT NULL,
  `level_min` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `level_max` SMALLINT UNSIGNED NOT NULL DEFAULT 80,
  `weight`    INT UNSIGNED NOT NULL DEFAULT 100,
  `enabled`   TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  INDEX `idx_flavor` (`flavor`),
  INDEX `idx_item`   (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `terror_zones_unique_drops`;

INSERT INTO `terror_zones_unique_drops`
    (`item_id`, `flavor`, `level_min`, `level_max`, `weight`, `enabled`) VALUES
    -- Prospector's (2) — gathering-adjacent reagents reinforcing the
    -- professions-matter intent. All items are known-good trade goods
    -- from the AH bot allow-list in docker-compose.override.yml.
    (2447,  2,  1,  20, 100, 1),   -- Peacebloom (herb, low-level)
    (785,   2,  3,  25, 100, 1),   -- Mageroyal (herb, low-mid)
    (2453,  2, 10,  30, 100, 1),   -- Bruiseweed (herb, mid)
    (2770,  2,  1,  25, 100, 1),   -- Copper Ore (mining, low)
    (2771,  2, 15,  35, 100, 1),   -- Tin Ore (mining, mid)
    (2772,  2, 20,  40, 100, 1),   -- Iron Ore (mining, mid-high)
    (14047, 2, 50,  80, 100, 1),   -- Runecloth (cloth, late-game)
    (33470, 2, 68,  80, 100, 1),   -- Frostweave Cloth (cloth, WotLK)
    -- Merchant's (5) — high-vendor-value bulk trade goods
    -- reinforcing the gold-zone intent.
    (14047, 5, 40,  80, 100, 1),   -- Runecloth
    (33470, 5, 68,  80, 100, 1),   -- Frostweave Cloth
    (4306,  5,  1,  25, 100, 1),   -- Linen Cloth
    (4338,  5, 10,  30, 100, 1),   -- Wool Cloth
    (10938, 5, 15,  40, 100, 1);   -- Lesser Magic Essence

-- Bloodbath (1), Warlord's (3), Arcane (4) — Pass 1 pools
-- intentionally empty. Curation happens in-game (Pass 2).
