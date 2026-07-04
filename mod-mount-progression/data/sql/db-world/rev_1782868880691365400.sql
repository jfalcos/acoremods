-- ============================================================
-- Mount Progression — Slice 5 — "Choose Your Companion" starter quest
-- ============================================================
-- HAND-CURATED. quest_template entry 900000, matching the module's
-- existing "custom content id" convention (creature_template 900000 is
-- the Mount Tamer). Confirmed free live against the 900000-900100
-- range. No RequiredNpcOrGo/RequiredItem objectives -- this quest is
-- granted directly via Player::AddQuest (no live questgiver needed,
-- see MaybeSendStarterQuest) and completed+rewarded directly via
-- Player::CompleteQuest + RewardQuest the moment the player picks a
-- mount at the Mount Tamer (see CompleteStarterQuest), not through the
-- standard objective/turn-in UI flow. RewardMoney/items intentionally
-- left at 0 -- the mount itself is the reward; this quest is purely a
-- log entry + mail nudge pointing the player at the Mount Tamer.
-- ============================================================

DELETE FROM `quest_template` WHERE `ID` = 900000;
INSERT INTO `quest_template`
    (`ID`, `QuestLevel`, `MinLevel`, `LogTitle`, `LogDescription`,
     `QuestDescription`, `ObjectiveText1`, `AllowableRaces`)
VALUES
    (900000, 1, 1,
     'Choose Your Companion',
     'A Mount Tamer waits near your starting grounds, ready to bond a new rider with their first companion.',
     'Every rider needs a first companion -- one that will grow alongside you. Seek out the Mount Tamer and choose which bond calls to you: the steady one, the fierce one, or the curious one.',
     'Visit the Mount Tamer and choose your first mount.',
     0);
