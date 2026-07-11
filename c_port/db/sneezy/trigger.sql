-- Scripted mob/object/room behavior (user, 2026-07-11: "implement mob
-- object and room scripting -- examine sneezy for ideas -- we want
-- interaction with mobs objs and room via scripts"). Not a port of
-- SneezyMUD's spec proc system (code/code/spec/spec_*.cc,
-- docs/systems/critical/10-spec-procs.md): that system is hardcoded C++
-- functions keyed by a numeric ID, requiring a recompile to add or change
-- behavior. This is the in-game-authorable alternative the user chose
-- instead: a builder attaches a trigger to a mob/obj/room prototype via
-- `edit trigger ...`, and it's stored here as data -- no recompile needed
-- to add a new trigger to a new (or existing) mob/obj/room.
--
-- `target_type`/`target_vnum` identify what the trigger is attached to.
-- `trigger_type` is the event that fires it -- see trigger.h for the
-- fixed set (room: enter, random; mob: greet, speech, death, random;
-- obj: get, wear). `match_text` is only meaningful for `speech` (the
-- keyword said) -- NULL for every other type. `chance_pct` is only
-- meaningful for `random` (rolled once per world tick, same ~60s cadence
-- as mob_ai_tick/obj_pool_decay_tick) -- 100 for every other type (always
-- fires). `script` is a newline-separated list of actions from a small,
-- fixed vocabulary (echo/echoroom/emote/teleport/give/damage/log -- see
-- trigger.c's trigger_run()), not a general-purpose language.
CREATE TABLE IF NOT EXISTS `trigger` (
  `id` int NOT NULL AUTO_INCREMENT,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` varchar(64) NOT NULL DEFAULT '',
  `target_type` varchar(8) NOT NULL,
  `target_vnum` int NOT NULL,
  `trigger_type` varchar(16) NOT NULL,
  `match_text` varchar(64) DEFAULT NULL,
  `chance_pct` int NOT NULL DEFAULT 100,
  `script` text NOT NULL,
  PRIMARY KEY (`id`),
  KEY `target_lookup` (`target_type`, `target_vnum`, `trigger_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
