-- ============================================================
-- mod-paragon — fix paragon_char_delivery.guid column type
-- ============================================================
-- The table pre-existed from the original third-party mod-paragon with
-- `guid BIGINT UNSIGNED`. The revival's CREATE TABLE IF NOT EXISTS
-- (rev_...0002_paragon_core.sql) declares `guid INT UNSIGNED` to match
-- characters.guid, but silently no-opped against the pre-existing table.
-- Table was empty at the time of this fix, so a plain MODIFY is safe.
-- ============================================================

ALTER TABLE `paragon_char_delivery`
    MODIFY COLUMN `guid` INT UNSIGNED NOT NULL;
