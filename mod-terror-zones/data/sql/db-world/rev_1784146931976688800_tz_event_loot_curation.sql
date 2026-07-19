-- ============================================================
-- Terror Zones — event-boss bonus loot curation (audit F4a)
-- ============================================================
-- HAND-CURATED. Pass-2 curation for `terror_zones_event_bosses_loot_pool`
-- (created rev_1777171200000000001.sql). The Pass-1 comment on that
-- migration flagged the guaranteed-blue slot as a placeholder (raw
-- trade-good stacks — Linen Cloth, Copper Ore, Runecloth, Frostweave)
-- and purple_item_id as 0 everywhere pending real curation. This
-- replaces both with real, era-appropriate BoE items pulled from live
-- `item_template` (Quality 3/4, equippable slot, bonding=2, filtered
-- for QA/test rows, ranked by ItemLevel within each band).
--
-- No epic-quality item exists natively below ~level 30 in this DB, so
-- bands 10-19 and 20-29 keep purple_item_id=0 (nothing to curate) —
-- their purple_chance is zeroed too so the roll isn't a dead no-op.
-- ============================================================

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 890,      -- Twisted Chanter's Staff (ilvl 24)
    `purple_item_id`          = 0,
    `purple_chance`           = 0
WHERE `level_min` = 10;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 7728,     -- Beguiler Robes (ilvl 34)
    `purple_item_id`          = 0,
    `purple_chance`           = 0
WHERE `level_min` = 20;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 1715,     -- Polished Jazeraint Armor (ilvl 44)
    `purple_item_id`          = 1981,     -- Icemail Jerkin (ilvl 44)
    `purple_chance`           = 0.20
WHERE `level_min` = 30;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 1203,     -- Aegis of Stormwind (ilvl 54)
    `purple_item_id`          = 810,      -- Hammer of the Northern Wind (ilvl 54)
    `purple_chance`           = 0.20
WHERE `level_min` = 40;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 19050,    -- Mantle of the Timbermaw (ilvl 64)
    `purple_item_id`          = 12641,    -- Invulnerable Mail (ilvl 63)
    `purple_chance`           = 0.25
WHERE `level_min` = 50;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 22757,    -- Sylvan Crown (ilvl 70)
    `purple_item_id`          = 22664,    -- Icy Scale Breastplate (ilvl 80)
    `purple_chance`           = 0.25
WHERE `level_min` = 60;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 34366,    -- Sunfire Handwraps (ilvl 159)
    `purple_item_id`          = 44210,    -- Faces of Doom (ilvl 200)
    `purple_chance`           = 0.25
WHERE `level_min` = 70;

UPDATE `terror_zones_event_bosses_loot_pool`
SET `guaranteed_blue_item_id` = 37196,    -- Runecaster's Mantle (ilvl 200)
    `purple_item_id`          = 49890,    -- Deathfrost Boots (ilvl 264)
    `purple_chance`           = 0.25
WHERE `level_min` = 80;
