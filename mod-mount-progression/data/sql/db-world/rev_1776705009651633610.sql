-- ============================================================
-- Mount Progression — Slice 1 — Type enrichment
-- ============================================================
-- Auto-generated (SQL, not Python). Rules-based regex on spell name only.
-- Hand-curated per-spell overrides live in the next migration.
--
-- Layering: UPDATEs applied in order. Later UPDATEs can overwrite earlier
-- ones where patterns overlap (e.g., "X-51 Nether-Rocket" matches both the
-- arcane "nether" pattern and the mechanical "rocket" pattern — mechanical
-- is applied last and wins, which matches spec §3).
--
-- Type taxonomy (spec §3):
--   stamina     horses, elekk, kodos, mammoths        (default, not UPDATED)
--   predator    wolves, raptors, hyenas, bears
--   agility     cats, talbuks, striders (hawk-)
--   arcane      drakes, flying mounts, nether-mounts
--   mechanical  motorcycles, mechano-*, rockets
-- ============================================================

-- Layer 1: predator
UPDATE `mount_progression_catalog`
SET type = 'predator'
WHERE display_name REGEXP 'Wolf|Raptor|Hyena|Worg|Ravasaur|Bear|Venomhide';

-- Layer 2: agility (cats, hawkstriders, talbuks)
UPDATE `mount_progression_catalog`
SET type = 'agility'
WHERE display_name REGEXP 'Tiger|Saber|Hawkstrider|Talbuk|Cheetah|Panther|Warstrider';

-- Layer 3: arcane (flying + magical mounts)
UPDATE `mount_progression_catalog`
SET type = 'arcane'
WHERE display_name REGEXP 'Drake|Dragon|Wyrm|Wyvern|Proto-Drake|Nether Ray|Nether Drake|Netherwing|Windrider|Wind Rider|Gryphon|Hippogryph|Dragonhawk|Carpet|Broom|Warp Stalker|Phoenix|Frostbrood|Frost Wyrm';

-- Layer 4: mechanical (wins over arcane when 'rocket' collides with 'nether')
UPDATE `mount_progression_catalog`
SET type = 'mechanical'
WHERE display_name REGEXP 'Mechano|Chopper|Motorcycle|Turbo|Rocket|Flying Machine|Mekgineer';
