-- ============================================================
-- Mount Progression — Slice 5 fix — quest-ender map marker
-- ============================================================
-- HAND-CURATED. Quest 900000 ("Choose Your Companion") was showing in
-- the sidebar tracker (client-side, driven purely by the player's quest
-- log) but had no yellow "?" turn-in marker on the map, because the
-- client only renders that from two DB-driven links that a plain
-- Player::AddQuest() grant never populates:
--
--   1. `creature_questender` -- tells the client which creature
--      completes a given quest (drives the head-icon "?" + the
--      supertrack arrow once nearby). Added ONLY questender, not
--      queststarter -- this quest is auto-granted via mail, not
--      accepted from the NPC, so a "!" (new quest available) icon
--      would be misleading since clicking the NPC opens our custom
--      mount-choice gossip, not a quest-accept dialog.
--   2. `quest_poi` + `quest_poi_points` -- drives the marker on the
--      zoomed-out/continent-level big map (in-zone tracking works off
--      `creature_questender` + live position alone, but the full World
--      Map screen needs an explicit POI blob). One point-blob per
--      Mount Tamer spawn (8 total, one per starting zone/map), each
--      `ObjectiveIndex = -1` (the "quest ender" marker convention,
--      confirmed against 10175 existing rows using it, e.g. quest 6's
--      own Northshire-area ender POI). WorldMapAreaId per zone was
--      resolved by cross-referencing existing quest_poi_points
--      clustered near each Mount Tamer spawn's coordinates (not
--      guessed): Northshire=30, Coldridge=27, Shadowglen=41,
--      Valley of Trials=4, Mulgore=9, Tirisfal=24, Ghostlands=462,
--      Azuremyst=464. X/Y are the Mount Tamer's own live spawn
--      coordinates (confirmed raw-world-coordinate convention by
--      matching quest 6/7's Northshire ender POI, X=-8934/Y=-137,
--      against the real nearby NPC positions -- same coordinate space
--      as `creature.position_x/position_y`, not a separate transform).
-- ============================================================

DELETE FROM `creature_questender` WHERE `id` = 900000 AND `quest` = 900000;
INSERT INTO `creature_questender` (`id`, `quest`) VALUES (900000, 900000);

DELETE FROM `quest_poi` WHERE `QuestID` = 900000;
DELETE FROM `quest_poi_points` WHERE `QuestID` = 900000;

INSERT INTO `quest_poi`
    (`QuestID`, `id`, `ObjectiveIndex`, `MapID`, `WorldMapAreaId`, `Floor`, `Priority`, `Flags`)
VALUES
    (900000, 0, -1, 0,   30,  0, 0, 1),  -- Northshire Abbey (Human)
    (900000, 1, -1, 0,   27,  0, 0, 1),  -- Coldridge Valley (Dwarf/Gnome)
    (900000, 2, -1, 1,   41,  0, 0, 1),  -- Shadowglen (Night Elf)
    (900000, 3, -1, 1,   4,   0, 0, 1),  -- Valley of Trials (Orc/Troll)
    (900000, 4, -1, 1,   9,   0, 0, 1),  -- Camp Narache (Tauren)
    (900000, 5, -1, 0,   24,  0, 0, 1),  -- Deathknell (Undead)
    (900000, 6, -1, 530, 462, 0, 0, 1),  -- Sunstrider Isle (Blood Elf)
    (900000, 7, -1, 530, 464, 0, 0, 1);  -- Ammen Vale (Draenei)

INSERT INTO `quest_poi_points`
    (`QuestID`, `Idx1`, `Idx2`, `X`, `Y`)
VALUES
    (900000, 0, 0, -8944,  -136),
    (900000, 1, 0, -6359,  563),
    (900000, 2, 0, 10317,  833),
    (900000, 3, 0, -613,   -4251),
    (900000, 4, 0, -2910,  -261),
    (900000, 5, 0, -17,    -980),
    (900000, 6, 0, 10355,  -6357),
    (900000, 7, 0, -4269,  -13227);
