-- ============================================================
-- mod-living-world — Phase 1 — abstract actors lookup
-- ============================================================
-- Lookup table for "abstract" graph nodes — string-named nodes
-- not tied to in-game entities. Phase 1 reserves id=1 for the
-- 'global' anchor used by per-guild flag edges.
--
-- See docs/living_world_state_graph_design.md §4, §5.

CREATE TABLE IF NOT EXISTS `mod_living_world_abstract_actors` (
    `id`         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name`       VARCHAR(64)     NOT NULL,
    `created_at` TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY  `uk_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `mod_living_world_abstract_actors` (`id`, `name`)
VALUES (1, 'global');
