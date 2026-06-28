-- ============================================================
-- Terror Zones — Slice 6 — world-map tracker quests
-- ============================================================
-- HAND-CURATED. A Quest POI is the only way to paint a marker on
-- the 3.3.5 worldmap for an arbitrary location: the client only
-- renders POIs for quests the player has in their quest log.
-- This migration adds:
--   1. 21 hidden dummy quests (IDs 90100-90120), one per
--      empowerable zone with a curated world boss. Each quest's
--      LogTitle is "Terror Zone: <Boss Name>"; the log entry is
--      what the player sees, with a short objective pointing at
--      the map marker.
--   2. 21 `quest_poi` rows pinning a single POI per quest to the
--      boss's anchor zone + map.
--   3. 21 `quest_poi_points` rows holding the (anchor_x, anchor_y)
--      as raw server coordinates (the WoW 3.3.5 client accepts
--      raw continent coords here — cross-validated against native
--      quest 13 "The People's Militia" whose POI points are at
--      Westfall server coords like (-10727, 1659)).
--   4. `terror_zones_event_bosses.tracker_quest_id` column, with
--      each of the 21 curated boss rows updated to point at its
--      matching dummy quest.
--
-- The worldserver auto-grants the tracker quest to every player
-- in an active event's zone (at 1Hz tick via MarkWorldBossForPlayers)
-- and auto-revokes when the player leaves the zone or the event
-- ends. The quest itself is non-completable — it exists purely as
-- a POI carrier, so the player's quest log shows one extra entry
-- while they're in an empowered zone during a boss event.
--

-- ------------------------------------------------------------
-- Quest templates (IDs 90100-90120, reserved above native 26034
-- and above the Mount-Progression range).
-- ------------------------------------------------------------

DELETE FROM `quest_template` WHERE `ID` BETWEEN 90100 AND 90120;

-- Columns: ID, QuestType, QuestLevel, MinLevel, QuestSortID,
--          QuestInfoID, Flags, LogTitle, LogDescription.
-- All other fields default to 0 / '' — non-completable helper row.
INSERT INTO `quest_template`
    (`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`,
     `QuestInfoID`, `Flags`,
     `LogTitle`, `LogDescription`)
VALUES
    (90100, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Foe Reaper 4000',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90101, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Boss Galgosh',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90102, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Fenros',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90103, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Razormaw Matriarch',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90104, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Syndicate Highwayman',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90105, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Terrowulf Packlord',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90106, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Lord Sakrasis',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90107, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Lord Azrethoc',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90108, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Zalas Witherbark',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90109, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Lady Sesspira',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90110, 2, -1, 1, 0, 0, 0,
     'Terror Zone: The Reak',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90111, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Scarlet Judge',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90112, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Ranger Lord Hawkspear',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90113, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Doomsayer Jurim',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90114, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Bog Lurker',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90115, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Morcrush',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90116, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Icehorn',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90117, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Grocklar',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90118, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Scarlet Highlord Daion',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90119, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Seething Hate',
     'A terror-zone world boss has manifested. Follow the map marker.'),
    (90120, 2, -1, 1, 0, 0, 0,
     'Terror Zone: Putridus the Ancient',
     'A terror-zone world boss has manifested. Follow the map marker.');

-- ------------------------------------------------------------
-- Quest POIs — one POI per quest, pinned to the curated anchor.
-- Columns: QuestID, id, ObjectiveIndex, MapID, WorldMapAreaId,
--          Floor, Priority, Flags.
-- WorldMapAreaId values were sampled from native quest_poi rows
-- in each zone, picking the dominant ID (most-used by existing
-- matching-map quests).
-- ------------------------------------------------------------

DELETE FROM `quest_poi` WHERE `QuestID` BETWEEN 90100 AND 90120;

INSERT INTO `quest_poi`
    (`QuestID`, `id`, `ObjectiveIndex`, `MapID`,
     `WorldMapAreaId`, `Floor`, `Priority`, `Flags`) VALUES
    (90100, 0, -1,   0,  39, 0, 0, 0),   -- Westfall
    (90101, 0, -1,   0,  35, 0, 0, 0),   -- Loch Modan
    (90102, 0, -1,   0,  34, 0, 0, 0),   -- Duskwood
    (90103, 0, -1,   0,  40, 0, 0, 0),   -- Wetlands
    (90104, 0, -1,   0,  24, 0, 0, 0),   -- Hillsbrad Foothills
    (90105, 0, -1,   1,  43, 0, 0, 0),   -- Ashenvale
    (90106, 0, -1,   0,  37, 0, 0, 0),   -- Stranglethorn Vale
    (90107, 0, -1,   1, 101, 0, 0, 0),   -- Desolace
    (90108, 0, -1,   0,  16, 0, 0, 0),   -- Arathi Highlands
    (90109, 0, -1,   1, 181, 0, 0, 0),   -- Azshara
    (90110, 0, -1,   0,  26, 0, 0, 0),   -- The Hinterlands
    (90111, 0, -1,   0,  22, 0, 0, 0),   -- Western Plaguelands
    (90112, 0, -1,   0,  23, 0, 0, 0),   -- Eastern Plaguelands
    (90113, 0, -1, 530, 465, 0, 0, 0),   -- Hellfire Peninsula
    (90114, 0, -1, 530, 467, 0, 0, 0),   -- Zangarmarsh
    (90115, 0, -1, 530, 475, 0, 0, 0),   -- Blade's Edge Mountains
    (90116, 0, -1, 571, 486, 0, 0, 0),   -- Borean Tundra
    (90117, 0, -1, 571, 491, 0, 0, 0),   -- Howling Fjord
    (90118, 0, -1, 571, 488, 0, 0, 0),   -- Dragonblight
    (90119, 0, -1, 571, 490, 0, 0, 0),   -- Grizzly Hills
    (90120, 0, -1, 571, 492, 0, 0, 0);   -- Icecrown

