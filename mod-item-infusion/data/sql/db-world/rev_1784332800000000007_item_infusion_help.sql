-- mod-item-infusion UX pass: the Alchemist's "How does infusion work?"
-- gossip explainer page (npc_text 96010, matching the NPC's entry).
DELETE FROM `npc_text` WHERE `ID` = 96010;
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`) VALUES
(96010,
 'Infusion is the art of pouring one item''s essence into another.$B$BChoose an equipped piece to receive the work, then sacrifice a donor from your bags. The donor is always destroyed, and a portion of its native stats soaks into the target - but every attempt risks destroying the target as well. The more an item has already been infused, and the further the work exceeds your mastery, the greater the danger.$B$BPledged Paragon Coins and alchemical stabilizers steady my hand, though they are consumed whether the work succeeds or fails. And mind the quality of your stabilizers - weak reagents do nothing for powerful gear.',
 1);
