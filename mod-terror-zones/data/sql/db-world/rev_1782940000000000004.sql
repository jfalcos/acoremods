-- ============================================================
-- Terror Zones — Teleport unlock — beacon needs a linked "ticket" spell
-- ============================================================
-- HAND-CURATED. Fixes rev_1782940000000000003.sql: right-clicking the
-- beacon (spellid_1 = 0) did nothing. Root cause: WorldSession::
-- HandleUseItemOpcode reads the spellId to use from the CLIENT's own
-- "use item" packet (populated from the item's synced spellid_1) and
-- immediately bails out with "unknown spell id" if that resolves to no
-- real SpellInfo — this check happens BEFORE ItemScript::OnUse is ever
-- dispatched. An item with no linked spell therefore never reaches our
-- gossip-opening code at all.
--
-- Fix: link spell 1206 (the same confirmed-unused, harmless internal
-- dummy spell from the superseded SpellScript approach) as spellid_1 /
-- ON_USE trigger. It never actually casts — ItemScript::OnUse
-- (TerrorZonesTeleportItem.cpp) always returns true, which skips
-- CastItemUseSpell entirely — so 1206's own icon-less/uncastable-from-
-- spellbook problem (the reason the earlier approach was abandoned)
-- doesn't apply here: item use never goes through the spellbook.
-- ============================================================

UPDATE `item_template`
SET spellid_1 = 1206, spelltrigger_1 = 0
WHERE entry = 900100;