-- ------------------------------------------------------------
-- Quest POI points — one point per POI at the boss's anchor.
-- X, Y are RAW SERVER COORDINATES (not zone-normalized; the
-- 3.3.5 client does the normalization using WorldMapArea.dbc).
-- Values cast to int32 (anchor precision ≥1yd is plenty for map
-- marker rendering).
-- ------------------------------------------------------------

DELETE FROM `quest_poi_points` WHERE `QuestID` BETWEEN 90100 AND 90120;

INSERT INTO `quest_poi_points`
    (`QuestID`, `Idx1`, `Idx2`, `X`, `Y`) VALUES
    (90100, 0, 0, -10225,  1448),   -- Foe Reaper 4000, Westfall
    (90101, 0, 0,  -5700, -3871),   -- Boss Galgosh, Loch Modan
    (90102, 0, 0, -10380,  -833),   -- Fenros, Duskwood
    (90103, 0, 0,  -2953, -3281),   -- Razormaw Matriarch, Wetlands
    (90104, 0, 0,   -898, -2026),   -- Syndicate Highwayman, Hillsbrad
    (90105, 0, 0,   3150, -1170),   -- Terrowulf Packlord, Ashenvale
    (90106, 0, 0, -13830,   412),   -- Lord Sakrasis, STV
    (90107, 0, 0,  -1818,  1673),   -- Lord Azrethoc, Desolace
    (90108, 0, 0,  -2044, -3359),   -- Zalas Witherbark, Arathi
    (90109, 0, 0,   3458, -5071),   -- Lady Sesspira, Azshara
    (90110, 0, 0,    359, -3841),   -- The Reak, Hinterlands
    (90111, 0, 0,   2836, -1398),   -- Scarlet Judge, WPL
    (90112, 0, 0,   3322, -4204),   -- Ranger Lord Hawkspear, EPL
    (90113, 0, 0,  -1842,  4231),   -- Doomsayer Jurim, Hellfire
    (90114, 0, 0,   -294,  6951),   -- Bog Lurker, Zangarmarsh
    (90115, 0, 0,   1915,  5150),   -- Morcrush, Blade's Edge
    (90116, 0, 0,   3565,  3635),   -- Icehorn, Borean Tundra
    (90117, 0, 0,   3061, -1840),   -- Grocklar, Howling Fjord
    (90118, 0, 0,   4105, -1132),   -- Scarlet Highlord Daion, Dragonblight
    (90119, 0, 0,   3601, -3275),   -- Seething Hate, Grizzly Hills
    (90120, 0, 0,   6726,  2521);   -- Putridus the Ancient, Icecrown

-- ------------------------------------------------------------
-- Link each boss row to its tracker quest.
-- ------------------------------------------------------------

-- MySQL 8.4 doesn't support ADD COLUMN IF NOT EXISTS; guard via
-- INFORMATION_SCHEMA + dynamic SQL so re-running the file is safe.
SET @col_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME   = 'terror_zones_event_bosses'
      AND COLUMN_NAME  = 'tracker_quest_id'
);
SET @ddl := IF(@col_exists = 0,
    'ALTER TABLE `terror_zones_event_bosses` ADD COLUMN `tracker_quest_id` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `weight`',
    'SELECT 1');
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90100 WHERE `zone_id` = 40;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90101 WHERE `zone_id` = 38;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90102 WHERE `zone_id` = 10;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90103 WHERE `zone_id` = 11;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90104 WHERE `zone_id` = 267;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90105 WHERE `zone_id` = 331;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90106 WHERE `zone_id` = 33;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90107 WHERE `zone_id` = 405;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90108 WHERE `zone_id` = 45;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90109 WHERE `zone_id` = 16;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90110 WHERE `zone_id` = 47;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90111 WHERE `zone_id` = 28;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90112 WHERE `zone_id` = 139;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90113 WHERE `zone_id` = 3483;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90114 WHERE `zone_id` = 3521;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90115 WHERE `zone_id` = 3522;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90116 WHERE `zone_id` = 3537;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90117 WHERE `zone_id` = 495;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90118 WHERE `zone_id` = 65;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90119 WHERE `zone_id` = 394;
UPDATE `terror_zones_event_bosses` SET `tracker_quest_id` = 90120 WHERE `zone_id` = 210;
