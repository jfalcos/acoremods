-- ============================================================
-- Terror Zones — Slice 7 — hand-curated
-- ============================================================
-- Adds the per-category announcement bitmask to
-- `character_terror_zones_prefs`. 0xFF default = every category
-- enabled. The per-player master switch (`announce_enabled`) is
-- unchanged; the bitmask is only consulted when master is on.
--
-- MySQL 8.4 does NOT accept `ADD COLUMN IF NOT EXISTS` (MariaDB
-- only), so the migration uses an INFORMATION_SCHEMA-guarded
-- dynamic-SQL pattern. Idempotent on re-run, which matters
-- because pending_db_*/ files stay PENDING in the schema updater
-- (applied manually via mysql for now, lands normally when the
-- file moves out of pending at merge time).
--

SET @col_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_terror_zones_prefs'
      AND COLUMN_NAME = 'announce_categories');
SET @sql := IF(@col_exists = 0,
    'ALTER TABLE `character_terror_zones_prefs`
        ADD COLUMN `announce_categories` TINYINT UNSIGNED
        NOT NULL DEFAULT 255',
    'SELECT 1');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
