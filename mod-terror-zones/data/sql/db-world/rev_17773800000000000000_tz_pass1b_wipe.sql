-- Wipe Pass 1 phase-1 test rows ahead of the Pass 1b regeneration
-- (5-playstyle / 12-slot encoding change invalidates the old entry
-- IDs). Pass 0 sentinel at entry 700000 is preserved.

DELETE FROM `terror_zones_item_template`
    WHERE `entry` <> 700000;

DELETE FROM `terror_zones_item_display_donors`
    WHERE `custom_entry` <> 700000;

DELETE FROM `terror_zones_event_boss_class_drops`;
