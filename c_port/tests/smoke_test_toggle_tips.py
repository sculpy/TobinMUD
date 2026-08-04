#!/usr/bin/env python3
"""Regression test for a user bug report: "tips channel should be a
toggle to shut it off or turn it on again". Before this, the only way to
silence the periodic pulse-driven tip echo (tips_repo.c's
tips_pulse_tick()) was `toggle newbie`, which also drops the player off
the newbie help channel entirely (cmd_newbie.c) -- a much bigger side
effect than "stop showing me tips". Fixed with a dedicated PLR_NOTIPS
pflag bit (being.h) and a `toggle tips` switch (cmd_toggle.c),
independent of PLR_NEWBIE.

Covers:
  1. Bare `toggle` lists "tips" and it defaults to on.
  2. `toggle tips` flips it off; the pflags bit persists to the DB.
  3. `toggle tips` again flips it back on; the bit clears.
  4. Turning `toggle newbie` off does NOT affect the tips switch's value
     -- confirms the two are genuinely independent bits, not one flag
     wearing two names.

    python3 tests/smoke_test_toggle_tips.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_toggle_tips", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql_scalar(stmt):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
                          check=True, capture_output=True, text=True)
    return out.stdout.strip()


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "togtippw"); recv_all(s)
    send_line(s, "togtippw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


name = f"Togtip{_suffix}"
s = make_char(name)

out = strip(cmd(s, "toggle"))
check("tips" in out, "bare toggle lists the tips switch")
check(re.search(r"tips\s+on\s", out), "tips defaults to on")

out = strip(cmd(s, "toggle tips"))
check("tips is now off" in out, "toggle tips flips it off")

pflags = int(sql_scalar(
    f"SELECT pflags FROM player WHERE name='{name}';"))
check((pflags & 32) != 0, "PLR_NOTIPS bit (32) is set in the DB after toggling off")

out = strip(cmd(s, "toggle tips"))
check("tips is now on" in out, "toggle tips flips it back on")

pflags = int(sql_scalar(
    f"SELECT pflags FROM player WHERE name='{name}';"))
check((pflags & 32) == 0, "PLR_NOTIPS bit (32) is cleared in the DB after toggling back on")

# --- independence from `toggle newbie` ---
out = strip(cmd(s, "toggle newbie"))
check("newbie is now off" in out, "toggle newbie flips the (separate) newbie-channel switch off")

out = strip(cmd(s, "toggle"))
check(re.search(r"tips\s+on\s", out), "tips is still on -- unaffected by turning the newbie channel off")

s.close()
announce_done("smoke_test_toggle_tips", host, port)
print("=== ALL CHECKS PASSED ===")
