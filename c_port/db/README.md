# Tobin schema (`c_port/db/`)

These are Tobin's **own** database tables and migrations — the pieces
layered on top of the world seed. Everything else (rooms, mobs, objects,
shops, the `player`/`account` base tables, etc.) comes from the **project's
own** seed under [`seed/`](seed/) — `seed/world/` → the `tobin` DB,
`seed/immortal/` → the `immortal` builder-sandbox DB. That seed is a
one-time copy of the original upstream world dumps, now owned here; the
build no longer depends on the old `sneezy` database or on
`sneezymud-master/` at all (user 2026-08-16: "no more sneezy db seed").

## `tobin/` — Tobin-specific tables (loaded into the `tobin` DB)

| File | Purpose |
|---|---|
| `help_topic.sql` | `help_topic` table + seeded topics for every command; edited in-game via `hedit`. |
| `player_attrs.sql` | `player_attrs` (6-stat point-buy set, FK to `player.id`). |
| `player_progress.sql` | `player_progress` (level / xp / hp, `true_level` for the mortal toggle). |
| `tobin_migrations.sql` | Idempotent `ALTER`s that evolve the base `player`/`room` tables (handedness, prompt flags, load_room, room_flag, ...). |

All four are safe to re-run: `CREATE TABLE IF NOT EXISTS`,
`INSERT ... ON DUPLICATE KEY UPDATE name=name`, and idempotent `ALTER`s.

## `seed/` — the game-world seed (project-owned)

`seed/world/*.sql` and `seed/immortal/*.sql` are self-contained mysqldump
files (CREATE + INSERT) for the base world and the builder sandbox. Refresh
them from a live DB with `sneezymud-master/db/update-seed-data.sh` if/when a
newer world snapshot is wanted (they are a point-in-time baseline; live
builder edits accumulate beyond them and are brought current by migrations).

## Seeding a database from scratch

One command builds and seeds both DBs and applies the Tobin schema — no
`sneezy` DB, no `sneezymud-master` dependency:

```sh
c_port/db/init-db.sh [db_user]        # -> creates + seeds `tobin` + `immortal`
```

To seed a throwaway copy for testing without touching the live DBs, pass
alternate names: `init-db.sh mud tobintest immortaltest`.

`apply-tobin-schema.sh [db_name]` (run for you by `init-db.sh`) can also be
run alone as the "apply new migrations to an existing DB" step after pulling
schema changes.
