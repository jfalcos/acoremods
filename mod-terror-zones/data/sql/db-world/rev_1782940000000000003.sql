-- ============================================================
-- Terror Zones — Teleport unlock — "Terror Zone Beacon" item
-- ============================================================
-- HAND-CURATED. Supersedes the earlier SpellScript-hijack approach
-- (rev_1782940000000000002.sql, spell 1206 bound via spell_script_names):
-- live testing found every genuinely spare/unreferenced SPELL_EFFECT_DUMMY
-- spell in this client also has SpellIconID = 0, which the WotLK client
-- silently refuses to render or let you /cast by name — the spell was
-- confirmed "known" (`.learn` said so) but permanently invisible/uncastable.
-- No client-side fix exists without patching the client's DBC, which this
-- module avoids.
--
-- Items don't have that problem — name/tooltip/icon are plain SQL columns
-- decoupled from any spell's DBC data. One reusable item ("Terror Zone
-- Beacon", modeled on Hearthstone: item_template 6948) is granted the
-- first time ANY tier unlocks; using it (ItemScript::OnUse,
-- TerrorZonesTeleportItem.cpp) opens a gossip menu built from whichever
-- tiers TerrorZonesMgr::IsTierUnlockedFor says are unlocked, and picking
-- one calls TeleportPlayerToTier — same destination-resolution logic as
-- before, just triggered by ItemScript::OnGossipSelect instead of a
-- SpellScript. No spell involved at all, so no per-tier spell id to find.
--
-- Cleans up the now-unused spell_script_names row from the superseded
-- approach.
-- ============================================================

DELETE FROM `spell_script_names` WHERE `spell_id` = 1206 AND `ScriptName` = 'spell_tz_teleport_tier';

DELETE FROM `item_template` WHERE `entry` = 900100;
INSERT INTO `item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `InventoryType`,
     `maxcount`, `stackable`, `bonding`, `ScriptName`)
VALUES
    (900100, 15, 0, 'Terror Zone Beacon', 6418, 1, 0,
     1, 1, 1, 'item_tz_teleport_beacon');

DELETE FROM `npc_text` WHERE `ID` = 900100;
INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES (900100, 'Where would you like to go?');
