-- ============================================================
-- Bag Sorter — dedicated "Bag & Bank Organizer" NPC (Stormwind)
-- ============================================================
-- Moves the deposit/organize-bank gossip off the real banker (forcing the
-- Gossip npcflag onto banker NPCs was confusing - they'd show a gossip box
-- with unrelated-looking options before you could reach the real bank) and
-- onto a small dedicated NPC that stands next to one. Bag sorting itself
-- stays on innkeepers (unaffected by this change).
--
-- Entry 900001 confirmed free against the live acore_world DB. Uses ONLY
-- UNIT_NPC_FLAG_GOSSIP (1) - never UNIT_NPC_FLAG_BANKER - with ScriptName
-- npc_bag_bank_organizer (registered in mod_bag_sorter_bank_npc.cpp), so this
-- NPC only ever offers our own menu, never the native bank window.
--
-- Model reuses CreatureDisplayID 1436 (already used by Newton Burnside,
-- entry 2456, one of the live Stormwind Bank tellers) for a fitting look.
--
-- Spawn is anchored ~3 yards from Newton Burnside (guid 79678, map 0,
-- -8935.27 613.135 99.606 - Stormwind Bank, Trade District), queried live,
-- so the ground is known-walkable.
--
-- NOTE: creature_template_model is a separate table in this schema version
-- (creature_template no longer carries model IDs directly) - without a row
-- there, ObjectMgr::LoadCreatureTemplates rejects the whole template. See
-- modules/mod-mount-progression's rev_1782865635620009100.sql for the same
-- gotcha hit previously; this file includes the model row up front.
-- ============================================================

DELETE FROM `creature_template` WHERE `entry` = 900001;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `dmgschool`,
     `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`,
     `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`,
     `lootid`, `pickpocketloot`, `skinloot`, `VehicleId`, `mingold`, `maxgold`,
     `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`,
     `ArmorModifier`, `DamageModifier`, `ExperienceModifier`, `RacialLeader`,
     `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`)
VALUES
    (900001, 'Bag & Bank Organizer', 'Bag Sorter', '', 900001, 1, 1,
     35, 1, 1, 1.14286, 0, 0,
     2000, 2000, 1, 1, 1,
     0, 2048, 0, 0, 7, 0,
     0, 0, 0, 0, 0, 0,
     '', 0, 1, 1, 1,
     1, 1, 1, 0,
     0, 1, 0, 0, 'npc_bag_bank_organizer');

DELETE FROM `creature_template_model` WHERE `CreatureID` = 900001;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (900001, 0, 1436, 1, 1);

-- Stormwind only for now, next to Newton Burnside (bank teller, guid 79678).
DELETE FROM `creature` WHERE `id` = 900001;
INSERT INTO `creature`
    (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`,
     `position_y`, `position_z`, `orientation`, `spawntimesecs`, `MovementType`)
VALUES
    (900001, 0, 0, 0, 1, 1, -8933.6, 616.4, 99.606, 0.5, 300, 0);

DELETE FROM `npc_text` WHERE `ID` = 900001;
INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES (900001, 'Bags overflowing? Bank a mess? I can sort both for you.');

DELETE FROM `gossip_menu` WHERE `MenuID` = 900001;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (900001, 900001);
