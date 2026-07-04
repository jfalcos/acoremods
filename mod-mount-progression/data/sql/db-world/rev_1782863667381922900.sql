-- ============================================================
-- Mount Progression — Slice 4 — Starter mount NPC ("Mount Tamer")
-- ============================================================
-- HAND-CURATED. New creature_template + spawns near every level-1
-- starting zone lacking a native Stable Master. Live-DB query during
-- planning confirmed entries 23392/27236/28555/28683 ("Stable Master")
-- only exist on map 530 (Outland), 571 (Northrend), and 609 (Ebon
-- Hold/Acherus) -- none on map 0 (Eastern Kingdoms) or map 1
-- (Kalimdor), i.e. none in any original level-1 zone. Uses ONLY
-- UNIT_NPC_FLAG_GOSSIP (1) -- never UNIT_NPC_FLAG_STABLEMASTER
-- (0x00400000) -- and ScriptName npc_mount_tamer (registered in
-- MountStarterNpcScript.cpp), so this NPC never opens the native
-- hunter pet-stable UI.
--
-- NOTE: entry 900000 confirmed free against the live acore_world DB
-- during planning. Spawn coordinates are anchored a few yards from a
-- real reference NPC already spawned in each starting zone (innkeeper
-- or quest giver), queried live, so each spot is guaranteed walkable
-- ground rather than a guessed coordinate -- still flag for a live
-- `.npc add 900000` / `.go xyz` walk-test before considering final,
-- particularly the Draenei (Ammen Vale) spawn, anchored on a lower-
-- confidence reference NPC match.
--
-- Death Knight start (Ebon Hold/Acherus, map 609) intentionally has no
-- spawn here -- Stable Masters Mercer/Kitrik already exist there.
-- ============================================================

DELETE FROM `creature_template` WHERE `entry` = 900000;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `dmgschool`,
     `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`,
     `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`,
     `lootid`, `pickpocketloot`, `skinloot`, `VehicleId`, `mingold`, `maxgold`,
     `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`,
     `ArmorModifier`, `DamageModifier`, `ExperienceModifier`, `RacialLeader`,
     `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`)
VALUES
    (900000, 'Mount Tamer', 'Mount Progression', '', 900000, 1, 1,
     35, 1, 1, 1.14286, 0, 0,
     2000, 2000, 1, 1, 1,
     0, 2048, 0, 0, 7, 0,
     0, 0, 0, 0, 0, 0,
     '', 0, 1, 1, 1,
     1, 1, 1, 0,
     0, 1, 0, 0, 'npc_mount_tamer');

-- Spawns: one per level-1 starting zone lacking a native Stable Master.
-- Coordinates are offset a few yards from a real, live-verified
-- reference NPC per zone (innkeeper/quest giver) so the ground is
-- known-walkable. zoneId/areaId left 0 to match this DB's existing
-- creature rows (these fields are server-recomputed, not authoritative
-- here -- see plan verification notes).
DELETE FROM `creature` WHERE `id` = 900000;
INSERT INTO `creature`
    (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`,
     `position_y`, `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES
    -- Human -- Northshire Abbey (near Marshal Marris, map 0)
    (900000, 0, 0, 0, 1, 1, -9281.0, -2295.0, 67.6, 0, 300, 0),
    -- Dwarf/Gnome -- Coldridge Valley (near Innkeeper Allison, map 0)
    (900000, 0, 0, 0, 1, 1, -8864.8, 670.7, 98.0, 0, 300, 0),
    -- Night Elf -- Shadowglen, Teldrassil (near Conservator Ilthalaine, map 1)
    (900000, 1, 0, 0, 1, 1, 10317.0, 832.8, 1326.5, 0, 300, 0),
    -- Orc/Troll -- Valley of Trials, Durotar (near Kaltunk, map 1)
    (900000, 1, 0, 0, 1, 1, -613.1, -4250.5, 39.0, 0, 300, 0),
    -- Tauren -- Camp Narache, Mulgore (near Grull Hawkwind, map 1)
    (900000, 1, 0, 0, 1, 1, -2909.7, -260.5, 53.0, 0, 300, 0),
    -- Undead -- Deathknell, Tirisfal Glades (near Novice Thaivand, map 0)
    (900000, 0, 0, 0, 1, 1, -16.5, -980.4, 55.9, 0, 300, 0),
    -- Blood Elf -- Sunstrider Isle, Ghostlands (near Magistrix Erona, map 530)
    (900000, 530, 0, 0, 1, 1, 10355.0, -6356.9, 34.1, 0, 300, 0),
    -- Draenei -- Ammen Vale, Azuremyst Isle (near Captain Garran Vimes,
    -- map 1; LOWER CONFIDENCE -- verify this is actually Ammen Vale
    -- before ship, see note above)
    (900000, 1, 0, 0, 1, 1, -3732.1, -4553.2, 27.2, 0, 300, 0);

DELETE FROM `npc_text` WHERE `ID` = 900000;
INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES (900000, 'Every rider needs a first companion. Choose your bond -- it will grow with you.');

DELETE FROM `gossip_menu` WHERE `MenuID` = 900000;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (900000, 900000);
