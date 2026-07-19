-- ============================================================
-- Terror Zones — item stat-budget rebalance (audit F1/F2/F3)
-- ============================================================
-- HAND-CURATED. Fixes findings from the 2026-07-15 progression audit
-- against the offline generator's (docs/terror-zones/tools/tz_item_gen/
-- tz_item_gen.py — lives in the acoremods monorepo, not this checkout)
-- output in `terror_zones_item_template`:
--
--   F1 — band 5 (level 60) sat at 2.4x-4.2x the native epic average
--        stat total for that level (every other band runs 1.0x-1.9x),
--        with a +40 ItemLevel discontinuity at the band4->band5
--        boundary (band4 T5 ilvl 90 -> band5 T1 ilvl 130).
--   F2 — mob difficulty grows ~2.0x-2.3x from T1->T5 within a band
--        (Combat.HpMult/DamageMult x TierHpBonus/TierDamageBonus,
--        mod_terror_zones.conf.dist), but item stat budget only grew
--        ~1.6x-1.8x over the same span — gear reward didn't track
--        difficulty growth.
--   F3 — band 7 (level 80) T5 sat below its own T1-T4 slope.
--
-- Fix: rescale the EXISTING rows in place (preserves every item's name,
-- archetype identity, and per-slot weight shape) rather than
-- regenerating, since the generator isn't available in this checkout.
-- Only bands 4-7 are touched — bands 0-3 were already within ~5 stat
-- points of a native-anchored, difficulty-matched target (noise-level).
--
-- Per-(band,tier) multiply factor = target-budget / current-avg-budget,
-- where target = that band's native epic average (Quality=4 avg stat
-- total in `item_template` at the matching RequiredLevel) x the tier
-- ladder the combat multipliers already use, normalized off T1
-- (1.00 / 1.20 / 1.40 / 1.60 / 2.00). Weapon-slot rows (slot_index=12)
-- additionally scale dmg_min1/dmg_max1 by the same factor so weapon DPS
-- stays consistent with the corrected stat budget; `delay` (weapon
-- speed) is untouched. Verified against a disposable scratch DB before
-- being applied here — see memory `terror-zones-item-progression-audit`.
--
-- Band 5 also gets a flat ItemLevel -30 (130/143/156/169/182 ->
-- 100/113/126/139/152 for T1-T5), turning the +40 band4->band5 jump
-- into a +10 step and moving the one genuinely large ilvl jump to the
-- band5->band6 boundary (level 60->70, the real Outland->Northrend
-- content-tier transition, where a bigger step is authentic). Bands 4,
-- 6, 7 keep their current ItemLevel — only band 5 was flagged.
-- ============================================================

