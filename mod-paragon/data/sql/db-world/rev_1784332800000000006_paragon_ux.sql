-- ============================================================
-- mod-paragon UX pass — world data
-- ============================================================
-- HAND-CURATED. Gossip explainer pages for the Quartermaster's
-- "How does all this work?" menu, and the Paragon Handbook: a
-- readable book item mailed once per account at first eligibility
-- (also handed out free from the help menu).
--
-- ID conventions (all in the module's 96000+ window; npc_text,
-- page_text, and item_template are independent namespaces):
--   npc_text  96000-96004  Quartermaster help pages
--   item      96001        The Paragon Handbook
--   page_text 96001-96005  Handbook pages
-- ============================================================

-- 1) Help pages (shown in the gossip parchment above the options)
DELETE FROM `npc_text` WHERE `ID` BETWEEN 96000 AND 96004;
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`) VALUES
(96000,
 'Ask, and I will explain. Prestige rewards the well-informed.',
 1),
(96001,
 'Paragon is a second journey that runs alongside your first.$B$BTell me what share of the experience you earn to set aside, and I will bank it for your entire account. Each time the pool fills, your account gains a Paragon Level and I mail that character a Paragon Coin - with finer gifts at certain milestones.$B$BThe experience you divert is truly spent: the character levels slower while the account grows stronger. Adjust the share, or pause entirely, in my settings - whenever you like.$B$BYoung adventurers must first come of age before the pool accepts their experience.',
 1),
(96002,
 'Paragon Coins buy permanent perks for the character who spends them: Strength, Agility, Stamina, Intellect, Spirit, Attack Power, and Spell Power, one rank at a time.$B$BEarly ranks cost a single coin and the price climbs as training deepens, up to a maximum rank in each. Every character trains separately - coins may be earned by the account, but perks belong to the one who buys them.',
 1),
(96003,
 'For coins I will also work raw power directly into a piece of your equipped gear.$B$BEvery item holds a limited budget for such work - rarer and higher-level pieces hold more. The cost in coins rises as an item fills toward its limit.$B$BThe work binds to that exact item: sell it, shatter it, or feed it to the Alchemist, and the work goes with it.',
 1),
(96004,
 'Each week I stock a small collection of pets and mounts, payable in Paragon Coins. The stock rotates on a four-week cycle - if nothing catches your eye today, patience will be rewarded.',
 1);

-- 2) The Paragon Handbook — readable book (display cloned from the
--    "Legends of the Gurubashi" books, parchment page material).
DELETE FROM `item_template` WHERE `entry` = 96001;
INSERT INTO `item_template`
  (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `bonding`,
   `ItemLevel`, `RequiredLevel`, `AllowableClass`, `AllowableRace`,
   `stackable`, `Material`, `PageText`, `PageMaterial`, `description`)
VALUES
  (96001, 15, 0, 'The Paragon Handbook', 6672, 1, 1,
   1, 0, -1, -1,
   1, -1, 96001, 1,
   'A primer on paragon progression, penned by the Quartermaster.');

DELETE FROM `page_text` WHERE `ID` BETWEEN 96001 AND 96005;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(96001,
 'THE PARAGON HANDBOOK$B$BBy the Paragon Quartermaster$B$B$BParagon is a second journey that runs alongside your first.$B$BSet aside a share of the experience you earn - the choice is yours, changeable at any time - and it flows into a pool shared by your entire account. Each time the pool fills, your account gains a Paragon Level.$B$BThe experience you divert is truly spent: the character levels slower while the account grows stronger.',
 96002),
(96002,
 'COINS AND PERKS$B$BEvery Paragon Level brings a Paragon Coin by mail, and certain milestones bring rarer gifts - companions, mounts, and finery.$B$BCoins buy permanent perks from the Paragon Quartermaster: Strength, Agility, Stamina, Intellect, Spirit, Attack Power, and Spell Power, one rank at a time. Early ranks cost a single coin; the price climbs as training deepens. Perks belong to the character who buys them.',
 96003),
(96003,
 'ITEM UPGRADES$B$BThe Quartermaster can also work raw power directly into a piece of equipped gear, paid in coins.$B$BEvery item holds a limited budget for such work - rarer and higher-level pieces hold more - and the cost rises as the budget fills. The work binds to that exact item and is lost with it.',
 96004),
(96004,
 'INFUSION$B$BThe Arcane Alchemist practices a riskier art: pouring one item''s essence into another.$B$BSacrifice a donor item from your bags and a portion of its native stats soaks into a chosen piece of your gear. The donor is always destroyed - and every attempt risks destroying the target as well. Pledged Paragon Coins and alchemical stabilizers reduce the danger, though they are consumed whether the work succeeds or fails.',
 96005),
(96005,
 'GETTING STARTED$B$BSpeak to the Paragon Quartermaster and choose Paragon settings to begin diverting experience, pause, or change your share.$B$BThe impatient may prefer chat commands:$B  .paragon info$B  .paragon setxp <percent>$B  .paragon toggle$B$BMay your second journey outshine your first.',
 0);
