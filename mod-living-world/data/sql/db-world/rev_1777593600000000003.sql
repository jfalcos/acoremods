-- ============================================================
-- mod-living-world — Phase 2 — test speaker NPC (throwaway)
-- ============================================================
-- A custom NPC entry used by the Phase 2 reactive-layer demo.
-- See docs/phase_2_demo.md.
--
-- Entry 800100 in module-owned ID range. Idempotent on re-run
-- via DELETE + INSERT (no ON DUPLICATE KEY needed since the
-- key set is small and module-owned).

DELETE FROM `creature_template`        WHERE `entry`     = 800100;
DELETE FROM `creature_template_model`  WHERE `CreatureID` = 800100;

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`,
     `gossip_menu_id`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `unit_class`, `ScriptName`)
VALUES
    (800100, 'Living World Test Speaker', 'Phase 2 Plumbing',
     0, 60, 60,
     35, 1, 1, 'npc_lwtest_speaker');

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`,
     `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (800100, 0, 5444, 1, 1, 0);
