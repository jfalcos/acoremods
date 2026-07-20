-- ============================================================
-- mod-paragon revival — world-DB setup
-- ============================================================
-- HAND-CURATED. Strings, milestone reward catalog, the Paragon
-- Coin (a commandeered native item entry so the client renders a
-- real icon — see below), the Quartermaster NPC, and the 4-week
-- rotating cosmetic vendor stock.
-- ============================================================

-- 1) Player-facing strings
DELETE FROM `acore_string` WHERE `entry` BETWEEN 12000 AND 12014 OR `entry` IN (12950, 12951);
INSERT INTO `acore_string` (`entry`,`content_default`) VALUES
  (12000, 'Paragon — |cff00ff00PL {0}|r ({1} / {2} PX to next level).'),
  (12001, 'Paragon — PL {0} | Progress {1} / {2} PX | Lifetime PX {3}'),
  (12002, 'Paragon is currently disabled.'),
  (12003, 'Paragon debug is now: {0}'),
  (12004, 'Paragon config reloaded.'),
  (12011, 'Added {0} PX to your account.'),
  (12012, '|cffffd100Paragon|r — {0} has reached Paragon Level {1}!'),
  (12013, '|cffffd100Paragon|r — Reward for Level {0} delivered to this character.');

-- 2) Milestone reward catalog (cosmetics mailed on crossing a level)
CREATE TABLE IF NOT EXISTS `paragon_rewards` (
  `level`        INT UNSIGNED NOT NULL PRIMARY KEY,
  `mail_subject` VARCHAR(100) NOT NULL,
  `mail_body`    VARCHAR(255) NOT NULL,
  `item_id`      INT UNSIGNED DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `paragon_rewards`;
INSERT INTO `paragon_rewards` (`level`,`mail_subject`,`mail_body`,`item_id`) VALUES
  (1,  'Paragon — Welcome!',   'Your journey begins with a token of appreciation.', NULL),
  (5,  'Paragon Milestone 5',  'Celebrate your dedication!',            6833),
  (10, 'Paragon Milestone 10', 'A rare companion joins your side.',     8491),
  (20, 'Paragon Milestone 20', 'Speed into legend.',                    18768),
  (30, 'Paragon Milestone 30', 'Wear your honor proudly.',              35279),
  (40, 'Paragon Milestone 40', 'Ride into glory.',                      35513),
  (50, 'Paragon Milestone 50', 'A timeless companion for your deeds.',  68833);

-- 3) Paragon Coin — commandeered native entry 37711
--    ("Currency Token Test Token 1"), a Blizzard dev token that is wired
--    end-to-end for the CURRENCY TAB: listed in the client's
--    CurrencyTypes.dbc (id 1, category 1 "Miscellaneous" — a visible
--    category, same as Champion's Seal), BagFamily 8192 server-side, and
--    has ZERO acquisition paths and zero owned instances (verified
--    2026-07-19). No client patch needed. (43949 was tried first but its
--    CurrencyTypes.dbc row carries a corrupt category id and the client
--    cannot display it; it is restored verbatim below.) The old custom
--    entry 80000 (red question mark) stays retired, and 34518 (Golden Pig
--    Coin) — used briefly during development — is restored verbatim too.
DELETE FROM `item_template` WHERE `entry` = 80000;
UPDATE `item_template` SET
  `name` = 'Paragon Coin',
  `Quality` = 4,
  `bonding` = 1,           -- bind on pickup
  `BuyPrice` = 0,
  `SellPrice` = 0,
  `ItemLevel` = 1,
  `RequiredLevel` = 0,
  `AllowableClass` = -1,
  `AllowableRace` = -1,
  `stackable` = 2147483647,
  `description` = 'Earned by gaining Paragon levels. Spend at the Paragon Quartermaster.'
WHERE `entry` = 37711;

-- Restore 43949 to its pristine base-DB row.
DELETE FROM `item_template` WHERE `entry` = 43949;
INSERT INTO `item_template` VALUES
(43949,10,0,-1,'zzzOLDDaily Quest Faction Token',41435,3,2048,0,1,0,0,0,262143,32767,80,80,0,0,0,0,0,0,0,0,2147483647,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,1,'',0,0,0,0,0,-1,0,0,0,0,0,0,0,0,8192,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,'',0,0,0,0,0,12340);

-- Restore Golden Pig Coin (34518) to its pristine base-DB row.
DELETE FROM `item_template` WHERE `entry` = 34518;
INSERT INTO `item_template` VALUES
(34518,15,2,-1,'Golden Pig Coin',47816,3,64,0,1,0,0,0,-1,-1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,55884,0,-1,0,-1,0,-1,45174,6,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,0,0,0,-1,0,-1,0,'Teaches you how to summon this companion.',0,0,0,0,0,-1,0,0,0,0,0,0,0,0,4096,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,'',0,0,0,0,0,11159);

-- 4) Quartermaster NPC (entry 96000)
DELETE FROM `creature_template` WHERE `entry` = 96000;
INSERT INTO `creature_template` (
  `entry`, `name`, `subname`, `IconName`, `npcflag`,
  `gossip_menu_id`, `minlevel`, `maxlevel`, `ScriptName`, `faction`
) VALUES (
  96000, 'Paragon Quartermaster', 'Curator of Prestige', 'Speak',
  1, 0, 80, 80, 'Paragon_QM_NPC', 35
);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 96000;
INSERT INTO `creature_template_model`
  (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 96000, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`
FROM `creature_template_model` WHERE `CreatureID` = 19;

-- 5) Weekly vendor stock — FOUR-WEEK ROTATION CYCLE (week_id 0-3, looked up
--    as absolute-week % 4), replacing the old absolute-epoch weeks that
--    silently expired after one month.
CREATE TABLE IF NOT EXISTS `paragon_vendor_stock` (
  `week_id` SMALLINT UNSIGNED NOT NULL,
  `slot`    TINYINT UNSIGNED  NOT NULL,
  `item`    INT UNSIGNED      NOT NULL,
  `cost`    TINYINT UNSIGNED  NOT NULL,
  PRIMARY KEY (`week_id`,`slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `paragon_vendor_stock`;
INSERT INTO `paragon_vendor_stock` (`week_id`,`slot`,`item`,`cost`) VALUES
  (0,1,25472,20),(0,2,8629,20),(0,3,44998,10),(0,4,45606,10),
  (0,5,22114,10),(0,6,40653,10),(0,7,49662,10),(0,8,13379,8),
  (1,1,25470,20),(1,2,13321,20),(1,3,45022,10),(1,4,44738,10),
  (1,5,46892,10),(1,6,44980,10),(1,7,38050,10),(1,8,49646,10),
  (2,1,34060,20),(2,2,46100,20),(2,3,34425,10),(2,4,34478,10),
  (2,5,25535,10),(2,6,44984,10),(2,7,22780,10),(2,8,44841,10),
  (3,1,21324,20),(3,2,8592,20),(3,3,49912,10),(3,4,33993,10),
  (3,5,41133,10),(3,6,49663,10),(3,7,46767,10),(3,8,49693,10);
