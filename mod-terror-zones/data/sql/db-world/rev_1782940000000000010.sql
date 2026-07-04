-- ============================================================
-- Terror Zones — Teleport landing points, faction-safety pass.
-- ============================================================
-- The first landing-point pass (rev_...009) picked coordinates named
-- after a zone's best-known hub, without checking who's actually
-- standing there. Cross-checked every one of the 22 zones' landing
-- points against live creature spawns + factiontemplate_dbc
-- (EnemyGroup bitmask) within a 40-50yd radius of the exact
-- coordinate. Six were landing players inside an opposite-faction
-- guarded town, where a real combat-flagged guard sits within
-- ~10-20 yards of the landing spot (would attack on sight):
--   - Duskwood (Darkshire): Deputy Willem stands ON the exact spot.
--   - Hillsbrad Foothills: Hillsbrad Footman ~15yd away.
--   - Loch Modan (Thelsamar): a cluster of Mountaineers nearby.
--   - Howling Fjord (Valgarde): Valgarde Defender ~13yd away.
--   - Hellfire Peninsula (Thrallmar): Bat Rider Guard ~8yd away.
--   - Borean Tundra (Fizzcrank Airstrip): Fizzcrank NPCs nearby.
-- Replaced with alternate graveyard/game_graveyard points in the
-- same zone that came back with zero (or one, harmless, quest-NPC)
-- faction-hostile hits within 50yd. The other 16 zones' original
-- picks were re-verified clean at this same radius and are
-- unchanged (their earlier "hostile NPC" hits, if any, were all
-- well outside real aggro/attack range).
-- ============================================================

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -10606.8, `tp_y` = 294.048, `tp_z` = 31.8007, `tp_o` = 0
    WHERE `zone_id` = 10; -- Duskwood, Ravenhill (was Darkshire — guard on top of the spot)

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -898.726, `tp_y` = -1473.56, `tp_z` = 58.0803, `tp_o` = 0
    WHERE `zone_id` = 267; -- Hillsbrad Foothills, Thoradin's Wall (was near a Footman patrol)

UPDATE `terror_zones_pool` SET `tp_map` = 0, `tp_x` = -5329.98, `tp_y` = -3779.33, `tp_z` = 310.214, `tp_o` = 0
    WHERE `zone_id` = 38; -- Loch Modan, The Loch (was Thelsamar — Mountaineer cluster)

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 1191.77, `tp_y` = -4115.08, `tp_z` = 149.689, `tp_o` = 0
    WHERE `zone_id` = 495; -- Howling Fjord, Central GY (was Valgarde — Defender nearby)

UPDATE `terror_zones_pool` SET `tp_map` = 530, `tp_x` = -1272.42, `tp_y` = 2436.85, `tp_z` = 64.0972, `tp_o` = 0
    WHERE `zone_id` = 3483; -- Hellfire Peninsula, Spinebreaker GY (was Thrallmar — Bat Rider Guard nearby)

UPDATE `terror_zones_pool` SET `tp_map` = 571, `tp_x` = 3621.88, `tp_y` = 6805.54, `tp_z` = 171.706, `tp_o` = 0
    WHERE `zone_id` = 3537; -- Borean Tundra, Coldarra (was Fizzcrank Airstrip — Alliance-aligned quest hub)
