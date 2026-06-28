-- ============================================================
-- Terror Zones — Slice 1 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned table; does NOT touch any
-- AC-native table. Loaded at worldserver boot by
-- mod-terror-zones's OnStartup hook. GM-tunable: flip
-- `enabled` to 0 to pull a zone out of rotation, or edit
-- `level_min`/`level_max` to reshape weighting — both take
-- effect on next worldserver restart (no hot-reload in
-- Slice 1; see SLICE_1_PLAN §8).
--
-- Coverage: ~20 open-world zones spanning levels 10-80, with
-- ≥3 zones per spec-defined band (10-20 / 20-30 / 30-45 /
-- 45-60 / 60-68 / 68-80). Expand during playtest. Zone IDs
-- are `AreaTable.dbc` IDs (the same values used by
-- `Player::GetZoneId()`), not map IDs.
--

CREATE TABLE IF NOT EXISTS `terror_zones_pool` (
  `zone_id`      INT UNSIGNED NOT NULL,
  `display_name` VARCHAR(128) NOT NULL,
  `level_min`    SMALLINT UNSIGNED NOT NULL,
  `level_max`    SMALLINT UNSIGNED NOT NULL,
  `enabled`      TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`zone_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `terror_zones_pool`;

INSERT INTO `terror_zones_pool`
    (`zone_id`, `display_name`,               `level_min`, `level_max`, `enabled`) VALUES
    -- 10-20
    (40,        'Westfall',                   10,          20,          1),
    (38,        'Loch Modan',                 10,          20,          1),
    (10,        'Duskwood',                   18,          30,          1),
    -- 20-30
    (11,        'Wetlands',                   20,          30,          1),
    (267,       'Hillsbrad Foothills',        20,          30,          1),
    (331,       'Ashenvale',                  18,          30,          1),
    -- 30-45
    (33,        'Stranglethorn Vale',         30,          45,          1),
    (405,       'Desolace',                   30,          40,          1),
    (45,        'Arathi Highlands',           30,          40,          1),
    (400,       'Thousand Needles',           25,          35,          1),
    -- 45-60
    (16,        'Azshara',                    45,          55,          1),
    (47,        'The Hinterlands',            40,          50,          1),
    (28,        'Western Plaguelands',        51,          58,          1),
    (139,       'Eastern Plaguelands',        53,          60,          1),
    -- 60-68
    (3483,      'Hellfire Peninsula',         60,          63,          1),
    (3521,      'Zangarmarsh',                60,          64,          1),
    (3522,      'Blade''s Edge Mountains',    65,          68,          1),
    -- 68-80
    (3537,      'Borean Tundra',              68,          72,          1),
    (495,       'Howling Fjord',              68,          72,          1),
    (65,        'Dragonblight',               71,          74,          1),
    (394,       'Grizzly Hills',              73,          75,          1),
    (210,       'Icecrown',                   77,          80,          1);
