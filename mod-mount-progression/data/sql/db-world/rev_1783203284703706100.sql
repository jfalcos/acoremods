-- ============================================================
-- Mount Progression — per-starting-zone starter quest rows.
-- ============================================================
-- Follow-up to rev_...706800 (QuestSortID = -22 "Seasonal" generic fix).
-- -22 cleared "Missing Header" but reads oddly (Seasonal) for a permanent,
-- non-holiday quest. Since QuestSortID is one static value per quest row,
-- and this quest is granted at 8 different starting zones via a single ID,
-- no single value can read correctly for every race.
--
-- Fix: give each starting zone its own quest_template row (900000 + a
-- fixed per-race offset, see StarterQuestOffsetForRace in
-- MountProgressionMgr.cpp), each with that zone's real QuestSortID, so
-- every race sees their own actual starting zone as the header --
-- matching how real Blizzard starting-zone quests work. Zone IDs
-- resolved by cross-referencing each Mount Tamer spawn's exact
-- coordinates against real nearby quest-givers' QuestSortID (filtering
-- out class-training quests, which use their own separate category IDs):
--   900000 Human      (Northshire Abbey)   zone 9
--   900001 Orc/Troll   (Valley of Trials)   zone 363
--   900002 Dwarf/Gnome (Coldridge Valley)   zone 132
--   900003 Night Elf   (Shadowglen)         zone 188
--   900004 Undead      (Tirisfal Glades)    zone 267
--   900005 Tauren      (Mulgore)            zone 220
--   900006 Blood Elf   (Sunstrider Isle)    zone 3431
--   900007 Draenei     (Azuremyst Isle)     zone 3524
-- Text is identical across all 8 -- only ID and QuestSortID differ.
-- ============================================================

UPDATE `quest_template` SET `QuestSortID` = 9 WHERE `ID` = 900000;

DELETE FROM `quest_template` WHERE `ID` BETWEEN 900001 AND 900007;
INSERT INTO `quest_template`
    (`ID`, `QuestLevel`, `MinLevel`, `QuestSortID`, `LogTitle`, `LogDescription`,
     `QuestDescription`, `ObjectiveText1`, `AllowableRaces`)
VALUES
    (900001, 1, 1, 363,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900002, 1, 1, 132,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900003, 1, 1, 188,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900004, 1, 1, 267,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900005, 1, 1, 220,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900006, 1, 1, 3431,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0),
    (900007, 1, 1, 3524,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0);
