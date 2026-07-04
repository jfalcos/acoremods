-- ============================================================
-- Terror Zones — Teleport unlock — beacon icon/ticket-spell swap
-- ============================================================
-- HAND-CURATED. Fixes two issues found live-testing rev_...004's
-- Hearthstone-spell choice (displayid 6418 / spell 8690):
--
-- 1. The item rendered as a "?" placeholder icon in the bag despite
--    displayid 6418 being valid, real data (identical to Hearthstone's
--    own item_template row) — root cause not conclusively identified
--    (likely a lingering client-cache artifact from this exact item
--    entry's earlier broken states), but not worth chasing further.
-- 2. Hearthstone's "Use:" tooltip text ("Returns you to <bind city>...")
--    is confusing for a terror-zone beacon — this is an unfixable
--    constraint of reusing a real spell (that text is 100%
--    client-rendered from the spell's own local Description, not
--    server-controllable — see design notes in TerrorZonesTeleportItem.cpp).
--
-- Fix: switch to Dimensional Ripper - Everlook's real item data (entry
-- 18984 in the live DB) — a genuine Engineering teleport-gadget item
-- used by real players constantly, so both its icon (41640) and spell
-- (23442) are guaranteed-correct, real, client-known values. Thematically
-- closer too ("teleport gadget" vs. "return home"), even though the
-- specific stated destination text is still not accurate to what this
-- item actually does.
-- ============================================================

UPDATE `item_template`
SET displayid = 41640, spellid_1 = 23442, spelltrigger_1 = 0
WHERE entry = 900100;
