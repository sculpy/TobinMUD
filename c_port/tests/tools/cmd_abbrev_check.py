#!/usr/bin/env python3
"""Guard rail for reordering src/cmd/cmd_table.c.

WHY THIS EXISTS
---------------
That table's ORDER IS ITS SEMANTICS. cmd_dispatch() dispatches to the FIRST
entry the caller's level lets them see whose name STARTS WITH the verb they
typed, so moving a line silently rewires which command owns every abbreviation
it shares with another. That exact class of mistake bit the table three times:
`set` landing on `settrap`, `g` reaching `goto` instead of `get`, and the
per-entry comments quietly drifting out of sync with reality (they claimed "c"
reached `color` and "h" reached `help`, when `close` and `hit` had taken both).

Eyeballing a 113-entry table for this does not work. Diff it mechanically.

USAGE
-----
    # Did a reorder change any abbreviation? (the important one)
    python3 cmd_abbrev_check.py old_cmd_table.c new_cmd_table.c

    # What is each command's shortest reachable abbreviation, and who
    # shadows it on the shorter prefixes?
    python3 cmd_abbrev_check.py cmd_table.c

Get the "old" copy from git:
    git show HEAD:c_port/src/cmd/cmd_table.c > /tmp/old_cmd_table.c

Exits 0 if the two tables resolve every prefix identically, 1 otherwise. A
reorder meant as a pure refactor MUST exit 0 -- the 2026-07-17 alphabetization
pass did.
"""
import re
import sys

# Level macros used in the table's min_level column, resolved to the numbers
# being.h / cmd_internal.h give them. A macro missing here is a hard error
# rather than a guess, so a newly-added command can't silently skip checking.
LEVELS = {
    "MORTAL_LEVEL_MIN": 1,
    "IMMORTAL_LEVEL_MIN": 51,
    "BUILD_MIN_LEVEL": 51,
    "PURGE_MIN_LEVEL": 51,
    "LOG_MIN_LEVEL": 54,
    "STAT_MIN_LEVEL": 55,
    "TEST_MIN_LEVEL": 58,
    "PROMOTE_MIN_LEVEL": 58,
    "USERS_MIN_LEVEL": 58,
    "GAMETOG_MIN_LEVEL": 58,
    "SET_MIN_LEVEL": 58,
    "COPYOVER_MIN_LEVEL": 59,
    "DELBUG_MIN_LEVEL": 59,
    "EDBUG_MIN_LEVEL": 59,
    "DELIDEA_MIN_LEVEL": 59,
    "DELTYPO_MIN_LEVEL": 59,
    "SNOOP_MIN_LEVEL": 59,
    "MULTIPLAY_MIN_LEVEL": 59,
    "EXEC_MIN_LEVEL": 60,
    "BALANCE_MIN_LEVEL": 60,
    "EGOTRIP_MIN_LEVEL": 60,
    "TIPEDIT_MIN_LEVEL": 53,
    "WIPE_MIN_LEVEL": 59,
    "SHUTDOWN_MIN_LEVEL": 60,
    "POSSESS_MIN_LEVEL": 59,
    "LOADSUIT_MIN_LEVEL": 56,
}

# { "name", cmd_fn, "help text" | NULL, LEVEL_MACRO },
_ENTRY = re.compile(
    r'^\s*\{\s*"([a-z!]+)"\s*,\s*(\w+)\s*,\s*(?:NULL|".*?")\s*,\s*(\w+)\s*\}\s*,',
    re.MULTILINE,
)


def parse(path):
    """Every COMMANDS[] row as (name, handler, min_level), in table order."""
    with open(path, encoding="utf-8", errors="replace") as f:
        src = f.read()
    try:
        start = src.index("static const cmd_entry_t COMMANDS[]")
        end = src.index("#define NUM_COMMANDS", start)
    except ValueError:
        sys.exit("%s: no COMMANDS[] table found" % path)
    out = []
    for m in _ENTRY.finditer(src[start:end]):
        name, fn, lvl = m.group(1), m.group(2), m.group(3)
        if lvl not in LEVELS:
            sys.exit("%s: unknown level macro %r on command %r -- add it to "
                     "LEVELS in this script" % (path, lvl, name))
        out.append((name, fn, LEVELS[lvl]))
    if not out:
        sys.exit("%s: COMMANDS[] parsed as empty" % path)
    return out


def resolve(table, verb, level):
    """The (name, handler) cmd_dispatch() lands on, or None for 'not found'."""
    for name, fn, min_level in table:
        if min_level > level:
            continue
        if name.startswith(verb):
            return name, fn
    return None


def all_prefixes(table):
    """Every string a player could type that reaches some command."""
    verbs = set()
    for name, _, _ in table:
        for i in range(1, len(name) + 1):
            verbs.add(name[:i])
    return verbs


def test_levels():
    """Every level at which the visible command set can differ."""
    return sorted({1, 50} | set(LEVELS.values()))


def cmd_diff(old_path, new_path):
    old, new = parse(old_path), parse(new_path)

    if sorted(n for n, _, _ in old) != sorted(n for n, _, _ in new):
        print("!! ENTRY SET CHANGED -- a reorder must not add or drop commands")
        for n in sorted(set(n for n, _, _ in old) ^ set(n for n, _, _ in new)):
            print("   present on only one side: %s" % n)
        return 1

    verbs = all_prefixes(old)
    diffs = []
    for level in test_levels():
        for verb in sorted(verbs):
            a, b = resolve(old, verb, level), resolve(new, verb, level)
            if a != b:
                diffs.append((level, verb, a, b))

    if not diffs:
        print("OK: all %d prefixes resolve identically at every level %s"
              % (len(verbs), test_levels()))
        return 0

    show = lambda r: "not found" if r is None else "%s (%s)" % (r[0], r[1])
    print("%d abbreviation change(s) -- intended?\n" % len(diffs))
    for level, verb, a, b in diffs:
        print("  lvl %-2d  %-10r  %-28s -> %s" % (level, verb, show(a), show(b)))
    return 1


def cmd_report(path):
    """Shortest working abbreviation per command, and what shadows it."""
    table = parse(path)
    print("%-12s %-8s %-8s  %s" % ("command", "mortal", "lvl60", "shadowed by"))
    print("-" * 66)
    for name, _, ml in table:
        cols = []
        for level in (1, 60):
            got = "-"
            if ml <= level:
                for i in range(1, len(name) + 1):
                    r = resolve(table, name[:i], level)
                    if r and r[0] == name:
                        got = name[:i]
                        break
                else:
                    got = "!! UNREACHABLE"
            cols.append(got)
        blockers = []
        short = cols[1]
        if short not in ("-", "!! UNREACHABLE"):
            for k in range(1, len(short)):
                r = resolve(table, name[:k], 60)
                if r and r[0] != name and r[0] not in blockers:
                    blockers.append(r[0])
        print("%-12s %-8s %-8s  %s" % (name, cols[0], cols[1], ", ".join(blockers)))
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 3:
        sys.exit(cmd_diff(sys.argv[1], sys.argv[2]))
    if len(sys.argv) == 2:
        sys.exit(cmd_report(sys.argv[1]))
    sys.exit(__doc__)
