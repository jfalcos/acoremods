-- ============================================================
-- mod-living-world — Phase 3 — Allison the innkeeper
-- ============================================================
-- First real deep NPC: Innkeeper Allison (entry 6740) at the
-- Gilded Rose, Stormwind. Five branches keyed to four flags
-- form a social-investment chain. See docs/phase_3_log.md.
--
-- This migration:
--   1. Adds 5 npc_text rows (80100..80104) with the body text
--      for each branch. Probability0=1 so the row fires.
--   2. Patches creature_template.ScriptName for entry 6740 from
--      'npc_innkeeper' to 'npc_allison_living_world' so our
--      script handles her gossip. Vendor + hearth-bind + quest
--      function are replicated inside the new script.
--
-- Idempotent on re-run via DELETE+INSERT and UPDATE.

-- 1. npc_text rows for body text per branch
DELETE FROM `npc_text` WHERE `ID` BETWEEN 80100 AND 80104;

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(80100,
 'Welcome to the Gilded Rose. We''ve drink, food, and a bed if you need one. Or just a chair, if all you need is somewhere out of the rain. Take your time.',
 'Welcome to the Gilded Rose. We''ve drink, food, and a bed if you need one. Or just a chair, if all you need is somewhere out of the rain. Take your time.',
 1),
(80101,
 'You''re back. Same seat?',
 'You''re back. Same seat?',
 1),
(80102,
 'News? The cathedral''s busy with the war — whole pews of new widows every week. The keep''s quiet; the king''s been keeping his counsel close. The harbor''s restless, sailors arguing over who owes who. And the docks. I don''t hear from the docks anymore.',
 'News? The cathedral''s busy with the war — whole pews of new widows every week. The keep''s quiet; the king''s been keeping his counsel close. The harbor''s restless, sailors arguing over who owes who. And the docks. I don''t hear from the docks anymore.',
 1),
(80103,
 'I had a friend down there. Years back. We don''t speak now, and I don''t speak about her. Drink your drink. Some questions don''t get answered just because you asked them.',
 'I had a friend down there. Years back. We don''t speak now, and I don''t speak about her. Drink your drink. Some questions don''t get answered just because you asked them.',
 1),
(80104,
 'You''re a regular now, near as makes no difference. You ask quiet questions, you don''t pry past a no. Rarer than you''d think. On the house — don''t make it a habit.',
 'You''re a regular now, near as makes no difference. You ask quiet questions, you don''t pry past a no. Rarer than you''d think. On the house — don''t make it a habit.',
 1);

-- 2. Patch Allison's ScriptName
UPDATE `creature_template`
   SET `ScriptName` = 'npc_allison_living_world'
 WHERE `entry` = 6740;
