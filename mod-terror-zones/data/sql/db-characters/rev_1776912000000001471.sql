-- ============================================================
-- Terror Zones — Slice 4 — hand-curated
-- ============================================================
-- HAND-CURATED. Adds the `flavor` column to `terror_zones_history`
-- so each rotation slot is tagged with one of five Flavor enum
-- values (see TerrorZonesMgr.h — Bloodbath=1, Prospectors=2,
-- Warlords=3, Arcane=4, Merchants=5). Pre-Slice-4 rows keep the
-- default 0 (FLAVOR_NONE) and are displayed as "—" in the
-- .zones history command.
--

ALTER TABLE `terror_zones_history`
    ADD COLUMN `flavor` TINYINT UNSIGNED NOT NULL DEFAULT 0
    AFTER `zone_id`;
