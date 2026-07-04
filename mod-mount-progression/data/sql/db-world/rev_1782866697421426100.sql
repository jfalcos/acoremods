-- ============================================================
-- Mount Progression — Slice 4 fix 2 — Arcane starter speed parity
-- ============================================================
-- HAND-CURATED. The catalog's regex-based type classification (Slice 1)
-- puts every "arcane" mount into the flying-mount bucket (drakes,
-- gryphons, wind riders, brooms, carpets) -- that's inherent to what
-- "arcane" means in this taxonomy. The only common-rarity arcane picks
-- are the two cheap Hallow's End brooms (Rickety Magic Broom 42692,
-- Borrowed Broom 61289), and both carry a reduced speed bonus vs. a
-- standard Apprentice-tier ground mount (confirmed live in-game: the
-- broom moved noticeably slower than Brown Horse (458) / Gray Wolf
-- (459), the other two starter picks) -- breaking the "equal power,
-- different flavor" starter design.
--
-- Fix: reclassify Skeletal Horse (8980, common, was default 'stamina')
-- to type='arcane'. It's an ordinary Apprentice-tier ground mount, so
-- it has the same standard speed as the other two starters, while its
-- appearance still reads as more "touched by magic" than a plain horse.
-- Same override pattern as rev_1776705052269966099.sql (Slice 1 manual
-- overrides).
-- ============================================================

UPDATE `mount_progression_catalog`
SET type = 'arcane'
WHERE spell_id = 8980;
