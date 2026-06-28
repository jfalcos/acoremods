-- ============================================================
-- Terror Zones — Slice 6 — hand-curated
-- ============================================================
-- HAND-CURATED. Module-owned tracker for dynamic events fired
-- inside empowered zones. Rows are written at rotation tick
-- time (state=PENDING) by TerrorZonesMgr::ScheduleEvents and
-- flipped to ACTIVE / EXPIRED by the lifecycle tick. Resume
-- path at worldserver boot (LoadActiveEvents) re-reads rows
-- with endsAt > now, rebuilds live state, and re-summons
-- spawn entities. Pre-restart spawn GUIDs are NOT reattached
-- for the MVP — temp-summons die with the worldserver process,
-- so we summon fresh (same eventId) rather than track dangling
-- GUIDs. See docs/terror-zones/SLICE_6_PLAN.md §2.5.
--
-- Primary key `(tick_at, slot_index, event_id)` is event-type
-- agnostic so treasure caravan + champion grounds (deferred to
-- Slice 6b) slot in without schema churn. `event_type` values
-- map to the mod_terror_zones::EventType enum.
--

CREATE TABLE IF NOT EXISTS `terror_zones_events` (
  `tick_at`       BIGINT UNSIGNED NOT NULL,
  `slot_index`    INT UNSIGNED NOT NULL,
  `event_id`      INT UNSIGNED NOT NULL,
  `event_type`    TINYINT UNSIGNED NOT NULL,
  `state`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `starts_at`     BIGINT UNSIGNED NOT NULL,
  `ends_at`       BIGINT UNSIGNED NOT NULL,
  `zone_id`       INT UNSIGNED NOT NULL,
  `map_id`        INT UNSIGNED NOT NULL,
  `definition_id` INT UNSIGNED NOT NULL,
  `anchor_x`      FLOAT NOT NULL,
  `anchor_y`      FLOAT NOT NULL,
  `anchor_z`      FLOAT NOT NULL,
  PRIMARY KEY (`tick_at`, `slot_index`, `event_id`),
  KEY `idx_state_ends_at` (`state`, `ends_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
