-- ============================================================
-- mod-living-world — Phase 4 — "The Dockside Friend" arc
-- ============================================================
-- A multi-stage arc that crosses two NPCs (Allison + Arn). See
-- docs/phase_4_log.md for the design and verification procedure.
--
-- This migration:
--   1. Adds the new NPC: Arn Brennan (entry 800101), a dock hand.
--   2. Adds 8 npc_text rows for the new branches:
--        80105 Allison HookGiven  — body after she names Marra
--        80106 Allison Helped     — post-resolution if player paid
--        80107 Allison Told       — post-resolution if player told her
--        80108 Arn Default        — random walk-up (no hook)
--        80109 Arn WithHook       — first interaction after Allison sent you
--        80110 Arn Story          — post-revelation (Marra's story told)
--        80111 Arn Helped         — post-resolution if player paid
--        80112 Arn Told           — post-resolution if player told Allison
--
-- Idempotent on re-run.

-- 1. Arn Brennan creature template
DELETE FROM `creature_template`        WHERE `entry`     = 800101;
DELETE FROM `creature_template_model`  WHERE `CreatureID` = 800101;

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`,
     `gossip_menu_id`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `unit_class`, `ScriptName`)
VALUES
    (800101, 'Arn Brennan', 'Dock Hand',
     0, 50, 50,
     35, 1, 1, 'npc_arn_brennan_living_world');

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`,
     `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (800101, 0, 5449, 1, 1, 0);

-- 2. New npc_text rows
DELETE FROM `npc_text` WHERE `ID` BETWEEN 80105 AND 80112;

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES

-- Allison: HookGiven (80105) — she names Marra and sends you to Arn
(80105,
 'Marra worked the docks. Years back. She had a daughter - last I heard, the girl was still down there, taking on debts that weren''t hers. Marra crossed the wrong people. I won''t say more. If you go, ask for Arn. He knew her.',
 'Marra worked the docks. Years back. She had a daughter - last I heard, the girl was still down there, taking on debts that weren''t hers. Marra crossed the wrong people. I won''t say more. If you go, ask for Arn. He knew her.',
 1),

-- Allison: Helped (80106) — terminal body, you paid the debt
(80106,
 'I heard. You paid Marra''s debt. Don''t ask why I knew. The girl''s safer. So am I, in a way I don''t want to explain. Drink''s on me - sit a while.',
 'I heard. You paid Marra''s debt. Don''t ask why I knew. The girl''s safer. So am I, in a way I don''t want to explain. Drink''s on me - sit a while.',
 1),

-- Allison: Told (80107) — terminal body, you came back and told her
(80107,
 'You went down there. I knew you would. Thank you for telling me Marra''s girl is alive. I''d been afraid to ask, and afraid not to.',
 'You went down there. I knew you would. Thank you for telling me Marra''s girl is alive. I''d been afraid to ask, and afraid not to.',
 1),

-- Arn: Default (80108) — random walk-up, no hook from Allison
(80108,
 'Aye, busy day. Crates don''t load themselves. Move along.',
 'Aye, busy day. Crates don''t load themselves. Move along.',
 1),

-- Arn: WithHook (80109) — first interaction with Allison's hook
(80109,
 'Who told you to ask for me by name? Spit it out, I haven''t all day.',
 'Who told you to ask for me by name? Spit it out, I haven''t all day.',
 1),

-- Arn: Story (80110) — post-revelation, the story of Marra
(80110,
 'Marra. Aye, I knew her. She was a friend, and the friend of better people than me. She crossed Greaves the moneylender - borrowed for her daughter''s sickness, couldn''t pay back fast enough. He took it out of her in the end. The girl works the docks now, paying her mother''s debt to the same man. Fifty gold''s what''s left on it. I don''t have it. Few here do.',
 'Marra. Aye, I knew her. She was a friend, and the friend of better people than me. She crossed Greaves the moneylender - borrowed for her daughter''s sickness, couldn''t pay back fast enough. He took it out of her in the end. The girl works the docks now, paying her mother''s debt to the same man. Fifty gold''s what''s left on it. I don''t have it. Few here do.',
 1),

-- Arn: Helped (80111) — post-resolution, player paid
(80111,
 'You paid Greaves off. I won''t ask where you got the gold. The girl knows. She doesn''t know it was you, but she knows. That''ll do.',
 'You paid Greaves off. I won''t ask where you got the gold. The girl knows. She doesn''t know it was you, but she knows. That''ll do.',
 1),

-- Arn: Told (80112) — post-resolution, player told Allison
(80112,
 'Heard you went back to the inn. She''d want to know. Marra''s girl is safer for the knowing - even if nothing changes for her, someone outside the docks remembers her name.',
 'Heard you went back to the inn. She''d want to know. Marra''s girl is safer for the knowing - even if nothing changes for her, someone outside the docks remembers her name.',
 1);
