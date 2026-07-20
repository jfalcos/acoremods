-- ============================================================
-- mod-item-infusion — world data: the Alchemist NPC
-- ============================================================
-- HAND-CURATED. Gossip-only NPC (entry 96010, module entry window
-- alongside the Paragon Quartermaster's 96000); spawned manually via
-- `.npc add 96010` wherever the server owner wants it. Model is
-- cloned from Alchemist Mallory (1215), the classic human alchemy
-- trainer.
-- ============================================================

DELETE FROM `creature_template` WHERE `entry` = 96010;
INSERT INTO `creature_template` (
  `entry`, `name`, `subname`, `IconName`, `npcflag`,
  `gossip_menu_id`, `minlevel`, `maxlevel`, `ScriptName`, `faction`
) VALUES (
  96010, 'Arcane Alchemist', 'Item Infusion', 'Speak',
  1, 0, 80, 80, 'ItemInfusion_Alchemist', 35
);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 96010;
INSERT INTO `creature_template_model`
  (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 96010, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`
FROM `creature_template_model` WHERE `CreatureID` = 1215;
