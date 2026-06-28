-- ============================================================
-- Terror Zones — Slice 9 Pass 0 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned map of custom_entry -> donor_entry
-- consulted by `AllItemScript::OnItemBuildValuesUpdate` to rewrite
-- OBJECT_FIELD_ENTRY in outgoing item update packets. The donor
-- is a real, client-known item entry whose display info
-- (`Item.dbc` + `ItemDisplayInfo.dbc`) the client already has —
-- the bag/equip/3D paths render the donor's icon and model, the
-- tooltip query path renders the real custom item.
--
-- Pass 0 donor selection: entry 2080 "Hillborne Axe" (displayid
-- 19400). DB scan against creature_loot_template / npc_vendor /
-- quest_template at curation time (2026-04-25) showed zero loot
-- sources, zero vendors, zero quest rewards — orphan item, no
-- player is wearing or seeing it organically. Maximum-obscurity
-- pick for the pipeline-validation Pass.
--
-- Pass 1 will replace this hand-pick with output from
-- docs/terror-zones/tools/scan_item_display_donors.py running
-- against the 8 loot-pool bands.

CREATE TABLE IF NOT EXISTS `terror_zones_item_display_donors` (
    `custom_entry` INT UNSIGNED NOT NULL,
    `donor_entry`  INT UNSIGNED NOT NULL,
    PRIMARY KEY (`custom_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `terror_zones_item_display_donors`
    WHERE `custom_entry` = 700000;

INSERT INTO `terror_zones_item_display_donors`
    (`custom_entry`, `donor_entry`)
VALUES
    (700000, 2080);   -- Terror Zone Test Axe -> Hillborne Axe
