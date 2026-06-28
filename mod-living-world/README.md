# mod-living-world

Foundational state graph module for the Living World Stormwind prototype.

**Status:** Phase 1 — foundation only.

Phase 1 ships per-guild flag storage with cache, persistence, and (slice 2) GM commands. Phases 2–5+ are designed in `docs/living_world_state_graph_design.md` but not built.

## Documentation

- [Operational spec](../../docs/living_world_prototype.md) — what the prototype is testing, phase gates, kill criteria
- [Inspirations and gap analysis](../../docs/living_world_inspirations.md) — the depth model
- [State graph design](../../docs/living_world_state_graph_design.md) — schema, API, actor-type tiers
- [Phase 0 reconnaissance](../../docs/phase_0_recon.md) — fork, hooks, removal story

## Database

Ships its schema/data in [`data/sql/db-characters/`](data/sql/db-characters/) and
[`data/sql/db-world/`](data/sql/db-world/) — AzerothCore auto-applies these on startup.
Tables: `mod_living_world_abstract_actors`, `mod_living_world_relationship_state`
(characters); the world files seed the prototype NPCs/quests (Phases 2–4).

## Removal

See `docs/living_world_state_graph_design.md` §13. Drop the two tables, remove this
directory (which now carries its own `data/sql/`), rebuild.
