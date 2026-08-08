#!/usr/bin/env python3
"""Smoke test for the command separator (user, 2026-08-08: "implement a
command seperator in the game"). `;` splits one typed line into several
commands run in order ("north;look;inventory"), the classic Diku/Sneezy
convention. This freed up `;` from its previous job as a leading-character
shorthand for `wiznet` (cmd_table.c) -- dropped on the user's own call
("use ';' anyway, drop the wiznet shorthand" / "if you need the ; wizards
can use alias to substitute").

Covers:
  1. Two commands chained with `;` both run, in order.
  2. Each piece re-enters cmd_dispatch() fresh, so a leading `'`
     (say-shorthand) still works per-piece.
  3. The old bare `;<message>` wiznet shorthand no longer works (falls
     through to "Command not found").
  4. `wiznet` itself (typed in full) still works normally.
  5. `help separator` and `help wiznet` both exist and read correctly
     (wiznet's help text no longer claims the retired shorthand).

    python3 tests/smoke_test_separator.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


announce("smoke_test_separator", host, port)

name, pw = f"Sepr{_suffix}", "seprpw123456"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step)
    recv_all(s)
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, pw, "1"):
    send_line(s, step)
    recv_all(s)

# --- 1: two chained commands both run ---
out = strip(cmd(s, "score;inventory", timeout=1.5))
check("Name:" in out and name in out, "the first chained command (score) ran")
check("You are carrying" in out, "the second chained command (inventory) ran")

# --- 2: leading ' still works per-segment ---
out = strip(cmd(s, "'hi;'bye", timeout=1.5))
check('You say, "hi"' in out, "the first say-shorthand segment ran")
check('You say, "bye"' in out, "the second say-shorthand segment ran")

# --- 3: the old bare `;<message>` wiznet shorthand is gone ---
out = strip(cmd(s, ";hello immortals", timeout=1.5))
check("Command not found" in out, "a leading ';' no longer works as a wiznet shorthand")

# --- 4: wiznet typed in full still works ---
out = strip(cmd(s, "wiznet still here", timeout=1.5))
check("still here" in out, "`wiznet <message>` typed in full still works")

# --- 5: help topics ---
out = strip(cmd(s, "help separator", timeout=1.5))
check("chains several commands" in out, "`help separator` exists and describes the feature")

out = strip(cmd(s, "help wiznet", timeout=1.5))
check("retired" in out.lower() and "separator" in out.lower(),
      "`help wiznet` no longer advertises the retired ';' shorthand and points at HELP SEPARATOR")
s.close()

announce_done("smoke_test_separator", host, port)
print("=== ALL CHECKS PASSED ===")
