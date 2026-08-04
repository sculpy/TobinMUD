#!/usr/bin/env python3
"""Smoke test for `snoop <name>` (59+, user 2026-07-11: "implement a snoop
command like sneezy, the command should be 59+ where you cant snoop
anyone of same or higher level").

  1. `snoop` is hidden from a 58 immortal (below 59).
  2. A 59+ snooper cannot snoop someone of equal or higher level ("You
     failed.").
  3. A 59+ snooper CAN snoop a lower-level mortal: their own typed
     command line is mirrored (prefixed "% "), and so is what they see in
     response.
  4. Bare `snoop` (no argument) stops the snoop, same as `snoop
     <yourself>` -- nothing more is mirrored after.

    python3 tests/smoke_test_snoop.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_snoop", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Snoopimm{_suffix}"
imm_pw = "snoopimmpw123"
mort_name = f"Snoopmort{_suffix}"
mort_pw = "snoopmortpw123"
peer_name = f"Snooppeer{_suffix}"
peer_pw = "snooppeerpw123"

# --- mortal target (level 1, well below the snooper) ---
sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
sm.close()
sm = login(mort_name, mort_pw)

# --- a peer immortal (59), to prove same-level snooping fails ---
sp = socket.create_connection((host, port), timeout=5)
make_char(sp, peer_name, peer_pw)
set_level(peer_name, 59)
sp.close()
sp = login(peer_name, peer_pw)

# --- the snooper, starting at 58 (below the gate) ---
si = socket.create_connection((host, port), timeout=5)
make_char(si, imm_name, imm_pw)
set_level(imm_name, 58)
si.close()
si = login(imm_name, imm_pw)
check("Command not found" in cmd(si, f"snoop {mort_name}"), "snoop is hidden below level 59")

# --- promote to 59 ---
set_level(imm_name, 59)
si.close()
si = login(imm_name, imm_pw)

# --- bare snoop while not snooping anyone yet ---
out = cmd(si, "snoop")
check("just snoop yourself" in out, "bare snoop with no active snoop just snoops yourself")

# --- cannot snoop an equal-level peer ---
out = cmd(si, f"snoop {peer_name}")
check("You failed" in out, "a 59 immortal cannot snoop an equal-level peer")

# --- snoop the mortal (well below 59) ---
out = cmd(si, f"snoop {mort_name}")
check("now snooping" in out, "a 59 immortal can snoop a lower-level mortal")

# --- the mortal's typed command AND its response are both mirrored ---
cmd(sm, "score")
mirrored = recv_all(si, timeout=1.0)
check("% score" in mirrored, "the snooped mortal's typed command is mirrored, prefixed '%'")
check("Level:" in mirrored or "HP:" in mirrored,
      "the snooped mortal's own response output is ALSO mirrored to the snooper")

# --- stop snooping with a bare `snoop` (no argument) ---
out = cmd(si, "snoop")
check("stop snooping" in out, "bare snoop (no argument) stops the snoop")

cmd(sm, "look")
after = recv_all(si, timeout=1.0)
check("% look" not in after, "nothing more is mirrored after snoop stops")

sm.close()
sp.close()
si.close()
announce_done("smoke_test_snoop", host, port)
print("=== ALL CHECKS PASSED ===")
