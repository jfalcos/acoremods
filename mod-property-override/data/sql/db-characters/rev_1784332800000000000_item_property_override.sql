-- ============================================================
-- Property Override System — prototype slice — item_property_override
-- ============================================================
-- HAND-CURATED. Per-item-instance stat overrides for the
-- mod-property-override module (shared foundation for AA, item
-- mixing, item upgrades — see acoremods PROPOSALS.md).
--
-- One row per (item instance, property). `item_guid` is
-- item_instance.guid (stable across sessions). `owner_guid` is
-- write-time bookkeeping only — the login load joins item_instance
-- for live ownership, so trades/mails self-heal at the receiver's
-- next login.
-- ============================================================

CREATE TABLE IF NOT EXISTS `item_property_override` (
  `item_guid`  INT UNSIGNED NOT NULL,
  `owner_guid` INT UNSIGNED NOT NULL,
  `property`   TINYINT UNSIGNED NOT NULL,
  `value`      INT NOT NULL,
  `expiry`     BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_guid`, `property`),
  INDEX `idx_owner` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
