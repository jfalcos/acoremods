-- ============================================================
-- mod-paragon revival — characters-DB schema
-- ============================================================
-- HAND-CURATED. Account-wide paragon XP pool + per-character
-- reward-delivery ledger + per-character perk ranks.
--
-- `paragon_account_progress.season*` columns are dormant (reserved
-- for a future seasons feature; nothing reads them yet).
-- Perk STATS are applied via mod-property-override rows (source
-- 'paragon'); paragon_perk_ranks is the rank source of truth for
-- display and cost math.
-- ============================================================

CREATE TABLE IF NOT EXISTS `paragon_account_progress` (
  `account`               INT UNSIGNED     NOT NULL,
  `lifetime_px`           BIGINT UNSIGNED  NOT NULL DEFAULT 0,
  `season_px`             BIGINT UNSIGNED  NOT NULL DEFAULT 0,
  `last_reward_level`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `paragon_opt_out`       TINYINT(1)       NOT NULL DEFAULT 0,
  `xp_allocation_percent` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `season`                SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `updated_at`            INT UNSIGNED     NOT NULL DEFAULT 0,
  `created_at`            INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `paragon_char_delivery` (
  `guid`  INT UNSIGNED NOT NULL,
  `level` INT UNSIGNED NOT NULL,
  `ts`    INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `paragon_perk_ranks` (
  `guid`     INT UNSIGNED     NOT NULL,
  `property` TINYINT UNSIGNED NOT NULL,
  `ranks`    SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `property`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
