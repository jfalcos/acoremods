-- ============================================================
-- Mount Progression — recolor-family type diversification (v2)
-- ============================================================
-- HAND-CURATED. Every mount's buff is entirely determined by its
-- `type` column (see MountProgressionMgr::ApplyMountBuff). Auditing the
-- catalog found ~45 "same base model, different color" families (Swift
-- Gryphon, Swift Ram, Hawkstrider, Wolf, mammoth/mechanostrider/proto-
-- drake groupings, etc.) that were 100% monotype -- e.g. all four Swift
-- Gryphon colors sat at agility, so switching between Blue/Red/Green/
-- Purple Gryphon gave the identical Agility buff regardless of color.
--
-- v1 of this migration (superseded, never shipped beyond this session)
-- only swapped ONE member per family, so most families ended up 3-same-
-- 1-different -- a valid but weak fix. This version spreads EVERY
-- member of every affected family across as many of the 5 types as the
-- family's size allows (e.g. the 4 Swift Gryphons now cover 4 distinct
-- types), while still leaving singleton/already-diverse mounts alone.
--
-- Constraint: rev_1782924999669508600.sql (Slice 7) deliberately evened
-- out the total mount count per type across the whole catalog (~76
-- each). This reassignment is a closed permutation over the 136 touched
-- rows: a round-robin interleave (assign family-order position i the
-- type sitting at type-balanced position i) plus a small manual swap of
-- 8 rows to fix the one family (X-53 Touring Rocket) the round-robin
-- left monotype. Verified before applying: the multiset of `type`
-- values across the 136 touched rows is IDENTICAL before and after (21
-- agility / 21 arcane / 29 mechanical / 30 predator / 35 stamina both
-- ways), so the catalog-wide 76/76/76/76/75 split from Slice 7 is
-- preserved exactly, and zero families end up monotype afterward
-- (verified by query before writing this file).
--
-- Excluded from this pass, matching precedent set by Slice 7:
--   - Reindeer (25675/25858/25859): deliberately unified to arcane in
--     rev_1782923106835295000.sql specifically to give Arcane a needed
--     3rd common-tier ground option -- not an oversight to "fix".
--   - Invincible (72281-72284/72286): legendary rarity, one of the
--     Slice-1 hand-curated spec-named legendaries Slice 7 also refused
--     to touch for the same reason.
--   - The 3 starter-mount spell_ids (458 Brown Horse, 459 Gray Wolf,
--     8980 Skeletal Horse) never appear in the reassignment lists below
--     -- 458/8980 have no monotype family, and 459's family (Wolf)
--     happened to land back on its original type (predator) through
--     the round-robin math, verified explicitly before writing this.
-- ============================================================

UPDATE `mount_progression_catalog`
SET type = 'agility'
WHERE spell_id IN (
    10787, 10795, 10800, 15780, 23239, 32290, 35018, 39800, 48025,
    58997, 59568, 59802, 59961, 61467, 65639, 66907, 71345, 74856,
    75614, 75957, 76153
);

UPDATE `mount_progression_catalog`
SET type = 'arcane'
WHERE spell_id IN (
    10803, 17455, 23223, 23241, 32239, 35022, 35025, 39803, 42680,
    50869, 51621, 54753, 59650, 59996, 60114, 61469, 66124, 68188,
    71342, 71347, 75972
);

UPDATE `mount_progression_catalog`
SET type = 'mechanical'
WHERE spell_id IN (
    578, 581, 10801, 16055, 16080, 17450, 17453, 23222, 23240, 23250,
    32235, 32292, 35020, 39801, 49322, 49379, 51617, 54726, 58999,
    59570, 59788, 59804, 60118, 66123, 67466, 68187, 71346, 75617,
    75973
);

UPDATE `mount_progression_catalog`
SET type = 'predator'
WHERE spell_id IN (
    459, 6654, 10969, 23238, 30837, 32245, 32289, 33630, 34795, 39798,
    46628, 48024, 48954, 50870, 54729, 55293, 58983, 59573, 59785,
    59791, 59799, 60140, 61447, 61465, 61997, 65644, 65917, 71344,
    74855, 76154
);

UPDATE `mount_progression_catalog`
SET type = 'stamina'
WHERE spell_id IN (
    579, 10873, 16081, 17458, 23225, 23243, 23252, 30829, 32240,
    32242, 32244, 35027, 43688, 43899, 47977, 48023, 54727, 59572,
    59793, 59797, 59976, 60116, 60119, 60136, 61294, 61425, 61470,
    61996, 64658, 66122, 71343, 74854, 75618, 75619, 75620
);
