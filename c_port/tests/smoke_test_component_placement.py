#!/usr/bin/env python3
"""Smoke test for the wild spell-component placement engine
(src/core/component_placement.c -- the "forage reagents in the wild" half
of Sneezy's obj_component.cc, ported data-driven into the
`component_placement` DB table).

A deterministic test rule (id 9001) is seeded before the server is
(re)booted so component_placement_load() picks it up: PLACE reagent 239
("a rainbow stone") into a single quiet room (1200), chance 100, any hour,
any weather, max_per_room 1.  With those wildcards the rule fires on the
very next placement tick (~60s), so:

  1. After purging the room to a known-empty floor and waiting one tick,
     the reagent has appeared on the floor (the load + hour/weather-
     wildcard match + chance + pick_room + spawn path all worked).
  2. After a second tick the room still holds exactly ONE (max_per_room
     cap honored -- the engine does not pile reagents up).

Run AFTER seeding rule 9001 and rebooting/copyover:
    python3 tests/smoke_test_component_placement.py [host] [port]
The harness seeds/cleans rule 9001 around this test.
"""
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_component_placement", host, port)

import socket

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "cppw123"
TEST_ROOM = 1200
REAGENT = "rainbow stone"
# One placement tick is COMP_PLACEMENT_PULSES = 600 pulses (~60s). Give it
# a little slack so a tick is guaranteed to land inside the wait.
TICK_WAIT = 68


def make_immortal(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in [name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"]:
        send_line(s, step); recv_all(s, 0.7)
    s.close()
    sql("UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


imm = f"Cpim{_sfx}"
make_immortal(imm)
s = login(imm)

# Stand in the target room and clear its floor to a known-empty baseline.
cmd(s, f"goto {TEST_ROOM}")
cmd(s, "purge")
out = cmd(s, "look")
check(REAGENT not in out.lower(),
      "the test room starts with no rainbow stone on the floor (purged)")

# --- 1: one placement tick spawns the reagent onto the floor ---
time.sleep(TICK_WAIT)
out = cmd(s, "look")
check(REAGENT in out.lower(),
      "after one placement tick the reagent has been placed on the floor")

# --- 2: a second tick does NOT pile a second one up (max_per_room cap) ---
time.sleep(TICK_WAIT)
out = cmd(s, "look")
# A stacked pair renders as "(2) a rainbow stone"; a single stays unmarked.
check("(2)" not in out and out.lower().count(REAGENT) == 1,
      "a second tick respects max_per_room=1 (no second reagent piled up)")

# Leave the room clean for the next run.
cmd(s, "purge")
s.close()

announce_done("smoke_test_component_placement", host, port)
print("PASS: smoke_test_component_placement")
