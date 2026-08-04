#!/usr/bin/env python3
"""Smoke test for the multiplay gate.

  1. Default off: a mortal account's second connected character is refused.
  2. A 59+ immortal can `multiplay on`, after which the second mortal
     character connects.
  3. `multiplay` is hidden from mortals.

    python3 tests/smoke_test_multiplay.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_multiplay", host, port)

# Defensive reset for a prior run that errored out before restoring this
# (the multiplay flag persists in the `game_config` DB table across a
# server restart, multiplay.c's multiplay_load()/multiplay_set() -- NOT
# just an in-memory default, so a crashed earlier run can leave it stuck
# "on" indefinitely, silently defeating this test's own "default off"
# assumption on every later run until manually reset).
subprocess.run(["mariadb", "tobin", "-e",
                "UPDATE game_config SET value='off' WHERE name='multiplay';"], check=True)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


acct = f"Mpacct{_suffix}"
pw = "mppw"
char1, char2 = f"Mpone{_suffix}", f"Mptwo{_suffix}"

# --- Build an account with two mortal characters ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, acct); recv_all(s)
send_line(s, "y"); recv_all(s)        # confirm new account creation
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)         # confirm password
send_line(s, "new"); recv_all(s)
send_line(s, char1); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)     # playing char1
send_line(s, "done"); recv_all(s)  # alignment: neutral
send_line(s, "quit!"); recv_all(s)    # -> account menu
send_line(s, "new"); recv_all(s)
send_line(s, char2); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)     # playing char2
send_line(s, "done"); recv_all(s)  # alignment: neutral
send_line(s, "quit!"); recv_all(s)    # -> account menu
s.close()


def login(char_index):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, acct); recv_all(r)
    send_line(r, pw); recv_all(r)
    return r, cmd(r, str(char_index))


# --- Default off: second character refused ---
s1, out1 = login(1)
check("Welcome" in out1 or "Center Square" in out1, "char1 connects normally")
s2, out2 = login(2)
check("multiplaying is not allowed" in out2,
      "with multiplay off, the account's second character is refused")
s2.close()

# --- A 59 immortal turns multiplay on ---
immname = f"Mpimm{_suffix}"
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
for step in (immname, "y", "mppw", "mppw", "new", immname, "1", "1", "1", "done", "done"):
    send_line(si, step); recv_all(si)
si.close()
subprocess.run(["mariadb", "tobin", "-e",
                f"UPDATE player_progress SET level=59 WHERE player_id="
                f"(SELECT id FROM player WHERE name='{immname}');"], check=True)
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
send_line(si, immname); recv_all(si)
send_line(si, "mppw"); recv_all(si)
send_line(si, "1"); recv_all(si)
check("now ON" in cmd(si, "multiplay on"), "a 59 immortal turns multiplay on")

# --- Now the second character connects ---
s2b, out2b = login(2)
check("Welcome" in out2b or "Center Square" in out2b,
      "with multiplay on, the second character connects")

# --- gate: mortals can't multiplay ---
sm = socket.create_connection((host, port), timeout=5)
recv_all(sm)
mort = f"Mpmort{_suffix}"
for step in (mort, "y", "mppw", "mppw", "new", mort, "1", "1", "1", "done", "done"):
    send_line(sm, step); recv_all(sm)
check("Command not found" in cmd(sm, "multiplay off"), "multiplay is hidden from mortals")

# restore default off
cmd(si, "multiplay off")

s1.close(); s2b.close(); si.close(); sm.close()
announce_done("smoke_test_multiplay", host, port)
print("=== ALL CHECKS PASSED ===")
