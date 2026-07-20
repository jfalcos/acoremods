-- ============================================================
-- Property Override System — slice 2 — player_property_override
-- ============================================================
-- HAND-CURATED. Per-character stat overrides for the
-- mod-property-override module: rows targeting a PLAYER instead of
-- an item instance. `source` namespaces which system owns the row
-- ('gm', 'mount', 'aa', ...) so systems can clear their own rows
-- without touching each other's; the same property from different
-- sources stacks additively. Rows are purged transactionally with
-- character deletion (PlayerScript::OnPlayerDeleteFromDB).
-- ============================================================

CREATE TABLE IF NOT EXISTS `player_property_override` (
  `player_guid` INT UNSIGNED NOT NULL,
  `source`      VARCHAR(16)  NOT NULL,
  `property`    TINYINT UNSIGNED NOT NULL,
  `value`       INT NOT NULL,
  `expiry`      BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`player_guid`, `source`, `property`),
  INDEX `idx_guid` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
