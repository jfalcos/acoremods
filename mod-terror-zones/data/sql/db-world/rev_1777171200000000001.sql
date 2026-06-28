-- ============================================================
-- Terror Zones — Slice 8 — hand-curated
-- ============================================================
-- HAND-CURATED. Event-boss bonus-loot pool keyed by level band.
-- Loaded at startup by TerrorZonesMgr::LoadEventBossLootPool() and
-- consulted by TryEventBossDrop() when a terror-zone event boss is
-- killed. Drops are ADDITIVE — they do not substitute for the boss's
-- native loot template.
--
-- Schema:
--   level_min / level_max    — inclusive band boundaries. First
--                              matching band wins at roll time
--                              (no overlap expected in Pass 1).
--   guaranteed_blue_item_id  — always drops on boss kill.
--   purple_item_id           — rolled at purple_chance. 0 = no
--                              purple roll for this band.
--   purple_chance            — 0..1 probability. Global
--                              TerrorZones.Events.WorldBoss.LootPool.
--                              PurpleMultiplier dials this live.
--   gold_min_copper / max    — gold-chest range added to loot.gold.
--                              In copper — 10000 = 1g.
--   enabled                  — 0 = pulled without deleting the row.
--
-- Pass 1 curation note: the guaranteed_blue slot currently uses
-- trade-goods from the Slice 4 unique-drops pool that are known-good
-- in AC's default install. These are PLACEHOLDERS so the mechanic
-- works end-to-end; Pass-2 curation replaces them with ilvl-
-- appropriate BoE blues from native loot tables once in-game
-- validation picks representative items per band. Purple slot left
-- at 0 across all bands pending the same Pass-2 pass.

CREATE TABLE IF NOT EXISTS `terror_zones_event_bosses_loot_pool` (
    `id`                      INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `level_min`               TINYINT UNSIGNED NOT NULL,
    `level_max`               TINYINT UNSIGNED NOT NULL,
    `guaranteed_blue_item_id` INT UNSIGNED NOT NULL,
    `purple_item_id`          INT UNSIGNED NOT NULL DEFAULT 0,
    `purple_chance`           FLOAT NOT NULL DEFAULT 0.20,
    `gold_min_copper`         INT UNSIGNED NOT NULL DEFAULT 0,
    `gold_max_copper`         INT UNSIGNED NOT NULL DEFAULT 0,
    `enabled`                 TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    INDEX `idx_band` (`level_min`, `level_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `terror_zones_event_bosses_loot_pool`;

INSERT INTO `terror_zones_event_bosses_loot_pool`
    (`level_min`, `level_max`, `guaranteed_blue_item_id`,
     `purple_item_id`, `purple_chance`,
     `gold_min_copper`, `gold_max_copper`, `enabled`) VALUES
    -- Band 10-19 — early-leveling zones (Elwynn/Durotar/DK start).
    -- Placeholder blue = 4306 Linen Cloth stack, gold 5s-20s.
    (10, 19, 4306,  0, 0.20,    500,   2000, 1),
    -- Band 20-29 — Redridge/Barrens tier.
    -- Placeholder blue = 4338 Wool Cloth.
    (20, 29, 4338,  0, 0.20,   2000,   8000, 1),
    -- Band 30-39 — Duskwood/Stonetalon tier.
    -- Placeholder blue = 2770 Copper Ore stack (substitute to stronger
    -- trade good once Pass 2 curates a real BoE blue).
    (30, 39, 2770,  0, 0.20,   5000,  20000, 1),
    -- Band 40-49 — Arathi/STV tier.
    -- Placeholder blue = 2771 Tin Ore.
    (40, 49, 2771,  0, 0.20,  15000,  50000, 1),
    -- Band 50-59 — Felwood/Ungoro/Blasted Lands tier.
    -- Placeholder blue = 14047 Runecloth.
    (50, 59, 14047, 0, 0.25,  30000, 100000, 1),
    -- Band 60-69 — Hellfire/Zangarmarsh/Outland early.
    -- Placeholder blue = 14047 Runecloth (still relevant).
    (60, 69, 14047, 0, 0.25,  50000, 200000, 1),
    -- Band 70-79 — Borean Tundra/Howling Fjord/Dragonblight tier.
    -- Placeholder blue = 33470 Frostweave Cloth.
    (70, 79, 33470, 0, 0.25, 100000, 400000, 1),
    -- Band 80 — cap. Storm Peaks / Icecrown tier.
    -- Placeholder blue = 33470 Frostweave Cloth.
    (80, 83, 33470, 0, 0.25, 200000, 800000, 1);
