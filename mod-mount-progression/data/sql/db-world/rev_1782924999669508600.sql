-- ============================================================
-- Mount Progression — Slice 7 — type-count rebalance across all 5 types
-- ============================================================
-- HAND-CURATED. `type` is this mod's own invented taxonomy (buff
-- category), not a property of the real mount — nothing requires it to
-- match how WoW itself would categorize a given spell, so this
-- reclassifies purely to even out the TOTAL usable (excluded=0) mount
-- count per type, at user request. Before: stamina=128, predator=48,
-- agility=57, mechanical=33, arcane=113 (sum 379). Target: ~76 each
-- (379/5), moving mounts OUT of the two oversized types (stamina,
-- arcane) INTO the three undersized ones (predator, agility,
-- mechanical). Rarity is left untouched — only `type` changes, since
-- rarity drives the XP curve/buff ceiling and that tuning stays
-- correct regardless of which type a mount belongs to.
--
-- Legendary rarity is deliberately EXCLUDED from this rebalance in both
-- donor types (stamina's 1 legendary, arcane's 10) — those are the
-- Slice-1 hand-curated spec-named legendaries (Rivendare's Deathcharger,
-- Invincible, Raven Lord, Black Qiraji Battle Tank, etc.) and moving
-- them would undo that deliberate curation for no reason related to
-- this rebalance.
--
-- Also deliberately EXCLUDED from the donor pools: the 4 spell_ids
-- currently pinned as starter-mount picks (458 Brown Horse/stamina,
-- 8980 Skeletal Horse + 25675/25858/25859 Reindeer/arcane, see Slice 4
-- and Slice 6) — reclassifying these out from under the starter
-- config would silently break the starter picker's type/buff match.
--
-- Donor selection within each (type, rarity) bucket is deterministic
-- (ORDER BY spell_id, take the needed count) for reproducibility, not
-- picked by name/theme — by design, since type no longer needs to
-- reflect real-mount flavor.
--
-- Common donors (13: 7 stamina + 6 arcane) -> 4 predator / 3 agility / 6 mechanical
-- Rare donors   (13: 10 stamina + 3 arcane) -> 4 predator / 3 agility / 6 mechanical
-- Epic donors   (64: 35 stamina + 29 arcane) -> 20 predator / 13 agility / 31 mechanical
--
-- After: stamina=76, predator=76, agility=76, mechanical=76, arcane=75.
-- ============================================================

UPDATE `mount_progression_catalog`
SET type = 'predator'
WHERE spell_id IN (
    -- common (4)
    468, 470, 471, 472,
    -- rare (4)
    5784, 6777, 6898, 6899,
    -- epic (20)
    6896, 17460, 17461, 17465, 18991, 18992, 22718, 22719, 22720, 22722,
    23161, 23214, 23227, 23228, 23229, 23238, 23239, 23240, 23246, 23247
);

UPDATE `mount_progression_catalog`
SET type = 'agility'
WHERE spell_id IN (
    -- common (3)
    3363, 6648, 6897,
    -- rare (3)
    13819, 16058, 17462,
    -- epic (13)
    23248, 23249, 23510, 25953, 26054, 26055, 26056, 29059,
    32242, 32246, 32289, 32290, 32292
);

UPDATE `mount_progression_catalog`
SET type = 'mechanical'
WHERE spell_id IN (
    -- common (6)
    16082, 28828, 32345, 40212, 42692, 43810,
    -- rare (6)
    17463, 17464, 18989, 32235, 32239, 32240,
    -- epic (31)
    32295, 32296, 32297, 34407, 34767, 35712, 35713, 35714, 37015,
    39798, 39800, 39801, 39802, 39803, 40192, 41513, 41514, 41515,
    41516, 41517, 41518, 42668, 42683, 43900, 43927, 44317, 44655,
    48027, 48954, 49322, 49378
);
