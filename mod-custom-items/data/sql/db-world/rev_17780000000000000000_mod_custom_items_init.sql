-- ============================================================
-- mod-custom-items — init / rename from terror_zones_*
-- ============================================================
-- Creates the module-owned tables `custom_item_template` and
-- `custom_item_display_donors` for the custom-item / display-donor
-- pipeline (extracted from mod-terror-zones Slice 9 so other consumer
-- mods can reuse the framework without a client patch).
--
-- Idempotent. Safe across both fresh installs and live environments
-- that already ran the original mod-terror-zones migrations:
--
--   Path A — live DB with `terror_zones_item_*` tables:
--     RENAME TABLE the existing tables to `custom_item_*`, ALTER ADD
--     the new policy columns, UPDATE all existing rows to the TZ-style
--     flags (strip_sockets/strip_equip_gating/strip_weapon_procs/
--     strip_random_affixes/strip_vendor_fields = 1, force_bonding = 1)
--     so the original loader's hard-coded stripping behavior is
--     preserved exactly on the rows that were seeded under it.
--
--   Path B — fresh DB:
--     `terror_zones_item_*` doesn't exist (or only briefly, courtesy
--     the still-applied historical TZ pending migrations whose CREATE
--     TABLE runs before this one). On the rare path where it does not
--     exist at all, CREATE TABLE LIKE `item_template` for the items
--     table and a minimal two-column donors table.
--
-- The procedural form is required because RENAME TABLE has no
-- "IF EXISTS" clause and ALTER ADD COLUMN errors on duplicate columns.
-- INFORMATION_SCHEMA checks gate each step so re-running the migration
-- (after a hash bump or manual replay) is a no-op.
--
-- Schema notes:
--   * `custom_item_template` is `LIKE item_template` so every column
--     stays in lockstep with the upstream schema. Schema drift surfaces
--     as an ASSERT at boot, not a silent row drop.
--   * Policy columns are TINYINT(1) booleans (0 = inherit donor,
--     1 = apply strip). `force_bonding` is TINYINT UNSIGNED NULL where
--     NULL = inherit donor and 1=BoP, 2=BoU, 3=BoE, 4=Quest override.
--   * Reserved entry-ID window is [700000, 800000); enforced loader-
--     side, not as a CHECK constraint (so test fixtures can ignore it).

DROP PROCEDURE IF EXISTS `_mod_custom_items_init`;

DELIMITER //

CREATE PROCEDURE `_mod_custom_items_init`()
BEGIN
    -- custom_item_template: rename from TZ if present, else create fresh
    IF (SELECT COUNT(*) FROM information_schema.tables
        WHERE table_schema = DATABASE()
          AND table_name = 'custom_item_template') = 0 THEN
        IF (SELECT COUNT(*) FROM information_schema.tables
            WHERE table_schema = DATABASE()
              AND table_name = 'terror_zones_item_template') > 0 THEN
            RENAME TABLE `terror_zones_item_template`
                      TO `custom_item_template`;
        ELSE
            CREATE TABLE `custom_item_template` LIKE `item_template`;
        END IF;
    END IF;

    -- custom_item_display_donors: rename from TZ if present, else create fresh
    IF (SELECT COUNT(*) FROM information_schema.tables
        WHERE table_schema = DATABASE()
          AND table_name = 'custom_item_display_donors') = 0 THEN
        IF (SELECT COUNT(*) FROM information_schema.tables
            WHERE table_schema = DATABASE()
              AND table_name = 'terror_zones_item_display_donors') > 0 THEN
            RENAME TABLE `terror_zones_item_display_donors`
                      TO `custom_item_display_donors`;
        ELSE
            CREATE TABLE `custom_item_display_donors` (
                `custom_entry` INT UNSIGNED NOT NULL,
                `donor_entry`  INT UNSIGNED NOT NULL,
                PRIMARY KEY (`custom_entry`)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
        END IF;
    END IF;

    -- Policy columns. Guard via strip_sockets — if it's already there
    -- the whole block has run; this branch becomes a no-op.
    IF (SELECT COUNT(*) FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'custom_item_template'
          AND column_name = 'strip_sockets') = 0 THEN
        ALTER TABLE `custom_item_template`
            ADD COLUMN `strip_sockets`        TINYINT(1)       NOT NULL DEFAULT 0,
            ADD COLUMN `strip_equip_gating`   TINYINT(1)       NOT NULL DEFAULT 0,
            ADD COLUMN `strip_weapon_procs`   TINYINT(1)       NOT NULL DEFAULT 0,
            ADD COLUMN `strip_random_affixes` TINYINT(1)       NOT NULL DEFAULT 0,
            ADD COLUMN `strip_vendor_fields`  TINYINT(1)       NOT NULL DEFAULT 0,
            ADD COLUMN `force_bonding`        TINYINT UNSIGNED NULL     DEFAULT NULL;

        -- Preserve the original mod-terror-zones loader's behavior on
        -- any rows that came in via the rename. The pre-extraction
        -- loader unconditionally stripped sockets / equip gating /
        -- weapon procs / random affixes / vendor fields and forced
        -- bonding=1; those become explicit per-row flags here so the
        -- new generic loader (which respects the flags) reproduces
        -- the old behavior exactly. Rows inserted by post-extraction
        -- migrations set these columns explicitly and aren't reached
        -- by this UPDATE — it only fires once, gated by the column
        -- existence check above.
        UPDATE `custom_item_template` SET
            `strip_sockets`        = 1,
            `strip_equip_gating`   = 1,
            `strip_weapon_procs`   = 1,
            `strip_random_affixes` = 1,
            `strip_vendor_fields`  = 1,
            `force_bonding`        = 1;
    END IF;
END //

DELIMITER ;

CALL `_mod_custom_items_init`();

DROP PROCEDURE `_mod_custom_items_init`;
