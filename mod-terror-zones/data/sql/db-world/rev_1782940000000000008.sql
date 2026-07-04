-- ============================================================
-- Terror Zones — Teleport unlock — beacon via mod-custom-items
-- ============================================================
-- HAND-CURATED. Supersedes the standalone item_template row (entry
-- 900100, outside mod-custom-items' reserved [700000, 800000) window)
-- with a proper custom_item_template row. The standalone row's icon
-- rendered as a "?" in the bag despite a valid displayid — custom-range
-- entries apparently need the wire-level donor-rewrite trick
-- mod-custom-items already provides (client renders the donor's real,
-- client-known icon; tooltip substitution shows our row's name/
-- description) rather than just setting displayid directly.
--
-- Required a small mod-custom-items code change (CustomItems.cpp) to
-- read/apply this row's spellid_1/spelltrigger_1/ScriptName — the
-- existing loader only ever inherited those three fields from the
-- donor, fine for the gear items it was built for, but it would have
-- silently dropped our ItemScript binding and "ticket" spell.
--
-- Donor: Dimensional Ripper - Everlook (18984) — same real,
-- client-known icon+spell already confirmed working live
-- (rev_1782940000000000006.sql). Not a zero-organic-acquisition donor
-- (Engineers can craft the real one), so there's a narrow, cosmetic-only
-- edge case where a player holding both sees tooltip bleed between them
-- (see CustomItems.cpp's PickCustomEntryForQuery comment) — accepted for
-- this server's scale rather than hunting for an obscurer donor.
-- ============================================================

DELETE FROM `item_template` WHERE `entry` = 900100;

DELETE FROM `custom_item_display_donors` WHERE `custom_entry` = 705000;
INSERT INTO `custom_item_display_donors` (`custom_entry`, `donor_entry`)
VALUES (705000, 18984);

DELETE FROM `custom_item_template` WHERE `entry` = 705000;
INSERT INTO `custom_item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `InventoryType`,
     `ItemLevel`, `RequiredLevel`, `description`,
     `strip_equip_gating`, `strip_vendor_fields`, `force_bonding`,
     `spellid_1`, `spelltrigger_1`, `ScriptName`)
VALUES
    (705000, 15, 0, 'Terror Zone Beacon', 41640, 1, 0,
     1, 1, 'Right-click to choose an unlocked Terror Zone destination. Does not return you home.',
     1, 1, 1,
     23442, 0, 'item_tz_teleport_beacon');
