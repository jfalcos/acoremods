-- ============================================================
-- Mount Progression — Slice 4 fix — Mount Tamer model + Northshire coords
-- ============================================================
-- HAND-CURATED. Follow-up fix for rev_1782863667381922900.sql:
--
-- 1. entry 900000 had no row in `creature_template_model`. In this
--    schema version creature_template no longer carries model IDs
--    directly (that moved to creature_template_model) -- without a row
--    there, ObjectMgr::LoadCreatureTemplates rejects the whole template
--    ("Creature (Entry: 900000) has no model defined ... can't load"),
--    so none of the 8 spawns rendered. Fixed by adding a model row,
--    reusing CreatureDisplayID 24436 (already used by the live "Stable
--    Master Mercer" entries 27236/28555) -- same single-universal-
--    display convention mod-transmog's npc_transmogrifier already uses
--    for its own faction-neutral NPC (see trasm_world_NPC.sql).
--
-- 2. The Human spawn was anchored on "Marshal Marris", who is actually
--    a Redridge Mountains/Lakeshire NPC, not Northshire Abbey -- wrong
--    zone entirely. Re-anchored on the real Northshire Abbey cluster
--    (Marshal McBride / Deputy Willem / Brother Paxton, all ~(-8900,
--    -150, 82)), live-queried to confirm.
-- ============================================================

DELETE FROM `creature_template_model` WHERE `CreatureID` = 900000;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (900000, 0, 24436, 1, 1);

DELETE FROM `creature` WHERE `id` = 900000;
INSERT INTO `creature`
    (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`,
     `position_y`, `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES
    -- Human -- Northshire Abbey (near Marshal McBride, map 0) -- FIXED,
    -- was wrongly anchored on Lakeshire's Marshal Marris
    (900000, 0, 0, 0, 1, 1, -8899.6, -159.6, 82.0, 0, 300, 0),
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
    -- map 1; STILL LOWER CONFIDENCE -- verify in-game)
    (900000, 1, 0, 0, 1, 1, -3732.1, -4553.2, 27.2, 0, 300, 0);
