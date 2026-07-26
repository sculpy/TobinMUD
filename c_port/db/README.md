# Tobin schema (`c_port/db/`)

These are Tobin's **own** database tables and migrations — the pieces that
don't exist in the upstream SneezyMUD seed. Everything else (rooms, mobs,
objects, shops, the `player`/`account` base tables, etc.) comes from the
untouched upstream seed under `../../sneezymud-master/db/`.

## `tobin/` — Tobin-specific tables (loaded into the `tobin` DB)

| File | Purpose |
|---|---|
| `help_topic.sql` | `help_topic` table + seeded topics for every command; edited in-game via `hedit`. |
| `player_attrs.sql` | `player_attrs` (6-stat point-buy set, FK to `player.id`). |
| `player_progress.sql` | `player_progress` (level / xp / hp, `true_level` for the mortal toggle). |
| `tobin_migrations.sql` | Idempotent `ALTER`s that evolve the base `player`/`room` tables (handedness, prompt flags, load_room, room_flag, ...). |

All four are safe to re-run: `CREATE TABLE IF NOT EXISTS`,
`INSERT ... ON DUPLICATE KEY UPDATE name=name`, and idempotent `ALTER`s.

## Seeding a database from scratch

```sh
# 1. upstream seed — creates + fills fresh `tobin` and `immortal` DBs
sneezymud-master/db/init-db.sh [db_user]

# 2. Tobin schema on top (this dir)
c_port/db/apply-tobin-schema.sh          # defaults to the `tobin` DB
```

Step 2 alone also serves as the "apply new migrations to an existing DB"
step after pulling schema changes.
