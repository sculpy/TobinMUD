#!/usr/bin/env python3
"""Smoke test for per-race move verbs (Sneezy -> Tobin feature audit,
docs/RACE_STATS.md's/RACE_PERKS.md's "Not imported" list): checks that an
Ogre's default arrival/departure text reads "lumbers in"/"lumbers <dir>"
and a Human's still reads the old "has arrived"/"exits <dir>" -- straight
off each race's own RACE_* moveIn/moveOut field (race_move_verb_in()/
race_move_verb_out(), src/core/race_flavor.c), observed by a second
character standing in the same room.
    python3 tests/smoke_test_race_move_verbs.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, cmd, check, announce, announce_done, drain
from mud_creation import create_character
host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)
def new_char(name, pw, race_pick):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, race=str(race_pick))
    return s
announce("smoke_test_race_move_verbs", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
# HUMAN (race 1 at creation) stays put and watches; OGRE (race 3) walks
# out then back in. Center Square (room 100, the default load room) has a
# "north" exit (Tobin's own default landing spot -- used by every other
# creation-flow test too) so a plain "north"/"south" round trip works
# without needing to know the wider room graph.
watcher = new_char(f"Rmvw{suf}", "rmvpw12345", 1)   # Human, watches
drain(watcher)
mover = new_char(f"Rmvo{suf}", "rmvpw12345", 3)     # Ogre, moves
drain(mover)
drain(watcher)
cmd(mover, "north")  # own output is just the new room description, not the departure echo
seen = strip(recv_all(watcher, timeout=1.5))
check("lumbers to the north" in seen, f"watcher sees Ogre lumber out: {seen!r}")
cmd(mover, "south")
seen = strip(recv_all(watcher, timeout=1.5))
check("lumbers in" in seen, f"watcher sees Ogre lumber back in: {seen!r}")
# Human mover round-trip: still the old wording, unchanged by this feature.
drain(mover)
send_line(watcher, "north")
drain(watcher)
seen2 = strip(recv_all(mover, timeout=1.5))
check("exits to the north" in seen2, f"mover sees Human's default 'exits to the north': {seen2!r}")
send_line(watcher, "south")
seen3 = strip(recv_all(mover, timeout=1.5))
check("has arrived" in seen3, f"mover sees Human 'has arrived' (unchanged default wording): {seen3!r}")
for s in (watcher, mover):
    send_line(s, "quit!")
    s.close()
announce_done("smoke_test_race_move_verbs", host, port)
print("=== ALL CHECKS PASSED ===")
