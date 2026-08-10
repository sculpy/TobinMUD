#!/usr/bin/env python3
"""Smoke test for the SneezyMUD-derived race stat bonuses (user 2026-08-10:
"should be a conversion in the documents" -- fold the upstream lib/races
12-stat table into Tobin's 6 stats). Creates one character per PC race and
checks the stored attributes equal ATTR_BASE + class + race + territory
bonuses for every stat, and that each race's own bonus nets to zero. See
docs/RACE_STATS.md and being.c's race_stat_bonus().

    python3 tests/smoke_test_race_stats.py [host] [port]
"""
import socket
import subprocess
import sys
import time

from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

ATTR_BASE = 120
STATS = ["strength", "dexterity", "constitution", "intelligence", "wisdom", "charisma"]

# Mirrors of the three C bonus tables (being.c), keyed by the stored
# player.class / player.race / player.territory integer.
CLASS_BONUS = {
    0: {"intelligence": 4, "strength": -4},                                 # MAGE
    1: {"wisdom": 4, "strength": -2, "dexterity": -2},                      # CLERIC
    2: {"constitution": 3, "strength": 3, "charisma": -3, "wisdom": -3},    # WARRIOR
    3: {"dexterity": 4, "strength": -4},                                    # THIEF
    4: {"wisdom": 2, "constitution": 2, "intelligence": -4},               # DRUID
    5: {"strength": 2, "constitution": 2, "charisma": -4},                 # MONK
}
RACE_BONUS = {
    0: {},                                                                                                  # HUMAN
    1: {"strength": -2, "dexterity": 2, "constitution": -6, "intelligence": 3, "wisdom": 5, "charisma": -2},  # ELF
    2: {"strength": 6, "dexterity": -3, "constitution": 3, "intelligence": -2, "wisdom": -3, "charisma": -1}, # OGRE
    3: {"strength": 2, "dexterity": -3, "constitution": 4, "intelligence": -1, "wisdom": -1, "charisma": -1}, # DWARF
    4: {"strength": -4, "dexterity": 6, "constitution": -2, "intelligence": -1, "wisdom": -1, "charisma": 2}, # HOBBIT
    5: {"strength": -3, "dexterity": -3, "constitution": -2, "intelligence": 4, "wisdom": 3, "charisma": 1},  # GNOME
}
TERR_BONUS = {
    0: {"intelligence": 3, "charisma": 3, "constitution": -3, "strength": -3},  # URBAN
    1: {"wisdom": 2, "dexterity": 2, "strength": -2, "charisma": -2},           # RURAL
    2: {"constitution": 3, "strength": 3, "intelligence": -3, "charisma": -3},  # WILDS
}


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def make_char(name, pw, race_pick, class_pick):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, str(race_pick), "1", str(class_pick), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


announce("smoke_test_race_stats", host, port)

# Design invariant: every race bonus is net-zero.
for ridx, rb in RACE_BONUS.items():
    check(sum(rb.values()) == 0, f"race {ridx} bonus nets to zero: {rb}")

suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
for race_pick in range(1, 7):   # creation menu 1..6 -> race enum 0..5
    name, pw = f"Rst{'abcdef'[race_pick-1]}{suf}", "rstpw12345"
    s = make_char(name, pw, race_pick, "1")   # fixed class pick, fixed territory pick
    send_line(s, "quit!")
    s.close()
    row = query(
        "SELECT p.race,p.class,p.territory,"
        "a.strength,a.dexterity,a.constitution,a.intelligence,a.wisdom,a.charisma "
        f"FROM player p JOIN player_attrs a ON a.player_id=p.id WHERE p.name='{name}';")
    check(bool(row), f"{name} created with attrs row")
    vals = [int(x) for x in row.split("\t")]
    race, cls, terr = vals[0], vals[1], vals[2]
    got = dict(zip(STATS, vals[3:9]))
    exp = {st: ATTR_BASE for st in STATS}
    for tbl in (CLASS_BONUS.get(cls, {}), RACE_BONUS.get(race, {}), TERR_BONUS.get(terr, {})):
        for k, v in tbl.items():
            exp[k] += v
    check(got == exp,
          f"race {race} (class {cls}, terr {terr}): attrs {got} == expected {exp}")

announce_done("smoke_test_race_stats", host, port)
print("=== ALL CHECKS PASSED ===")
