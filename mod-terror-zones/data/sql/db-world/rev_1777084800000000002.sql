-- ============================================================
-- Terror Zones — Slice 6 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned curated pool of world-boss event
-- definitions. Loaded at worldserver boot by
-- TerrorZonesMgr::LoadEventContent and consulted by
-- ScheduleEvents when an EVENT_WORLD_BOSS event fires in an
-- empowered zone. Each row pins a named elite to its OWN
-- natural spawn coordinate (queried from the creature table so
-- we land on solid ground, not in open water); scheduling picks
-- a row whose zone + level band contains the rotation's scaled
-- mob level, weighted by `weight`. Despawn is driven by
-- TempSummon's duration argument.
--
-- Pass-1 curation: one iconic named elite per empowerable zone
-- (21 of 22 — Thousand Needles intentionally node-surge-only
-- for this pass). Every creature_template_id is verified to
-- exist in the DB, AND every anchor coord is the creature's
-- real natural spawn location — so teleporting to the anchor
-- lands the GM at the expected place in the zone.
--
-- Loot comes from the creature's native `creature_loot_template`
-- plus the TerrorZones reward chain (Slice 3 tier-bump, Slice 5
-- unique drops). Per-event-boss bonus loot is a Pass 2 layer.
--

CREATE TABLE IF NOT EXISTS `terror_zones_event_bosses` (
  `id`                   INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `zone_id`              INT UNSIGNED NOT NULL,
  `level_min`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `level_max`            TINYINT UNSIGNED NOT NULL DEFAULT 80,
  `creature_template_id` INT UNSIGNED NOT NULL,
  `anchor_map`           INT UNSIGNED NOT NULL,
  `anchor_x`             FLOAT NOT NULL,
  `anchor_y`             FLOAT NOT NULL,
  `anchor_z`             FLOAT NOT NULL,
  `anchor_o`             FLOAT NOT NULL DEFAULT 0,
  `display_name`         VARCHAR(96) NOT NULL DEFAULT '',
  `weight`               INT UNSIGNED NOT NULL DEFAULT 100,
  `enabled`              TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  INDEX `idx_zone` (`zone_id`),
  INDEX `idx_zone_band` (`zone_id`, `level_min`, `level_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `terror_zones_event_bosses`;

-- Columns: zone_id, lvl_min, lvl_max, creature_template_id,
--          map, x, y, z, o, display_name, weight, enabled.
-- Each anchor is the creature's real natural spawn.
INSERT INTO `terror_zones_event_bosses`
    (`zone_id`, `level_min`, `level_max`, `creature_template_id`,
     `anchor_map`, `anchor_x`, `anchor_y`, `anchor_z`, `anchor_o`,
     `display_name`, `weight`, `enabled`) VALUES
    -- Low band (10-30) — Eastern Kingdoms + Kalimdor
    (40,   10, 22, 573,    0, -10225.6, 1448.1,   40.8,  4.24,
     'Foe Reaper 4000',          100, 1),   -- Westfall, Harvest Reaper village
    (38,   10, 25, 1398,   0, -5700.3,  -3871.3,  331.6, 4.64,
     'Boss Galgosh',             100, 1),   -- Loch Modan, Stonesplinter troggs
    (10,   18, 32, 507,    0, -10379.9, -833.2,   44.5,  3.33,
     'Fenros',                   100, 1),   -- Duskwood, dread wolf elite
    (11,   20, 32, 1140,   0, -2953.2,  -3281.3,  62.4,  1.97,
     'Razormaw Matriarch',       100, 1),   -- Wetlands, raptor cave matriarch
    (267,  20, 32, 2586,   0, -898.1,   -2026.4,  34.5,  4.44,
     'Syndicate Highwayman',     100, 1),   -- Hillsbrad/Alterac foothills
    (331,  18, 32, 3792,   1, 3150.4,   -1170.1,  217.3, 5.14,
     'Terrowulf Packlord',       100, 1),   -- Ashenvale, Warsong logging
    -- Mid band (30-45)
    (33,   32, 47, 2541,   0, -13830,   412.8,    87.0,  2.15,
     'Lord Sakrasis',            100, 1),   -- STV, Ruins of Aboraz naga
    (405,  30, 42, 5760,   1, -1818.6,  1673.2,   62.0,  2.86,
     'Lord Azrethoc',            100, 1),   -- Desolace, Magram ogre camp
    (45,   32, 42, 2605,   0, -2044.9,  -3359,    60.8,  0.44,
     'Zalas Witherbark',         100, 1),   -- Arathi, Witherbark troll village
    (16,   42, 55, 6649,   1, 3458.5,   -5071.9,  84.8,  4.53,
     'Lady Sesspira',            100, 1),   -- Azshara, naga elite
    -- High band (45-60)
    (47,   42, 52, 8212,   0, 359.9,    -3841.9,  107.1, 2.21,
     'The Reak',                 100, 1),   -- Hinterlands, Wildhammer cliffs
    (28,   51, 60, 1837,   0, 2836.4,   -1398.3,  147.5, 3.78,
     'Scarlet Judge',            100, 1),   -- WPL, Scarlet Crusade tribunal
    (139,  53, 62, 10824,  0, 3322.9,   -4204.4,  159.2, 4.53,
     'Ranger Lord Hawkspear',    100, 1),   -- EPL, troll elite
    -- Outland (60-70)
    (3483, 58, 66, 18686,  530, -1842.5, 4231.0,  21.1,  2.23,
     'Doomsayer Jurim',          100, 1),   -- Hellfire, Fel Reaver ruins
    (3521, 58, 66, 18682,  530, -294.9,  6951.4,  19.4,  5.87,
     'Bog Lurker',               100, 1),   -- Zangarmarsh, Serpent Lake
    (3522, 62, 70, 18690,  530, 1915.7,  5150.2,  265.7, 1.08,
     'Morcrush',                 100, 1),   -- Blade''s Edge, ogre king
    -- Northrend apex (68-80)
    (3537, 68, 74, 32361,  571, 3565.3,  3635.4,  36.4,  1.9,
     'Icehorn',                  100, 1),   -- Borean Tundra, mammoth rare
    (495,  68, 76, 32422,  571, 3061.4,  -1840.0, 66.0,  6.07,
     'Grocklar',                 100, 1),   -- Howling Fjord, ogre rare
    (65,   70, 76, 32417,  571, 4105.5,  -1132.1, 134.3, 0.95,
     'Scarlet Highlord Daion',   100, 1),   -- Dragonblight, Scarlet Onslaught
    (394,  72, 77, 32429,  571, 3601.2,  -3275.5, 222.1, 4.61,
     'Seething Hate',            100, 1),   -- Grizzly Hills, elemental rare
    (210,  75, 82, 32487,  571, 6726.5,  2521.5,  428.2, 0.13,
     'Putridus the Ancient',     100, 1);   -- Icecrown, abomination rare
