-- ============================================================
-- Property Override System — item rows gain a `source` namespace
-- ============================================================
-- HAND-CURATED. Mirrors player_property_override's design: `source`
-- namespaces which system owns the row ('gm', 'paragon', 'mix', ...)
-- so systems can budget/clear their own rows without touching each
-- other's. The same property from different sources stacks additively.
--
-- Existing rows predate the column and default to 'paragon': at the
-- time of this migration the only shipped writer of persistent item
-- rows was the paragon item-upgrade purchase path (GM `.propover add`
-- test rows also existed; misclassifying those as 'paragon' only
-- affects budget accounting on test items and is accepted).
-- ============================================================

ALTER TABLE `item_property_override`
  ADD COLUMN `source` VARCHAR(16) NOT NULL DEFAULT 'paragon' AFTER `owner_guid`,
  DROP PRIMARY KEY,
  ADD PRIMARY KEY (`item_guid`, `source`, `property`);
