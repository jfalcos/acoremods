-- ============================================================
-- Mount Progression — Slice 6 — catalog audit: exclusion flag + rebalance
-- ============================================================
-- HAND-CURATED. Full audit of the Common rarity tier (78 rows) found
-- Blizzard internal test/dev spells and temporary quest-loaner mounts
-- mixed into mount_progression_catalog by the original name-based
-- ingestion, plus confirmed Common+Arcane/Common+Mechanical are
-- genuinely thin on real, non-flying, permanently-grantable options
-- (same root cause: both types are mostly high-level flying mounts by
-- nature). Two independent changes:
--
-- 1. New `excluded`/`exclusion_reason` columns, enforced at load time
--    (MountProgressionMgr::LoadCatalog now filters WHERE excluded = 0)
--    rather than left as a documentation-only flag, so no current or
--    future feature (starter picker, `.mount give`, a future random
--    pick, etc.) can select these by accident. Rows are kept (not
--    deleted) so the reasoning survives and a future catalog re-seed
--    doesn't silently reintroduce them without a record of why they
--    were flagged.
--
--    - Dev/test/QA spells (never meant to reach players):
--      26332 Summon Mouth Tentacle, 32420 Old Crappy McWeakSauce,
--      33631 Video Mount, 39949 Mount (Test Anim), 42363/42387 Dan's
--      Steam Tank Form, 42929 [DNT] Test Mount (literal "Do Not Take"
--      dev tag), 45177 Copy of Riding Turtle, 46980 Northrend Nerubian
--      Mount (Test), 61983 Dan's Test Mount.
--    - Event-minigame-only, not a permanent player mount:
--      43883 Rental Racing Ram (Brewfest ram race, temporary).
--    - Unfinished placeholder data (literal "[PH]" dev tag):
--      64992/64993 Big Blizzard Bear [PH].
--    - Quest-loaner mounts, designed to be temporary/quest-bound, risky
--      to grant permanently via learnSpell:
--      60120 Summon Loaner Wind Rider, 64681 Loaned Gryphon,
--      64761 Loaned Wind Rider, 61289 Borrowed Broom.
--
-- 2. Rebalance: Reindeer (25675, 25858, 25859 -- same conceptual Winter
--    Veil holiday mount across 2 display variants) reclassified from
--    stamina to arcane. Ordinary reskinned-speed holiday mount (same
--    tier as any other common ground mount), giving Arcane a 3rd real
--    ground option alongside Skeletal Horse (rev_1782866697421426100)
--    and the already-native Black Warp Stalker (50281, a real Netherstorm
--    ground mount -- flagged for an in-game speed check, not yet
--    verified, same caveat as everything else in this system).
--    Mechanical intentionally left at its single real option (Steel
--    Mechanostrider, 15781) -- no thematically-sane reclassification
--    candidate exists in the clean common stamina/predator pool without
--    a bigger stretch than Skeletal Horse's already-borrowed "dark
--    magic" framing.
-- ============================================================

-- Idempotency guard: this migration's ALTER ran against the live DB
-- previously but the run was never recorded in the `updates` tracking
-- table (state/record drift discovered 2026-07-05 while rebuilding the
-- ac-db-import image for an unrelated change). `ADD COLUMN IF NOT
-- EXISTS` is a MariaDB-only extension -- real MySQL (this deployment
-- runs MySQL 8.4) rejects it as a syntax error -- so the standard
-- information_schema + dynamic-SQL idiom is used instead to make both
-- ADD COLUMN clauses safe to replay.
SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'mount_progression_catalog'
      AND column_name = 'excluded');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `mount_progression_catalog` ADD COLUMN `excluded` TINYINT(1) NOT NULL DEFAULT 0',
    'SELECT 1');
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col_exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'mount_progression_catalog'
      AND column_name = 'exclusion_reason');
SET @ddl = IF(@col_exists = 0,
    'ALTER TABLE `mount_progression_catalog` ADD COLUMN `exclusion_reason` VARCHAR(255) NOT NULL DEFAULT \'\'',
    'SELECT 1');
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

UPDATE `mount_progression_catalog`
SET excluded = 1, exclusion_reason = 'Blizzard internal dev/test spell, not a real player mount'
WHERE spell_id IN (26332, 32420, 33631, 39949, 42363, 42387, 42929, 45177, 46980, 61983);

UPDATE `mount_progression_catalog`
SET excluded = 1, exclusion_reason = 'Event minigame mount (Brewfest ram race), not permanently grantable'
WHERE spell_id = 43883;

UPDATE `mount_progression_catalog`
SET excluded = 1, exclusion_reason = 'Unfinished placeholder catalog entry ("[PH]" dev tag)'
WHERE spell_id IN (64992, 64993);

UPDATE `mount_progression_catalog`
SET excluded = 1, exclusion_reason = 'Quest-loaner mount, designed to be temporary/quest-bound'
WHERE spell_id IN (60120, 64681, 64761, 61289);

UPDATE `mount_progression_catalog`
SET type = 'arcane'
WHERE spell_id IN (25675, 25858, 25859);
