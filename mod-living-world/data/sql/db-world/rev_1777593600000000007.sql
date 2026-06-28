-- ============================================================
-- mod-living-world — Phase 4 — consequence layer (WoW primitives)
-- ============================================================
-- Adds the artifact + world-change consequences for the Dockside
-- Friend arc, using existing WoW systems (item_template, creature
-- spawns, faction reputation) rather than parallel infrastructure.
--
-- Per docs/living_world_inspirations.md §4.2: bend WoW primitives
-- past where Blizzard took them; only build new infra where there's
-- no native equivalent.
--
-- This migration adds:
--   1. Item 800200 — "Allison's House Pour" (consumable; food buff
--      via existing spell 11009, the Junglevine Wine effect)
--   2. Item 800201 — "Allison's Token" (BoP keepsake, no buff)
--   3. Creature 800102 — "Lia" (Marra's daughter), permanent dock spawn
--   4. Creature spawn for Lia at the Park-district canal
--
-- The state-graph still gates *who sees what* (Lia's gossip varies
-- per viewer). Faction-rep shifts and item rewards live in the C++
-- arc-resolution handlers, not here.

-- 1. Allison's House Pour (consumable food/drink)
DELETE FROM `item_template` WHERE `entry` = 800200;
INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`,
     `Quality`, `Bonding`, `BuyPrice`, `SellPrice`,
     `InventoryType`, `Stackable`, `MaxCount`,
     `spellid_1`, `spelltrigger_1`,
     `description`, `Material`, `FoodType`)
VALUES
    (800200, 0, 5, 'Allison''s House Pour', 18078,
     2, 1, 0, 0,
     0, 5, 0,
     11009, 0,
     'Served only to those Innkeeper Allison considers regulars.', 3, 0);

-- 2. Allison's Token (quest-like keepsake)
DELETE FROM `item_template` WHERE `entry` = 800201;
INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`,
     `Quality`, `Bonding`, `BuyPrice`, `SellPrice`,
     `InventoryType`, `Stackable`, `MaxCount`,
     `description`, `Material`)
VALUES
    (800201, 12, 0, 'Allison''s Token', 6499,
     2, 1, 0, 0,
     0, 1, 1,
     'A small keepsake from Innkeeper Allison. She doesn''t give these out lightly.', 0);

-- 3. Lia (Marra's daughter)
DELETE FROM `creature_template`        WHERE `entry`     = 800102;
DELETE FROM `creature_template_model`  WHERE `CreatureID` = 800102;

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`,
     `gossip_menu_id`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `unit_class`, `ScriptName`)
VALUES
    (800102, 'Lia', 'Dock Worker',
     0, 30, 30,
     35, 1, 1, 'npc_lia_living_world');

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`,
     `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (800102, 0, 1501, 1, 1, 0);

-- 4. Permanent spawn at the Park-district canal area
DELETE FROM `creature` WHERE `id1` = 800102;
INSERT INTO `creature`
    (`guid`, `id1`, `id2`, `id3`,
     `map`, `zoneId`, `areaId`,
     `position_x`, `position_y`, `position_z`, `orientation`,
     `spawntimesecs`, `MovementType`)
VALUES
    (800102, 800102, 0, 0,
     0, 1519, 1519,
     -8800, 870, 30, 0,
     300, 0);

-- 5. npc_text rows for Lia
--   80113 — default (debt-burdened; what everyone sees if no one in
--           their membership tree has helped)
--   80114 — helped (the consequence that "the world is different");
--           Tell path doesn't change Lia (her circumstance is unchanged)
DELETE FROM `npc_text` WHERE `ID` BETWEEN 80113 AND 80114;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(80113,
 'I''ve work to do. Crates won''t move themselves, and the lender''s man comes by every Tuesday. If you''re not buying or selling, please - leave me to it.',
 'I''ve work to do. Crates won''t move themselves, and the lender''s man comes by every Tuesday. If you''re not buying or selling, please - leave me to it.',
 1),
(80114,
 'Strangers paid my mother''s debt. I don''t know who. I''m not asking. The lender''s man came by Tuesday, looked at his book, and left without a word. First time in five years he''s done that. If you knew her - thank you. If you didn''t, thank you anyway.',
 'Strangers paid my mother''s debt. I don''t know who. I''m not asking. The lender''s man came by Tuesday, looked at his book, and left without a word. First time in five years he''s done that. If you knew her - thank you. If you didn''t, thank you anyway.',
 1);
