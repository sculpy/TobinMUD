#!/usr/bin/env python3
"""Smoke test for per-race height/weight/age (Sneezy -> Tobin feature
audit, docs/RACE_STATS.md's/RACE_PERKS.md's "Not imported" list). Creates
one character per PC race (default GENDER_NEUTER, so this exercises the
female height/weight dice -- race_roll_height()/race_roll_weight() only
branch on GENDER_MALE) and checks the persisted player.height/weight/
start_age columns land inside that race's own SneezyMUD dice range
(sneezymud-master/lib/races/RACE_*), and that `score` displays a
Height/Weight line and an Age that matches start_age (year 0 elapsed).
    python3 tests/smoke_test_race_flavor_height_weight_age.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, cmd, check, announce, announce_done
from mud_creation import create_character
host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)
def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()
# (age_min, age_max), (ht_min, ht_max), (wt_min, wt_max) -- female dice,
# since GENDER_NEUTER (the creation default) uses the femaleHt/femaleWt
# row (race_roll_height()/race_roll_weight(), src/core/race_flavor.c).
RANGES = {
    1: ((15+1, 15+4), (60+1, 60+12), (100+4, 100+40)),      # HUMAN
    2: ((100+5, 100+30), (44+1, 44+11), (70+3, 70+30)),     # ELF (RACE_WOODELF)
    3: ((25+1, 25+4), (82+1, 82+11), (250+12, 250+144)),    # OGRE
    4: ((40+5, 40+30), (38+1, 38+9), (105+4, 105+40)),      # DWARF
    5: ((20+3, 20+12), (28+1, 28+5), (18+5, 18+20)),        # HOBBIT
    6: ((60+3, 60+36), (30+1, 30+8), (68+1, 68+7)),         # GNOME
}
announce("smoke_test_race_flavor_height_weight_age", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
for race_pick in range(1, 7):
    name = f"Rfh{'abcdef'[race_pick-1]}{suf}"
    pw = "rfhpw12345"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, race=str(race_pick))
    row = query(
        f"SELECT p.race, p.height, p.weight, p.start_age FROM player p WHERE p.name='{name}';")
    check(bool(row), f"{name} created with a player row")
    race, height, weight, start_age = [int(x) for x in row.split("\t")]
    check(race == race_pick - 1, f"{name} stored race {race} == expected {race_pick - 1}")
    age_r, ht_r, wt_r = RANGES[race_pick]
    check(age_r[0] <= start_age <= age_r[1],
          f"{name} (race {race}) start_age {start_age} in [{age_r[0]},{age_r[1]}]")
    check(ht_r[0] <= height <= ht_r[1],
          f"{name} (race {race}) height {height} in [{ht_r[0]},{ht_r[1]}]")
    check(wt_r[0] <= weight <= wt_r[1],
          f"{name} (race {race}) weight {weight} in [{wt_r[0]},{wt_r[1]}]")
    out = strip(cmd(s, "score"))
    check("Height:" in out and "Weight:" in out, f"{name} score shows Height/Weight")
    m = re.search(r"Age:\s*(\d+) years old", out)
    check(bool(m) and int(m.group(1)) == start_age,
          f"{name} score Age {m.group(1) if m else '?'} == start_age {start_age} (no real time elapsed yet)")
    send_line(s, "quit!")
    s.close()
announce_done("smoke_test_race_flavor_height_weight_age", host, port)
print("=== ALL CHECKS PASSED ===")
