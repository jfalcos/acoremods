-- ============================================================
-- Terror Zones — Slice 7 — hand-curated
-- ============================================================
-- Adds `countdown_fired` to `terror_zones_events` so the per-event
-- "ending in N minutes" zone-scoped warning fires at most once,
-- across worldserver restarts. 0 = not fired, 1 = fired.
--
-- INFORMATION_SCHEMA-guarded dynamic-SQL pattern (see Slice 6
-- worldmap-POI migration for prior art). Idempotent on re-run.
--

SET @col_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'terror_zones_events'
      AND COLUMN_NAME = 'countdown_fired');
SET @sql := IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_events`
        ADD COLUMN `countdown_fired` TINYINT UNSIGNED
        NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
