-- ============================================================
-- mod-living-world — Phase 3 — ASCII-safe npc_text fixup
-- ============================================================
-- 3.3.5a vanilla NPC text is uniformly ASCII; the WoW client renders
-- multi-byte UTF-8 inconsistently. Replace em-dashes in Allison's
-- branches with ' - '. No other multi-byte characters were used.

UPDATE `npc_text`
   SET `text0_0` = 'News? The cathedral''s busy with the war - whole pews of new widows every week. The keep''s quiet; the king''s been keeping his counsel close. The harbor''s restless, sailors arguing over who owes who. And the docks. I don''t hear from the docks anymore.',
       `text0_1` = 'News? The cathedral''s busy with the war - whole pews of new widows every week. The keep''s quiet; the king''s been keeping his counsel close. The harbor''s restless, sailors arguing over who owes who. And the docks. I don''t hear from the docks anymore.'
 WHERE `ID` = 80102;

UPDATE `npc_text`
   SET `text0_0` = 'You''re a regular now, near as makes no difference. You ask quiet questions, you don''t pry past a no. Rarer than you''d think. On the house - don''t make it a habit.',
       `text0_1` = 'You''re a regular now, near as makes no difference. You ask quiet questions, you don''t pry past a no. Rarer than you''d think. On the house - don''t make it a habit.'
 WHERE `ID` = 80104;
