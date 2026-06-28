-- ============================================================
-- mod-living-world — Phase 1 — relationship state edges
-- ============================================================
-- The graph's edge table. One row per (actor_a, actor_b, key)
-- triple, with a typed value. See
-- docs/living_world_state_graph_design.md §3.4, §5.
--
-- value_type enum: 1=Int (in value_int), 2=Bool (in value_int),
--                  3=String (in value_string), 4=Enum (in value_string).

CREATE TABLE IF NOT EXISTS `mod_living_world_relationship_state` (
    `actor_a_type` TINYINT UNSIGNED NOT NULL,
    `actor_a_id`   BIGINT  UNSIGNED NOT NULL,
    `actor_b_type` TINYINT UNSIGNED NOT NULL,
    `actor_b_id`   BIGINT  UNSIGNED NOT NULL,
    `state_key`    VARCHAR(64)      NOT NULL,
    `value_type`   TINYINT UNSIGNED NOT NULL,
    `value_int`    BIGINT           NULL,
    `value_string` VARCHAR(255)     NULL,
    `updated_at`   TIMESTAMP        NOT NULL
                   DEFAULT CURRENT_TIMESTAMP
                   ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`actor_a_type`, `actor_a_id`,
                 `actor_b_type`, `actor_b_id`, `state_key`),
    INDEX `idx_actor_a` (`actor_a_type`, `actor_a_id`),
    INDEX `idx_actor_b` (`actor_b_type`, `actor_b_id`),
    INDEX `idx_key`     (`state_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
