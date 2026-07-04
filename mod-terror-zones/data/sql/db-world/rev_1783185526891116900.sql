-- ============================================================
-- Terror Zones — boss tracker quest QuestSortID fix.
-- ============================================================
-- The 21 tracker quests (90100-90120) are granted directly via
-- Player::AddQuest with no live questgiver object -- unlike a normal
-- quest, the client has no NPC/GO to derive a quest-log group header
-- from. Combined with QuestSortID left at 0, the client showed the
-- literal debug placeholder "Missing Header! (Quest designers...)" as
-- the group header, and the Details/Objectives pane stayed blank.
--
-- Fix: QuestSortID = the tracker's own zone_id (from
-- terror_zones_event_bosses). Each tracker quest is already anchored
-- to exactly one real zone, and a zone's numeric Area ID doubles
-- directly as a valid QuestSortID -- confirmed against existing native
-- quests already using each of these 21 values (e.g. zone 495 =
-- "Shoveltusk Soup Again?" in Howling Fjord, zone 400 = "Goblins Win!"
-- in Thousand Needles) before picking them.
-- ============================================================

UPDATE `quest_template` SET `QuestSortID` = 40 WHERE `ID` = 90100; -- Foe Reaper 4000 (zone 40)
UPDATE `quest_template` SET `QuestSortID` = 38 WHERE `ID` = 90101; -- Boss Galgosh (zone 38)
UPDATE `quest_template` SET `QuestSortID` = 10 WHERE `ID` = 90102; -- Fenros (zone 10)
UPDATE `quest_template` SET `QuestSortID` = 11 WHERE `ID` = 90103; -- Razormaw Matriarch (zone 11)
UPDATE `quest_template` SET `QuestSortID` = 267 WHERE `ID` = 90104; -- Syndicate Highwayman (zone 267)
UPDATE `quest_template` SET `QuestSortID` = 331 WHERE `ID` = 90105; -- Terrowulf Packlord (zone 331)
UPDATE `quest_template` SET `QuestSortID` = 33 WHERE `ID` = 90106; -- Lord Sakrasis (zone 33)
UPDATE `quest_template` SET `QuestSortID` = 405 WHERE `ID` = 90107; -- Lord Azrethoc (zone 405)
UPDATE `quest_template` SET `QuestSortID` = 45 WHERE `ID` = 90108; -- Zalas Witherbark (zone 45)
UPDATE `quest_template` SET `QuestSortID` = 16 WHERE `ID` = 90109; -- Lady Sesspira (zone 16)
UPDATE `quest_template` SET `QuestSortID` = 47 WHERE `ID` = 90110; -- The Reak (zone 47)
UPDATE `quest_template` SET `QuestSortID` = 28 WHERE `ID` = 90111; -- Scarlet Judge (zone 28)
UPDATE `quest_template` SET `QuestSortID` = 139 WHERE `ID` = 90112; -- Ranger Lord Hawkspear (zone 139)
UPDATE `quest_template` SET `QuestSortID` = 3483 WHERE `ID` = 90113; -- Doomsayer Jurim (zone 3483)
UPDATE `quest_template` SET `QuestSortID` = 3521 WHERE `ID` = 90114; -- Bog Lurker (zone 3521)
UPDATE `quest_template` SET `QuestSortID` = 3522 WHERE `ID` = 90115; -- Morcrush (zone 3522)
UPDATE `quest_template` SET `QuestSortID` = 3537 WHERE `ID` = 90116; -- Icehorn (zone 3537)
UPDATE `quest_template` SET `QuestSortID` = 495 WHERE `ID` = 90117; -- Grocklar (zone 495)
UPDATE `quest_template` SET `QuestSortID` = 65 WHERE `ID` = 90118; -- Scarlet Highlord Daion (zone 65)
UPDATE `quest_template` SET `QuestSortID` = 394 WHERE `ID` = 90119; -- Seething Hate (zone 394)
UPDATE `quest_template` SET `QuestSortID` = 210 WHERE `ID` = 90120; -- Putridus the Ancient (zone 210)