-- Band 4 (req level 50) — factors 0.683 / 0.739 / 0.736 / 0.763 / 0.848
UPDATE `terror_zones_item_template` t
JOIN `terror_zones_event_boss_class_drops` d ON d.item_entry = t.entry
SET
    t.stat_value1  = ROUND(t.stat_value1  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value2  = ROUND(t.stat_value2  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value3  = ROUND(t.stat_value3  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value4  = ROUND(t.stat_value4  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value5  = ROUND(t.stat_value5  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value6  = ROUND(t.stat_value6  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value7  = ROUND(t.stat_value7  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value8  = ROUND(t.stat_value8  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value9  = ROUND(t.stat_value9  * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.stat_value10 = ROUND(t.stat_value10 * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END),
    t.dmg_min1 = IF(d.slot_index = 12, ROUND(t.dmg_min1 * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END, 1), t.dmg_min1),
    t.dmg_max1 = IF(d.slot_index = 12, ROUND(t.dmg_max1 * CASE d.tier WHEN 1 THEN 0.683 WHEN 2 THEN 0.739 WHEN 3 THEN 0.736 WHEN 4 THEN 0.763 WHEN 5 THEN 0.848 END, 1), t.dmg_max1)
WHERE d.band_index = 4;

-- Band 5 (req level 60) — factors 0.415 / 0.431 / 0.437 / 0.433 / 0.480
UPDATE `terror_zones_item_template` t
JOIN `terror_zones_event_boss_class_drops` d ON d.item_entry = t.entry
SET
    t.stat_value1  = ROUND(t.stat_value1  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value2  = ROUND(t.stat_value2  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value3  = ROUND(t.stat_value3  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value4  = ROUND(t.stat_value4  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value5  = ROUND(t.stat_value5  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value6  = ROUND(t.stat_value6  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value7  = ROUND(t.stat_value7  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value8  = ROUND(t.stat_value8  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value9  = ROUND(t.stat_value9  * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.stat_value10 = ROUND(t.stat_value10 * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END),
    t.dmg_min1 = IF(d.slot_index = 12, ROUND(t.dmg_min1 * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END, 1), t.dmg_min1),
    t.dmg_max1 = IF(d.slot_index = 12, ROUND(t.dmg_max1 * CASE d.tier WHEN 1 THEN 0.415 WHEN 2 THEN 0.431 WHEN 3 THEN 0.437 WHEN 4 THEN 0.433 WHEN 5 THEN 0.480 END, 1), t.dmg_max1),
    t.ItemLevel = t.ItemLevel - 30
WHERE d.band_index = 5;

-- Band 6 (req level 70) — factors 0.535 / 0.546 / 0.552 / 0.547 / 0.600
UPDATE `terror_zones_item_template` t
JOIN `terror_zones_event_boss_class_drops` d ON d.item_entry = t.entry
SET
    t.stat_value1  = ROUND(t.stat_value1  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value2  = ROUND(t.stat_value2  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value3  = ROUND(t.stat_value3  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value4  = ROUND(t.stat_value4  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value5  = ROUND(t.stat_value5  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value6  = ROUND(t.stat_value6  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value7  = ROUND(t.stat_value7  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value8  = ROUND(t.stat_value8  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value9  = ROUND(t.stat_value9  * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.stat_value10 = ROUND(t.stat_value10 * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END),
    t.dmg_min1 = IF(d.slot_index = 12, ROUND(t.dmg_min1 * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END, 1), t.dmg_min1),
    t.dmg_max1 = IF(d.slot_index = 12, ROUND(t.dmg_max1 * CASE d.tier WHEN 1 THEN 0.535 WHEN 2 THEN 0.546 WHEN 3 THEN 0.552 WHEN 4 THEN 0.547 WHEN 5 THEN 0.600 END, 1), t.dmg_max1)
WHERE d.band_index = 6;

-- Band 7 (req level 80) — factors 0.957 / 0.970 / 0.972 / 0.983 / 1.141
UPDATE `terror_zones_item_template` t
JOIN `terror_zones_event_boss_class_drops` d ON d.item_entry = t.entry
SET
    t.stat_value1  = ROUND(t.stat_value1  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value2  = ROUND(t.stat_value2  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value3  = ROUND(t.stat_value3  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value4  = ROUND(t.stat_value4  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value5  = ROUND(t.stat_value5  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value6  = ROUND(t.stat_value6  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value7  = ROUND(t.stat_value7  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value8  = ROUND(t.stat_value8  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value9  = ROUND(t.stat_value9  * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.stat_value10 = ROUND(t.stat_value10 * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END),
    t.dmg_min1 = IF(d.slot_index = 12, ROUND(t.dmg_min1 * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END, 1), t.dmg_min1),
    t.dmg_max1 = IF(d.slot_index = 12, ROUND(t.dmg_max1 * CASE d.tier WHEN 1 THEN 0.957 WHEN 2 THEN 0.970 WHEN 3 THEN 0.972 WHEN 4 THEN 0.983 WHEN 5 THEN 1.141 END, 1), t.dmg_max1)
WHERE d.band_index = 7;
