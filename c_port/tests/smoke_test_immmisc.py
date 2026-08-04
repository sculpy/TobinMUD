#!/usr/bin/env python3
"""Smoke test for a few immortal conveniences:
  1. `goto <player>` teleports to that online player's room (not just a vnum).
  2. `help edit` documents the unified `edit <noun>` dispatcher.
  3. who/score tint an immortal's name by rank tier (color escape present for
     an immortal, absent for a mortal).

    python3 tests/smoke_test_immmisc.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_immmisc", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "y", "immpw", "immpw", "new", nm, "1", "1", "1", "done", "done"):
        send_line(s, step); recv_all(s)
    return s


def promote(nm, level):
    subprocess.run(["mariadb", "tobin", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{nm}');"], check=True)


def relogin(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "immpw", "1"):
        send_line(s, step); recv_all(s)
    return s


nameA, nameB, nameC = f"Imma{_suffix}", f"Immb{_suffix}", f"Immc{_suffix}"
make_char(nameA).close(); make_char(nameB).close()
promote(nameA, 59); promote(nameB, 51)
A = relogin(nameA)
B = relogin(nameB)
C = make_char(nameC)          # mortal
recv_all(A); recv_all(B); recv_all(C)

# --- goto <player> ---
cmd(A, "goto 100")            # A to Center Square
cmd(A, "color off")
cmd(B, "color off")
out = cmd(B, f"goto {nameA}")
check("Center Square" in out, "goto <player> teleports to that player's room")

# --- help edit (2026-07-11: ed* commands unified into one `edit <noun>`
# dispatcher; "help edit" is now a normal DB-backed topic describing it,
# not the old live index of standalone ed* commands) ---
out = cmd(A, "help edit")
check("edit room" in out and "edit player" in out, "help edit documents the edit <noun> dispatcher")

# --- rank color in score (color ON) ---
cmd(A, "color on")
out = cmd(A, "score")
check("\x1b[" in out, "an immortal's score name carries a rank color (ANSI escape present)")

cmd(C, "color on")
outC = cmd(C, "score")
check("\x1b[" not in outC, "a mortal's score has no rank color")

A.close(); B.close(); C.close()
announce_done("smoke_test_immmisc", host, port)
print("=== ALL CHECKS PASSED ===")
