#!/usr/bin/env python3
"""Smoke test for `toggle` (cmd_toggle.c) -- PERSONAL switches only:
  1. Bare `toggle` lists the player switches (color, hp) with values.
  2. `toggle hp` flips the hit-points-in-prompt switch.
  3. `toggle` never lists or accepts the global game toggle (multiplay),
     for anyone, at any level -- it moved entirely to the separate
     `gametog` (58+) command (see smoke_test_gametog.py) rather than just
     being hidden by level within `toggle`.

    python3 tests/smoke_test_toggle.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_toggle", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "togpw"); recv_all(s)
    send_line(s, "togpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "togpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# --- mortal ---
nameM = f"Togm{_suffix}"
s = make_char(nameM)

out = strip(cmd(s, "toggle"))
check("color" in out and "hp" in out, "bare toggle lists the player switches")
check("multiplay" not in out, "a mortal does not see the game toggle")

check("hp is now on" in strip(cmd(s, "toggle hp")), "toggle hp flips it on")
check("hp           on" in strip(cmd(s, "toggle")), "the hp switch now reads on")
check("No such toggle" in strip(cmd(s, "toggle multiplay")),
      "toggle no longer recognizes 'multiplay' by name at all")

s.close()

# --- toggle never shows/accepts multiplay even at 58+ (it moved to
# gametog entirely, not just hidden below some level within toggle) ---
nameI = f"Togi{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=58 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
si = relogin(nameI)

out = strip(cmd(si, "toggle"))
check("multiplay" not in out, "toggle doesn't list multiplay even for a 58+ immortal")
check("No such toggle" in strip(cmd(si, "toggle multiplay")),
      "toggle doesn't accept 'multiplay' even for a 58+ immortal")

si.close()
announce_done("smoke_test_toggle", host, port)
print("=== ALL CHECKS PASSED ===")
