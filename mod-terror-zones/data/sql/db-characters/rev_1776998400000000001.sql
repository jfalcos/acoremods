-- ============================================================
-- Terror Zones — Slice 5 — hand-curated
-- ============================================================
-- HAND-CURATED. Adds the `tier` column to `terror_zones_history`.
-- Each rotation slot now rolls both a flavor (Slice 4) AND a tier
-- (Slice 5, 1-5). Pre-Slice-5 rows keep the default 0 (TIER_NONE)
-- and are treated as Tier 1 for reward math at read-time; they
-- display as "Tier 1" in `.zones history`. See
-- TERROR_ZONES_SPEC.md v0.2 §4.2 + §4.4 for the math and
-- SLICE_5_PLAN.md §4.5 for the compatibility contract.
--
-- NOTE: MySQL 8.4 does not support `ADD COLUMN IF NOT EXISTS`.
-- AzerothCore's update system tracks applied migrations by file
-- name hash in the `updates` table, so a re-run of the same file
-- is a no-op regardless — no need for idempotency guards here.
--

ALTER TABLE `terror_zones_history`
    ADD COLUMN `tier` TINYINT UNSIGNED NOT NULL DEFAULT 0
    AFTER `flavor`;
