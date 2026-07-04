-- ============================================================
-- Terror Zones — Teleport unlock — Arathi Highlands landing point
-- ============================================================
-- HAND-CURATED. Second zone landing point (after Duskwood), so the
-- beacon has a real destination for whichever zone/tier happens to be
-- live during testing. Coordinates are the exact live position of
-- "Innkeeper Shay" at Refuge Pointe (creature.guid 15287) — a real,
-- currently-standing NPC, so the spot is guaranteed walkable ground.
-- ============================================================

UPDATE `terror_zones_pool`
SET tp_map = 0, tp_x = -5.96873, tp_y = -942.282, tp_z = 57.1621, tp_o = 2.7415
WHERE zone_id = 45;
