-- ============================================================
-- Terror Zones — Teleport landing points for the remaining 20
-- of 22 pool zones (Duskwood and Arathi Highlands were already
-- set in rev_1782940000000000002/007).
-- ============================================================
-- Coordinates are graveyard/resurrection points pulled from
-- graveyard_zone joined to game_graveyard for each zone_id — the
-- same verification method used to fix the earlier wrong Arathi
-- Highlands landing point (Comment column confirms the real zone
-- name, far more reliable than guessing off NPC names). Each pick
-- favors a safe, well-known hub for that zone.
-- ============================================================

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -2953.30, `tp_y` = -1758.10, `tp_z` = 9.41316, `tp_o` = 0
    WHERE `zone_id` = 11; -- Wetlands, Sundown Marsh

UPDATE `terror_zones_pool` SET `tp_map` = 1, `tp_x` = 2681.06, `tp_y` = -4009.75, `tp_z` = 107.849, `tp_o` = 0
    WHERE `zone_id` = 16; -- Azshara, Talrendis Point

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = 908.323, `tp_y` = -1520.29, `tp_z` = 55.0372, `tp_o` = 0
    WHERE `zone_id` = 28; -- Western Plaguelands, Chillwind Camp

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -14285, `tp_y` = 288.447, `tp_z` = 32.332, `tp_o` = 0
    WHERE `zone_id` = 33; -- Stranglethorn Vale, Booty Bay

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -5351.23, `tp_y` = -2881.58, `tp_z` = 340.942, `tp_o` = 0
    WHERE `zone_id` = 38; -- Loch Modan, Thelsamar

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -10546.9, `tp_y` = 1197.24, `tp_z` = 31.7263, `tp_o` = 0
    WHERE `zone_id` = 40; -- Westfall, Sentinel Hill

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = 323.513, `tp_y` = -2227.2, `tp_z` = 137.617, `tp_o` = 0
    WHERE `zone_id` = 47; -- The Hinterlands, Aerie Peak

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 3538.17, `tp_y` = 275.51, `tp_z` = 45.6119, `tp_o` = 0
    WHERE `zone_id` = 65; -- Dragonblight, Wyrmrest GY

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = 2208.66, `tp_y` = -2923.06, `tp_z` = 107.93, `tp_o` = 0
    WHERE `zone_id` = 139; -- Eastern Plaguelands, West GY (Light's Hope Chapel)

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 6070.45, `tp_y` = 85.9704, `tp_z` = 369.616, `tp_o` = 0
    WHERE `zone_id` = 210; -- Icecrown, Argent Vanguard

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -577.414, `tp_y` = 118.942, `tp_z` = 53.746, `tp_o` = 0
    WHERE `zone_id` = 267; -- Hillsbrad Foothills, Hillsbrad Fields

UPDATE `terror_zones_pool` SET `tp_map` = 1, `tp_x` = 2633.41, `tp_y` = -629.735, `tp_z` = 107.581, `tp_o` = 0
    WHERE `zone_id` = 331; -- Ashenvale, Astranaar

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 4323.39, `tp_y` = -3606.85, `tp_z` = 248, `tp_o` = 0
    WHERE `zone_id` = 394; -- Grizzly Hills, Central GY

UPDATE `terror_zones_pool` SET `tp_map` = 1, `tp_x` = -5361.55, `tp_y` = -2363.09, `tp_z` = -37.392, `tp_o` = 0
    WHERE `zone_id` = 400; -- Thousand Needles, Freewind Post

UPDATE `terror_zones_pool` SET `tp_map` = 1, `tp_x` = -1439.21, `tp_y` = 1972.74, `tp_z` = 85.7449, `tp_o` = 0
    WHERE `zone_id` = 405; -- Desolace, Ghost Walker Post

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 668.024, `tp_y` = -4931.68, `tp_z` = 3.90933, `tp_o` = 0
    WHERE `zone_id` = 495; -- Howling Fjord, Valgarde

UPDATE `terror_zones_pool` SET `tp_map` = 530, `tp_x` = 158.06, `tp_y` = 2562.73, `tp_z` = 75.7812, `tp_o` = 0
    WHERE `zone_id` = 3483; -- Hellfire Peninsula, Thrallmar

UPDATE `terror_zones_pool` SET `tp_map` = 530, `tp_x` = -212.452, `tp_y` = 5579.67, `tp_z` = 22.178, `tp_o` = 0
    WHERE `zone_id` = 3521; -- Zangarmarsh, Cenarion Refuge

UPDATE `terror_zones_pool` SET `tp_map` = 530, `tp_x` = 3065, `tp_y` = 5426.42, `tp_z` = 148.39, `tp_o` = 0
    WHERE `zone_id` = 3522; -- Blade's Edge Mountains, Evergrove

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 4223.44, `tp_y` = 5335.88, `tp_z` = 30.6522, `tp_o` = 0
    WHERE `zone_id` = 3537; -- Borean Tundra, Fizzcrank Airstrip
