-- ============================================================
-- Terror Zones — Slice 9 Pass 0 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned custom item template table for the
-- Slice 9 display-donor pipeline (mirrors the mod-mount-progression
-- Slice 3 `mount_progression_carrier_spells LIKE spell_dbc`
-- precedent — same shape, same merge-friendly story).
--
-- Custom items live in the reserved entry-ID window [700000,
-- 710000). They are loaded into AC's `_itemTemplateStore` at boot
-- via the new `WorldScript::OnAfterLoadItemTemplates` core hook,
-- so every server-side code path (loot, mail, AH, inventory)
-- treats them as first-class items.
--
-- The wire-side rendering trick: the WotLK 3.3.5a client falls
-- back to the `?` icon for entry IDs missing from its local
-- `Item.dbc`. The module-owned `terror_zones_item_display_donors`
-- map (sibling migration) maps each custom entry to a "donor"
-- entry the client already knows. The `AllItemScript::
-- OnItemBuildValuesUpdate` hook rewrites OBJECT_FIELD_ENTRY in
-- outgoing update packets to the donor's entry — bag/equip/3D
-- rendering all use the donor's display. The tooltip query
-- response (SMSG_ITEM_QUERY_SINGLE_RESPONSE) is left untouched
-- and carries the real custom row, so hover text shows the
-- custom name/stats/quality/flavor.
--
-- Schema parity is automatic via `LIKE item_template`. Schema
-- drift surfaces as an `ASSERT` at boot (same approach as the
-- mount-progression carrier-spells table), not a silent row
-- drop.
--
-- Pass 0 ships ONE row at entry 700000 (Terror Zone Test Axe).
-- Pass 1 will follow with the donor scanner tool + bulk fill
-- across the eight loot bands, gated on Pass 0 in-game smoke.

CREATE TABLE IF NOT EXISTS `terror_zones_item_template`
    LIKE `item_template`;

DELETE FROM `terror_zones_item_template` WHERE `entry` = 700000;

INSERT INTO `terror_zones_item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`,
     `Quality`, `InventoryType`, `ItemLevel`, `RequiredLevel`,
     `stat_type1`, `stat_value1`,
     `stat_type2`, `stat_value2`,
     `dmg_min1`, `dmg_max1`, `dmg_type1`,
     `delay`,
     `description`)
VALUES
    (700000, 2, 0,
     'Terror Zone Test Axe',
     19400,
     4,                              -- Epic quality — visually distinct
     13,                             -- One-hand
     50,
     1,
     7,  20,                         -- ITEM_MOD_STAMINA   = +20
     4,  20,                         -- ITEM_MOD_STRENGTH  = +20
     50, 100, 0,                     -- 50-100 physical dmg (vs donor 27-51)
     1500,                           -- 1.5s speed (vs donor 2.20s)
     'Terror Zones Pass-0 sentinel. Custom name, stats, damage, and '
     'speed all came from the module-owned item template.');
