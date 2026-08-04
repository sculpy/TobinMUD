#!/usr/bin/env python3
"""Smoke test for the account color preference (descriptor.c CONN_GET_COLOR_PREF,
account_repo.c, cmd_color.c):
  1. A new account is asked about color; answering 'n' disables it, and the
     `color` command confirms it is OFF.
  2. The preference persists across a reconnect (account.color_pref).
  3. `color on` re-enables it and also persists.

    python3 tests/smoke_test_color_pref.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_color_pref", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


name = f"Colp{_suffix}"

# --- create the account, decline color, make a character ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)          # new account name
send_line(s, "y"); recv_all(s)           # confirm new account creation
send_line(s, "colppw"); recv_all(s)      # new password
out = cmd(s, "colppw")                    # confirm password -> color prompt
check("Enable it?" in out or "color" in out.lower(),
      "a new account is asked about color at creation")
out = cmd(s, "n")                          # decline color
check("disabled" in out.lower(), "answering 'n' disables color")
send_line(s, ""); recv_all(s)              # accept the (none) time zone default
# now at account menu -> create a character
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)         # in game now
send_line(s, "done"); recv_all(s)  # alignment: neutral

check("currently OFF" in cmd(s, "color"), "color reports OFF in game after declining")
s.close()

# --- reconnect: preference must have persisted ---
def relogin():
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, name); recv_all(r)
    send_line(r, "colppw"); recv_all(r)
    send_line(r, "1"); recv_all(r)         # connect the only character
    return r


s = relogin()
check("currently OFF" in cmd(s, "color"), "color preference persisted as OFF across reconnect")

# --- turn it on; it should persist too ---
check("now ON" in cmd(s, "color on"), "color on re-enables color")
s.close()
s = relogin()
check("currently ON" in cmd(s, "color"), "the color-on choice persisted across reconnect")
s.close()

announce_done("smoke_test_color_pref", host, port)
print("=== ALL CHECKS PASSED ===")
