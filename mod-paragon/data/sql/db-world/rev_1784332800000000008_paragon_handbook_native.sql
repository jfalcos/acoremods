-- ============================================================
-- mod-paragon: Paragon Handbook -> commandeered native entry 3899
-- ============================================================
-- HAND-CURATED. Supersedes the item half of rev_..._006_paragon_ux.sql.
--
-- The custom entry 96001 rendered a red question mark: the 3.3.5
-- client resolves item ICONS through its local Item.dbc keyed by
-- item ENTRY (the displayid in the server's query response is
-- ignored), so any entry absent from the client DBC cannot render
-- an icon - same reason the Paragon Coin commandeers native 37711.
--
-- Donor: 3899 "Legends of the Gurubashi, Volume 3" - a vanilla
-- leftover lore book with ZERO acquisition paths (no loot, vendor,
-- or quest references) and zero owned instances (verified
-- 2026-07-21). Already a readable book with a book icon.
--
-- Original 3899 row for restoration (all other columns untouched):
--   name 'Legends of the Gurubashi, Volume 3', Quality 0, bonding 0,
--   BuyPrice 100, SellPrice 25, description 'Stone of the Tides',
--   PageText 286, PageMaterial 0
-- (its original page chain 286 is left in place, unreferenced)
DELETE FROM `item_template` WHERE `entry` = 96001;
UPDATE `item_template` SET
  `name` = 'The Paragon Handbook',
  `Quality` = 1,
  `bonding` = 1,           -- bind on pickup: it is handed out free
  `BuyPrice` = 0,
  `SellPrice` = 0,         -- no vendor value: free handout must not be a money loop
  `description` = 'A primer on paragon progression, penned by the Quartermaster.',
  `PageText` = 96001,
  `PageMaterial` = 1
WHERE `entry` = 3899;
