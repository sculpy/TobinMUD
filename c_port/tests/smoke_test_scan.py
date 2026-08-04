#!/usr/bin/env python3
"""Smoke test for `scan [direction|name]` (cmd_scan.c):
  1. With a target standing in an adjacent room, `scan` reports it tagged
     with the direction it lies in.
  2. `scan <direction>` reports only that direction.
  3. `scan <name>` filters to beings whose name matches.
  4. A linkdead target is no longer reported (Session 43 continued, user:
     "scan should ignore linkdead chars").

    python3 tests/smoke_test_scan.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_scan", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_player(tag):
    name = f"Scn{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "scanpw123"); recv_all(s)
    send_line(s, "scanpw123"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s, name


sScan, nameScan = make_player("a")   # the one scanning
sTgt, nameTgt = make_player("b")     # the target, will step into the next room
cmd(sScan, "color off")  # so the [Exits:] regex below isn't broken up by ANSI codes

# Find a real exit out of the shared start room.
outA = cmd(sScan, "look")
m = re.search(r"\[Exits:\]\s*([A-Za-z ]+)", outA)
check(bool(m and m.group(1).split()), "the start room has at least one obvious exit")
direction = m.group(1).split()[0].lower()  # the [Exits:] list is capitalized; lowercase to match scan's own text

# Target walks one room in that direction; now it's one room away.
cmd(sTgt, direction)
recv_all(sScan)  # drain the target's departure echo

# --- scan (all directions) sees the target, tagged with the direction ---
out = cmd(sScan, "scan").lower()
check(nameTgt.lower() in out, "scan reports the target standing in the adjacent room")
check(f"to the {direction}" in out, "scan tags the target with the direction it lies in")

# --- scan <direction> reports it too ---
out = cmd(sScan, f"scan {direction}").lower()
check(nameTgt.lower() in out, "scan <direction> reports the target down that exit")

# --- scan <name> filters to the matching being ---
out = cmd(sScan, f"scan {nameTgt}").lower()
check(nameTgt.lower() in out, "scan <name> finds the matching being")

# --- linkdead target is no longer reported (user: "scan should ignore
# linkdead chars") -- an abrupt close (not `quit!`) leaves the character
# linkdead in place rather than removing it. ---
sTgt.close()
time.sleep(0.5)
out = cmd(sScan, "scan").lower()
check(nameTgt.lower() not in out, "scan no longer reports the target once they go linkdead")

sScan.close()
announce_done("smoke_test_scan", host, port)
print("=== ALL CHECKS PASSED ===")
