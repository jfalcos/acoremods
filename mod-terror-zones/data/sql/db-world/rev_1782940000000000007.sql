-- ============================================================
-- Terror Zones — Teleport unlock — fix wrong Arathi Highlands landing point
-- ============================================================
-- HAND-CURATED. rev_1782940000000000005.sql used "Innkeeper Shay"'s
-- coordinates, which live tested as landing in Tarren Mill (Hillsbrad
-- Foothills), not Arathi Highlands — that NPC pick was wrong.
--
-- Re-verified zone 45 is genuinely Arathi Highlands via graveyard_zone /
-- game_graveyard (GhostZone is tied directly to the real death/
-- resurrection system, not a display-name guess): both of zone 45's
-- graveyards carry the comment "Arathi Highlands, ..." confirming the
-- zone_id itself is correct. Switching the landing point to the
-- "Eastern Road" graveyard (graveyard_zone.ID 99) — a real, guaranteed
-- safe/walkable spot tied directly to this zone by the game's own
-- systems, not a hand-picked NPC guess.
-- ============================================================

UPDATE `terror_zones_pool`
SET tp_map = 0, tp_x = -1307.66, tp_y = -3192.15, tp_z = 37.7853, tp_o = 0
WHERE zone_id = 45;
