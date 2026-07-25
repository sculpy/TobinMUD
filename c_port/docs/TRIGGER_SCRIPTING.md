# Trigger Scripting Reference

Tobin's in-game scripting system: attach a small script to a room, mob, or
object prototype via `edit trigger`, no recompile needed. As of 2026-07-25
the script language is a DG Scripts-style port (see `trigger_script.h`/`.c`
and `trigger.c`) — real branching, loops, and variables, ported from
tbaMUD's `dg_scripts.c`/`dg_variables.c`. Authoring still goes entirely
through the existing `edit trigger` flow (this was a language upgrade, not
a new editor).

## Attaching a trigger

```
edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]
edit trigger list <room|mob|obj> <vnum>
edit trigger delete <id>
```

Builder level (51+) required. After the header line you land in the shared
line editor to write the script body, one command per line — `/s` saves,
`/a` aborts, `/b` blanks the buffer, `/f` reflows to width.

### Trigger types

| Target | Type | Fires when | `match_text`/`chance` |
|---|---|---|---|
| room | `enter` | someone walks in | — |
| room | `random` | ambient, rolled every world tick | percent chance (default 25) |
| mob | `greet` | someone walks into the mob's room | — |
| mob | `speech` | someone says a matching keyword nearby | the keyword |
| mob | `death` | the mob dies (before it's removed) | — |
| mob | `random` | ambient, rolled every world tick | percent chance |
| obj | `get` | the object is picked up | — |
| obj | `wear` | the object is worn | — |

Only these firing hook points exist — this revamp changed the *language*,
not the trigger-type roster.

## Execution context

Every script line runs against:
- **actor** — the being who caused the event (the walker, speaker, wearer,
  killer). `NULL` for a pure-ambient `random` trigger (nobody in
  particular caused it) and, deliberately, after a `wait` resumes (see
  below).
- **room** — where it's happening. Always present.
- **self** — the mob/room's own display name, for actions that "speak" as
  it (`emote`/`say`). `NULL` for room triggers (no single "self"), which
  fall back to "Something".

## Variables and `%substitution%`

Any `%name%` inside a line's argument text is replaced before the line
runs:

| Token | Resolves to |
|---|---|
| `%self%` | the mob/room's own display name, or empty |
| `%actor%` | the triggering being's display name, or empty |
| `%arg%` | the trigger's matched keyword (speech triggers), or empty |
| `%time%` | current Unix time, as a number |
| `%random.N%` | a random integer from 1 to N |
| `%anything_else%` | a local variable (`set`), else a persisted global (`global`), else empty |

Unknown/unset variables resolve to empty string — typo-tolerant, matches
this language's philosophy everywhere else. A `%` with no closing partner
on the same line is left as a literal character.

### `set`/`unset`/`eval` — local variables

Scoped to one script run (survive a `wait` pause within that same run; do
NOT persist across two separate trigger firings).

```
set <name> <value>      -- assigns (value is %var%-substituted first)
unset <name>             -- removes a local variable
eval <name> <expr>       -- computes a simple left-to-right arithmetic
                             expression (+ - * / %) and assigns the result
```

```
set hp 100
eval hp %hp% - 25
echo Remaining: %hp%
```

### `global` — persisted variables

Backed by the `trigger_global_var` DB table — a flat, world-wide key/value
store any trigger's script can read via plain `%name%` substitution.
Useful for one mob's script to leave a flag a completely different mob's
(or room's) script later checks.

```
global shrine_blessed yes
```
```
if %shrine_blessed% == yes
echoroom The shrine still glows with old magic.
end
```

## Control flow

### `if` / `elseif` / `else` / `end`

```
if <expr>
  ...
elseif <expr>
  ...
else
  ...
end
```
`elseif`/`else` are optional; any number of `elseif` blocks are allowed.

### `while` / `done`

```
while <expr>
  ...
done
```
Re-checks `<expr>` each pass; exits once it's false. `break` exits early.

### `switch` / `case` / `default` / `done`

```
switch <value>
case <value1>
  ...
case <value2>
  ...
default
  ...
done
```
Matches `<value>` (after substitution) against each `case` in order.
**Real fallthrough**, same as the original DG Scripts and unlike most
C-family switches: a `case` body with no `break` at its end just keeps
running into the NEXT case's body. Add an explicit `break` to stop there.
`default` runs only if nothing else matched.

### `break`

Exits the nearest enclosing `while` or `switch` immediately.

## Expressions (`if`/`elseif`/`while`/`switch`'s `<expr>`)

- Comparisons: `== != < > <= >=` — numeric if both sides parse as a full
  number, otherwise a string comparison.
- Boolean combinators: `&&` (and), `||` (or) — `||` is lowest precedence,
  evaluated left to right, no parentheses.
- Negation: a leading `!` on any atom.
- A bare atom with no operator is truthy if non-empty and not the literal
  `0`.

```
if %hp% < 20 && %hp% > 0
echo You're badly hurt!
end
```

## Action vocabulary

Unchanged from before this revamp — every argument is now `%var%`-
substituted first, and these can appear inside `if`/`while`/`switch` bodies:

| Command | Effect |
|---|---|
| `echo <text>` | sent to `actor` only (no-op if no live actor — e.g. after `wait`, or in a `random` trigger, which has none) |
| `echoroom <text>` | sent to everyone else in the room (actor excluded) |
| `emote <text>` | `"<self> <text>"` sent to the whole room (actor included) |
| `say <text>` | `"<self> says, '<text>'"` sent to the whole room |
| `teleport <vnum>` | moves `actor` to room `<vnum>` (no-op if no actor) |
| `give <vnum>` | spawns object `<vnum>` into `actor`'s inventory |
| `damage <n>` | deals `<n>` damage to `actor`, never fatal on its own (floors at 1 HP) |
| `log <text>` | a silent, non-broadcast log entry |
| `wait <seconds>` | pauses the REST of the script for 1–3600 real seconds, then resumes (see below) |

## `wait` and what survives it

`wait <seconds>` pauses everything after that line; the pause is real
elapsed time, and resuming is forced by the ~1s pending-trigger pulse (or
immediately, for testing, by an immortal's `aitick`).

**Survives the pause:** the full `set`/`eval`/`global` variable scope, and
the resume point even inside a `while` loop.

**Does NOT survive:** `actor` itself — the triggering being may have
disconnected, moved, or died by the time the script resumes. Only
`say`/`emote`/`echoroom`/`log` make sense after a `wait` (they only need
`room`/`self`, both safely re-derived at resume time); `echo`/`teleport`/
`give`/`damage` silently no-op there, same as if `actor` were `NULL` for
any other reason.

## Managing existing triggers

```
edit trigger list <room|mob|obj> <vnum>   -- shows every trigger on a target, with ids
edit trigger delete <id>                   -- removes one by id
```

## Scope notes (what this is NOT)

This is a full port of DG Scripts' *language core* — not the whole DG
Scripts subsystem. Deliberately not ported:
- DG's hundreds of mob/object/room-specific script commands
  (`dg_mobcmd.c`/`dg_objcmd.c`/`dg_wldcmd.c`) — Tobin keeps its own small,
  fixed action vocabulary above instead.
- `remote`/`context`/`attach` (multi-script cross-targeting).
- `dg_olc.c`'s separate numbered-script-per-vnum authoring model — Tobin
  keeps one script per `edit trigger` row, authored through the same
  shared line editor every other content type uses.
- Full operator-precedence arithmetic in `eval` — it's strictly
  left-to-right (matches how the overwhelming majority of real DG scripts
  use it: a single binary operation).

## Worked example

A shrine that only speaks once per visitor "session" (tracked globally),
and gets ruder the more times anyone has rung its bell:

```
edit trigger room 31750 enter
if %shrine_greeted% == yes
echoroom The shrine ignores you; it has already spoken today.
else
global shrine_greeted yes
set rings 0
global shrine_rings 0
emote hums with ancient light.
end
```

```
edit trigger obj 31751 get
eval rings %shrine_rings% + 1
global shrine_rings %rings%
switch %rings%
case 1
echo The bell rings clear and true.
case 2
echo The bell rings again, a little duller.
default
echo The bell barely makes a sound anymore.
done
```
