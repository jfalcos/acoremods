-- ============================================================
-- Mount Progression — Slice 5 — character_mount_starter_quest_sent table
-- ============================================================
-- HAND-CURATED. One row per character: presence of the row means the
-- one-time starter mail+quest nudge has already been sent, so
-- MaybeSendStarterQuest doesn't re-send it on every subsequent login.
-- Separate from character_mount_starter_choice (Slice 4) since a
-- character can have been mailed the quest but not yet made their
-- choice (the normal in-between state).
-- ============================================================

CREATE TABLE IF NOT EXISTS `character_mount_starter_quest_sent` (
    `guid` INT UNSIGNED NOT NULL,
    `sent_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
