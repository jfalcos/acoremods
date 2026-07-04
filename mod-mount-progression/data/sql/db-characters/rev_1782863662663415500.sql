-- ============================================================
-- Mount Progression — Slice 4 — character_mount_starter_choice table
-- ============================================================
-- HAND-CURATED. One row per character: presence of the row means the
-- character has already made their one-time starter mount choice via
-- npc_mount_tamer. spell_id/chosen_at are kept for support/audit, not
-- strictly required by the gate logic (existence check is enough).
-- ============================================================

CREATE TABLE IF NOT EXISTS `character_mount_starter_choice` (
    `guid` INT UNSIGNED NOT NULL,
    `spell_id` INT UNSIGNED NOT NULL,
    `chosen_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
