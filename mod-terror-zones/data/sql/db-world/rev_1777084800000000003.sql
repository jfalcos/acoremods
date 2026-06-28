-- ============================================================
-- Terror Zones — Slice 6 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned curated pool of rare-node-surge
-- event definitions. Loaded at worldserver boot by
-- TerrorZonesMgr::LoadEventContent and consulted by
-- ScheduleEvents when an EVENT_RARE_NODE_SURGE event fires in
-- an empowered zone. Each row names a cluster anchor + radius;
-- when the event fires, `node_count` extra nodes drawn from
-- `node_gameobject_ids` (CSV of gameobject_template entry IDs)
-- spawn uniformly within the disc. All nodes despawn at event
-- end; ungathered ones vanish, gathered ones were already
-- spent.
--
-- Schema:
--   node_gameobject_ids — CSV of GO template entry IDs. Module
--     parses the CSV at load-time and validates entries against
--     gameobject_template; bad entries are dropped with a warn
--     log line. At least one valid ID required per row.
--   radius — disc radius in yards around the anchor. Defaults
--     to TerrorZones.Events.NodeSurge.DefaultRadius when 0.
--   node_count — how many nodes spawn. Defaults to
--     TerrorZones.Events.NodeSurge.DefaultNodeCount when 0.
--
-- Node IDs reference standard WotLK-era gathering-node
-- gameobjects: peacebloom (1617), silverleaf (1618), mageroyal
-- (1620), copper deposit (1731), tin deposit (1732), iron
-- deposit (1735), mithril deposit (2040), thorium vein (324),
-- khorium vein (181555), felsteel deposit (181556), goldthorn
-- (1621), sungrass (142141), golden sansam (142142), cobalt
-- deposit (189978), saronite deposit (189979), titanium vein
-- (191133), talandra's rose (191019), goldclover (189973),
-- adder's tongue (191303), icethorn (190172). Real WotLK
-- gameobject_template entries.
--

