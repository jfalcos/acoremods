-- ============================================================
-- Mount Progression — Slice 4 fix 3 — Northshire entrance + Azuremyst zone fix
-- ============================================================
-- HAND-CURATED. Two coordinate corrections from live user testing:
--
-- 1. The Draenei spawn was anchored on "Captain Garran Vimes", which
--    the user confirmed is actually in Theramore Isle (map 1), not
--    Azuremyst Isle -- wrong zone entirely, same mistake pattern as the
--    earlier Marshal Marris/Lakeshire mix-up. Re-anchored on "Ammen
--    Vale Guardian" (map 530, live-queried), which is unambiguously
--    inside Ammen Vale itself. Also corrects the map id: Azuremyst
--    Isle is on map 530 in this DB (shares the id with Outland/
--    Ghostlands, same pattern already seen for the Blood Elf spawn),
--    not map 1 as originally assumed.
--
-- 2. Human spawn moved to the entrance of Northshire Abbey, close to
--    Deputy Willem (map 0, live-queried), per user request.
-- ============================================================

DELETE FROM `creature` WHERE `id` = 900000;
INSERT INTO `creature`
    (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`,
     `position_y`, `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES
    -- Human -- Northshire Abbey entrance (near Deputy Willem, map 0)
    (900000, 0, 0, 0, 1, 1, -8944.0, -136.0, 83.7, 3.33358, 300, 0),
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
    -- Draenei -- Ammen Vale, Azuremyst Isle (near Ammen Vale Guardian,
    -- map 530 -- FIXED, was wrongly anchored on Theramore Isle)
    (900000, 530, 0, 0, 1, 1, -4271.9, -13224.2, 64.6, 0, 300, 0);
