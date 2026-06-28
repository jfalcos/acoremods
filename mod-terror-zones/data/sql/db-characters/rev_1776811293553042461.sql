-- ============================================================
-- Terror Zones — Slice 1 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned tables in `acore_characters`
-- (runtime-mutable state; `acore_world` is content a fresh
-- import rebuilds, which is wrong for rotation history).
-- Both tables are written/read exclusively by
-- mod-terror-zones; no AC-native table is touched.
--
-- `terror_zones_history` — one row per zone per rotation
--     tick. Composite PK is `(tick_at, slot_index)` so a
--     multi-slot rotation records one row per slot sharing
--     the same tick timestamp. Kept forever in Slice 1 (the
--     table stays tiny — 1 row / slot / hour ≈ 9 K rows /
--     year at `SlotCount = 1`). A slice-local pruning job
--     can trim if that ever matters.
--
-- `character_terror_zones_prefs` — per-character toggle for
--     rotation-tick and zone-entry chat lines. Loaded on
--     OnPlayerLogin, written on change. Default-on.
--

CREATE TABLE IF NOT EXISTS `terror_zones_history` (
  `tick_at`    BIGINT UNSIGNED NOT NULL,
  `slot_index` TINYINT UNSIGNED NOT NULL,
  `zone_id`    INT UNSIGNED NOT NULL,
  PRIMARY KEY (`tick_at`, `slot_index`),
  INDEX `idx_tick` (`tick_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_terror_zones_prefs` (
  `guid`             INT UNSIGNED NOT NULL,
  `announce_enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
