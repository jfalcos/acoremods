-- T1-now-drops-items + 5-tier encoding change wipe. Encoding's
-- per-band stride grew from 240 to 300, invalidating every cell's
-- (band, tier, archetype, slot) coordinates relative to entry ID.
-- Pass 0 sentinel at entry 700000 is preserved.

DELETE FROM `terror_zones_item_template`
    WHERE `entry` <> 700000;

DELETE FROM `terror_zones_item_display_donors`
    WHERE `custom_entry` <> 700000;

DELETE FROM `terror_zones_event_boss_class_drops`;
