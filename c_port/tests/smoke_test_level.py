#!/usr/bin/env python3
"""Smoke test for the new `level` command (cmd_level.c, user 2026-07-19:
"a level command that will display when your due for a gain in level,
You have X experience and need X experience to level"). Pulled the XP
math out into its own line rather than adding to `score`'s already-long
dump -- reuses progress_xp_for_level() (being.c), the SAME curve
progress_add_xp() levels a player up against.

Covers:
  1. A fresh level-1 character sees "You have 0 experience and need
     <N> more experience to level," where N matches
     progress_xp_for_level(2) (37, the real upstream XP-to-level table
     wired into being.c 2026-07-28 -- was 400 under the old level^2*100
     placeholder when this test was first written).
  2. After SQL-granting experience partway to level 2, the "need" number
     shrinks by exactly the granted amount.
  3. A level-50 (MORTAL_LEVEL_MAX) character gets the "already at the
     maximum mortal level" message instead of a nonsensical/negative
     "need" number.
  4. An immortal (level 51+) gets the "don't gain levels through
     experience" message instead.

    python3 tests/smoke_test_level.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_level", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(nm, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(nm, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


# --- fresh level-1 character ---
name, pw = f"Lvl{_suffix}", "lvlpw12345"
s = make_char(name, pw)

out = strip(cmd(s, "level"))
m = re.search(r"You have (\d+) experience and need (\d+) more experience to level\.", out)
check(m, "level shows the 'have X, need Y' message")
have, need = int(m.group(1)), int(m.group(2))
check(have == 0, "a fresh character has 0 experience")
check(have + need == 37, "need matches progress_xp_for_level(2) == 37 (real upstream table)")

# --- grant partial XP, need should shrink by exactly that much ---
# Stays comfortably below progress_xp_for_level(2)=37 so the character
# doesn't actually level up from the raw SQL grant (which bypasses
# progress_add_xp()'s own level-up bookkeeping).
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET experience=20 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s = relog(name, pw)

out = strip(cmd(s, "level"))
m = re.search(r"You have (\d+) experience and need (\d+) more experience to level\.", out)
check(m, "level still shows the message after gaining experience")
have2, need2 = int(m.group(1)), int(m.group(2))
check(have2 == 20, "have reflects the granted experience")
check(need2 == 17, "need shrank by exactly the granted amount (37-20=17)")

cmd(s, "quit!"); s.close()

# --- level 50 (MORTAL_LEVEL_MAX): no more leveling via XP ---
sql(f"UPDATE player_progress SET level=50, experience=999999 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s = relog(name, pw)
out = strip(cmd(s, "level"))
check("already at the maximum mortal level" in out, "level 50 gets the max-level message, not a need number")
cmd(s, "quit!"); s.close()

# --- immortal: no XP-driven leveling at all ---
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s = relog(name, pw)
out = strip(cmd(s, "level"))
check("don't gain levels through experience" in out, "an immortal gets the immortal-specific message")
cmd(s, "quit!"); s.close()

announce_done("smoke_test_level", host, port)
print("=== ALL CHECKS PASSED ===")
