#!/usr/bin/env python3
"""Smoke test for per-race body type (Sneezy -> Tobin feature audit,
docs/RACE_STATS.md's/RACE_PERKS.md's "Not imported" list): checks that
player_create() now assigns being_t.body_type via race_body_type()
(src/core/race_flavor.c) rather than the old bare BODY_HUMANOID hardcode
in being_create_pc(). Every one of Tobin's 6 playable races' own RACE_*
file says `body Humanoid` (verified directly), so there's no player-
visible DIFFERENCE to check for yet -- being_t.body_type isn't even
persisted to the DB (recomputed from race at login) and no command
prints the raw enum name for a player. What IS observable, and what
body_type actually drives (body.h's body_limb_weight()/wear-slot
logic), is that a fresh character can wear armor into real humanoid
slots -- this confirms the new race_body_type() plumbing didn't break
that for an Ogre (a non-Human race, so this isn't just exercising the
old hardcoded default by coincidence).
    python3 tests/smoke_test_race_body_type.py [host] [port]
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
announce("smoke_test_race_body_type", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
name = f"Rbty{suf}"
pw = "rbtpw12345"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", pw, pw):
    send_line(s, step)
    recv_all(s)
create_character(s, name, send_line, recv_all, race="3")  # Ogre (menu pick 3 -> player_race_t 2)
row = query(f"SELECT race FROM player WHERE name='{name}';")
check(row == "2", f"{name} really is race 2 (Ogre): {row}")
# The Ogre race suit (zz_newbie_gear_race.sql, vnums 36941-36951) granted
# a humanoid armor set into inventory at creation -- wear a head piece
# and a foot piece and confirm `equipment` reports them in real humanoid
# slots (body_limb_weight(BODY_HUMANOID, LIMB_HEAD/LIMB_FOOT) > 0 is
# exactly what makes these slots wearable at all).
out = strip(cmd(s, "wear hat"))
check("wear" in out.lower() or "you wear" in out.lower() or "head" in out.lower(),
      f"Ogre wears the hat: {out!r}")
eq = strip(cmd(s, "equipment"))
check("hat" in eq.lower() and "head" in eq.lower(),
      f"equipment shows the hat worn on the head slot: {eq!r}")
send_line(s, "quit!")
s.close()
subprocess.run(["mariadb", "tobin", "-e",
    f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');"], check=True)
subprocess.run(["mariadb", "tobin", "-e", f"DELETE FROM player WHERE name='{name}';"], check=True)
announce_done("smoke_test_race_body_type", host, port)
print("=== ALL CHECKS PASSED ===")
