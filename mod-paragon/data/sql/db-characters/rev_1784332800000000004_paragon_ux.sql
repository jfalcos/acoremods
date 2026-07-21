-- mod-paragon UX pass: one-time handbook mail ledger (per account).
ALTER TABLE `paragon_account_progress`
  ADD COLUMN `handbook_sent` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `xp_allocation_percent`;
