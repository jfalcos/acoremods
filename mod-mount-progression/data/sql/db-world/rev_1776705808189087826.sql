-- ============================================================
-- Mount Progression — Slice 1 — Hand curation pass 2
-- ============================================================
-- HAND-CURATED. Extends the first overrides migration with anomalies that
-- the rules-based rarity/type migrations couldn't catch:
--   1. Class-quest mounts (not item-taught → no name match → missed rarity)
--   2. Type-regex blind spots (Leopard / Tallstrider / Rooster / etc.)
--   3. Non-Black Qiraji Battle Tanks (cousins of the AQ40 legendary)
--   4. Event/holiday mounts that need specific type
--
-- Kept separate from the first manual-overrides file to preserve that
-- file's applied hash in AC's updates tracking.
-- ============================================================

-- ---- Class mounts (epic quality, stamina type) ----
-- Warlock: Felsteed (40) + Dreadsteed (level-60 epic quest)
UPDATE `mount_progression_catalog` SET rarity='rare',  type='stamina' WHERE spell_id = 5784;   -- Felsteed
UPDATE `mount_progression_catalog` SET rarity='epic',  type='stamina' WHERE spell_id = 23161;  -- Dreadsteed
-- Paladin: Warhorse (40) + Charger (level-60 epic quest)
UPDATE `mount_progression_catalog` SET rarity='rare',  type='stamina' WHERE spell_id = 13819;  -- Warhorse
UPDATE `mount_progression_catalog` SET rarity='epic',  type='stamina' WHERE spell_id = 23214;  -- Charger
UPDATE `mount_progression_catalog` SET rarity='epic',  type='stamina' WHERE spell_id = 34767;  -- Summon Charger (variant)
UPDATE `mount_progression_catalog` SET rarity='rare',  type='stamina' WHERE spell_id = 34769;  -- Summon Warhorse (variant)
-- Death Knight: Acherus Deathcharger (starting) + Naxxramas Deathcharger
UPDATE `mount_progression_catalog` SET rarity='rare',  type='stamina' WHERE spell_id = 48778;  -- Acherus Deathcharger
UPDATE `mount_progression_catalog` SET rarity='epic',  type='stamina' WHERE spell_id = 29059;  -- Naxxramas Deathcharger

-- ---- Type-regex blind spots (cats and birds the regex missed) ----
UPDATE `mount_progression_catalog` SET type='agility' WHERE spell_id = 10788;  -- Leopard
UPDATE `mount_progression_catalog` SET type='agility'
WHERE display_name REGEXP 'Tallstrider|Rooster|Dodostrider';

-- ---- Non-Black Qiraji Battle Tanks (AQ40 epic mounts; arcane like Black) ----
UPDATE `mount_progression_catalog` SET rarity='epic', type='arcane'
WHERE spell_id IN (25953, 26054, 26055, 26056);

-- ---- Event / holiday mounts ----
-- Headless Horseman's Mount (Hallow's End, flaming spectral horse → arcane)
UPDATE `mount_progression_catalog` SET rarity='epic', type='arcane'
WHERE spell_id IN (48023, 48024, 48025, 51617, 51621);
-- Flying Reindeer (Winter Veil, flying → arcane)
UPDATE `mount_progression_catalog` SET rarity='epic', type='arcane'
WHERE spell_id IN (44655, 44824, 44825, 44827);
-- Brewfest Riding Kodo (epic Brewfest reward, stamina by default)
UPDATE `mount_progression_catalog` SET rarity='epic' WHERE spell_id = 49378;

-- ---- Faction / tournament mounts ----
-- Grand Caravan Mammoth (Argent Tournament epic)
UPDATE `mount_progression_catalog` SET rarity='epic', type='stamina'
WHERE spell_id IN (60136, 60140);
-- Swift War Elekk (faction reward, stamina)
UPDATE `mount_progression_catalog` SET rarity='rare', type='stamina' WHERE spell_id = 47037;
-- Riding Clefthoof (BC quest reward)
UPDATE `mount_progression_catalog` SET rarity='rare', type='stamina' WHERE spell_id = 39910;
-- Little White Stallion (novelty mount, reissue)
UPDATE `mount_progression_catalog` SET rarity='rare', type='stamina' WHERE spell_id = 68768;
