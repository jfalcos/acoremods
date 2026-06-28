-- ============================================================
-- Mount Progression — Slice 1 — Manual overrides (hand-curated)
-- ============================================================
-- HAND-CURATED. Safe to edit. Preserved on regeneration of the other
-- Slice-1 migrations because this file is separate and only touches
-- specific spell_ids.
--
-- Contents:
--   1. The 5 spec-named legendaries (spec §4) with their designated types
--   2. The AQ40 Black Qiraji Battle Tank (item Quality=5, historical rarity)
--   3. Any classification anomalies surfaced during review
-- ============================================================

-- Spec legendaries — rarity=legendary + type per spec §4.
-- Invincible (ICC, Arcane per spec §4 "Your first spell after mounting is a guaranteed crit")
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'arcane'
WHERE spell_id IN (72281, 72282, 72283, 72284, 72286);

-- Swift Zulian Tiger (Zul'Gurub, Agility per spec §4)
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'agility'
WHERE spell_id = 24252;

-- Reins of the Raven Lord (Sethekk Halls, Arcane per spec §4)
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'arcane'
WHERE spell_id = 41252;

-- Rivendare's Deathcharger (Stratholme, Stamina per spec §4)
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'stamina'
WHERE spell_id = 17481;

-- Fiery Warhorse (Karazhan, Predator per spec §4)
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'predator'
WHERE spell_id = 36702;

-- AQ40 Black Qiraji Battle Tank (Scarab Lord reward; item_template.Quality=5).
-- Spec does not explicitly name this mount, but its unique world-first
-- provenance + legendary quality justify the same tier. Type: arcane
-- (alien/silithid magical creature, closest to "magical companion").
UPDATE `mount_progression_catalog`
SET rarity = 'legendary', type = 'arcane'
WHERE spell_id IN (25863, 26655, 26656, 31700);