CREATE TABLE IF NOT EXISTS `terror_zones_event_node_surges` (
  `id`                 INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `zone_id`            INT UNSIGNED NOT NULL,
  `level_min`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `level_max`          TINYINT UNSIGNED NOT NULL DEFAULT 80,
  `anchor_map`         INT UNSIGNED NOT NULL,
  `anchor_x`           FLOAT NOT NULL,
  `anchor_y`           FLOAT NOT NULL,
  `anchor_z`           FLOAT NOT NULL,
  `radius`             FLOAT NOT NULL DEFAULT 0,
  `node_gameobject_ids` VARCHAR(255) NOT NULL DEFAULT '',
  `node_count`         INT UNSIGNED NOT NULL DEFAULT 0,
  `display_name`       VARCHAR(96) NOT NULL DEFAULT '',
  `weight`             INT UNSIGNED NOT NULL DEFAULT 100,
  `enabled`            TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  INDEX `idx_zone` (`zone_id`),
  INDEX `idx_zone_band` (`zone_id`, `level_min`, `level_max`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `terror_zones_event_node_surges`;

-- Pass-1 curation. ~10 clusters across the empowerable pool,
-- each placed in a recognizable natural feature (grove,
-- mountain pass, quarry) so the anchor reads as "a real place"
-- rather than random coordinates.
INSERT INTO `terror_zones_event_node_surges`
    (`zone_id`, `level_min`, `level_max`, `anchor_map`,
     `anchor_x`, `anchor_y`, `anchor_z`, `radius`,
     `node_gameobject_ids`, `node_count`, `display_name`,
     `weight`, `enabled`) VALUES
    -- Low band (10-30)
    (40,   10, 20, 0, -10826.0, 1550.0,  41.0,  50.0,
     '1617,1618,1620,1731', 8,
     'the verdant dales',       100, 1),   -- Westfall, wheat fields
    (38,   10, 20, 0, -6177.0,  -3352.0, 253.0, 50.0,
     '1617,1618,1731',      8,
     'the Loch''s southern ridges', 100, 1),   -- Loch Modan ridgeline
    (10,   18, 28, 0, -10378.0, -1149.0, 32.0,  40.0,
     '1618,1620,1731,1732', 8,
     'the Twilight Grove',      100, 1),   -- Duskwood
    (11,   20, 30, 0, -3898.0,  -2820.0, 11.0,  60.0,
     '1620,1621,1732',      8,
     'the Wetlands marshes',    100, 1),   -- Wetlands
    (267,  20, 30, 0, -1045.0,  -3270.0, 77.0,  50.0,
     '1620,1621,1732,1735', 8,
     'the Southshore vineyards', 100, 1),   -- Hillsbrad
    (331,  20, 30, 1, 2350.0,   -1555.0, 95.0,  60.0,
     '1620,1621,1732,1618', 8,
     'the Warsong timber camps', 100, 1),   -- Ashenvale
    -- Mid band (30-45)
    (33,   30, 45, 0, -12230.0, 209.0,   16.0,  50.0,
     '1621,1735,1620',      8,
     'the Stranglethorn canopy', 100, 1),   -- STV
    (405,  30, 40, 1, -1225.0,  2050.0,  78.0,  60.0,
     '2041,1732,1735',      8,
     'the Kolkar plains',       100, 1),   -- Desolace
    (45,   30, 40, 0, -1855.0,  -2994.0, 89.0,  50.0,
     '1735,2040,1732',      8,
     'the Boulderfist ridges',  100, 1),   -- Arathi
    (400,  25, 35, 1, -4740.0,  -1790.0, 20.0,  60.0,
     '1735,2040,1732',      8,
     'the Thousand Needles mesas', 100, 1),   -- Thousand Needles
    -- High band (45-60)
    (47,   40, 50, 0, 56.0,     -3638.0, 122.0, 60.0,
     '2040,1732,1735,142141', 8,
     'the Hinterlands pine forest', 100, 1),   -- Hinterlands
    (16,   45, 55, 1, 3040.0,   -4690.0, 104.0, 60.0,
     '2040,1735,142141,142142', 8,
     'the Azshara highlands',   100, 1),   -- Azshara
    (139,  53, 60, 0, 2310.0,   -5380.0, 92.0,  60.0,
     '2040,324,142141',     8,
     'the Plaguewood margins',  100, 1),   -- EPL
    (28,   51, 58, 0, 2017.0,   -2008.0, 62.0,  50.0,
     '2040,324,142141',     8,
     'Dalson''s orchard',       100, 1),   -- WPL
    -- Apex band (60-80)
    (3483, 60, 64, 530, -267.0, 2061.0,  96.0,  60.0,
     '181555,181556,142142', 8,
     'the Hellfire glades',     100, 1),   -- Hellfire Peninsula
    (3521, 60, 64, 530, 350.0,  6900.0,  40.0,  60.0,
     '181555,142141,142142', 8,
     'the Zangarmarsh roots',   100, 1),   -- Zangarmarsh
    (3522, 65, 68, 530, 2550.0, 5950.0,  110.0, 60.0,
     '181555,181556',       8,
     'the Blade''s Edge quarry', 100, 1),   -- Blade's Edge
    (3537, 68, 72, 571, 3400.0, 5050.0,  1.0,   60.0,
     '189978,189973,191019',8,
     'the Tundra bluffs',       100, 1),   -- Borean Tundra
    (495,  68, 72, 571, 1850.0, -4820.0, 85.0,  60.0,
     '189978,189973,191019',8,
     'the Howling coast',       100, 1),   -- Howling Fjord
    (65,   71, 74, 571, 3650.0, 280.0,   50.0,  60.0,
     '189979,189973,191303',8,
     'the Dragonblight ossuary', 100, 1),   -- Dragonblight
    (394,  73, 75, 571, 3830.0, -3420.0, 240.0, 60.0,
     '189979,189973,191303',8,
     'the Grizzlemaw hollows',  100, 1),   -- Grizzly Hills
    (210,  77, 80, 571, 5660.0, 2075.0,  440.0, 60.0,
     '191133,190172,191303',8,
     'the Broken Front scar',   100, 1);   -- Icecrown
