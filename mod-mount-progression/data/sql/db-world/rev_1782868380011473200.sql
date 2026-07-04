-- ============================================================
-- Mount Progression — Slice 4 fix 4 — Coldridge Valley zone + Ammen Vale terrain fix
-- ============================================================
-- HAND-CURATED. Two more corrections from live user testing:
--
-- 1. The Dwarf/Gnome spawn was anchored on an "Innkeeper Allison" at
--    (-8864.8, 670.7, 98.0) -- user confirmed that's actually
--    Stormwind, not Coldridge Valley (low elevation gave it away in
--    hindsight: real Coldridge Valley sits at Z~380-430, a mountain
--    valley, not Z~98). Re-anchored on "Grelin Whitebeard" (map 0,
--    live-queried), cross-checked against a tight cluster of 17+
--    "Coldridge Mountaineer" spawns (name literally contains the zone)
--    all within the same X~-6100..-6400 / Y~100..600 / Z~380..430
--    envelope, so this one is now zone-confirmed by more than a single
--    name match (the mistake pattern from the last two fixes).
--
-- 2. The Draenei spawn (already zone-corrected to Ammen Vale/map 530 in
--    the previous fix) fell through the terrain -- the small ~3-4 yard
--    offset applied from the "Ammen Vale Guardian" anchor crossed a
--    terrain discontinuity (Ammen Vale has cliffs/water nearby). Fixed
--    by using the anchor's exact coordinates with zero offset, since
--    that guarantees a known-good position (an existing live spawn
--    can't be sitting in bad terrain).
-- ============================================================

DELETE FROM `creature` WHERE `id` = 900000;
INSERT INTO `creature`
    (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`,
     `position_y`, `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES
    -- Human -- Northshire Abbey entrance (near Deputy Willem, map 0)
    (900000, 0, 0, 0, 1, 1, -8944.0, -136.0, 83.7, 3.33358, 300, 0),
    -- Dwarf/Gnome -- Coldridge Valley (near Grelin Whitebeard, map 0)
    -- FIXED, was wrongly anchored in Stormwind
    (900000, 0, 0, 0, 1, 1, -6359.0, 563.0, 385.9, 0, 300, 0),
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
    -- Draenei -- Ammen Vale, Azuremyst Isle (exact position of Ammen
    -- Vale Guardian, map 530 -- FIXED, zero offset to avoid terrain fall)
    (900000, 530, 0, 0, 1, 1, -4268.94, -13227.2, 64.5769, 0, 300, 0);
