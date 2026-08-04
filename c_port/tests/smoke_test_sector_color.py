#!/usr/bin/env python3
"""Smoke test for sector-based room coloring in `look` (cmd_look.c /
sector_color() in room.c), checked at the raw-byte level (same discipline
as smoke_test_color.py): the room NAME gets the BRIGHT (uppercase) tag for
its sector's color, the DESCRIPTION only the DIM (lowercase) one.

  - A fresh mortal starts at Center Square (vnum 100, sector "TEMPERATE
    CITY") -- colored white (the "CITY" keyword rule in sector_color()),
    also confirming the default-white path renders through the
    name/description split correctly.
  - An immortal defaults to Imperia (vnum 1, sector "DEAD WOODS") -- not
    covered by any specific keyword rule, so it falls to the white default;
    also confirms the builder's `[vnum] name [ SECTOR ] flags` header still
    renders correctly alongside the new name/description coloring.

    python3 tests/smoke_test_sector_color.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_sector_color", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all_bytes(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks)


def make_player(tag, pw="sectorcolortest123"):
    name = f"Sec{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all_bytes(s)
    send_line(s, name); recv_all_bytes(s)
    send_line(s, "y"); recv_all_bytes(s)
    send_line(s, pw); recv_all_bytes(s)
    send_line(s, pw); recv_all_bytes(s)
    send_line(s, "new"); recv_all_bytes(s)
    send_line(s, name); recv_all_bytes(s)
    send_line(s, "1"); recv_all_bytes(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all_bytes(s)  # territory: urban
    send_line(s, "1"); recv_all_bytes(s)  # class: mage
    send_line(s, "done"); recv_all_bytes(s)
    send_line(s, "done"); recv_all_bytes(s)  # alignment: neutral
    return name, s


# --- mortal at Center Square (TEMPERATE CITY -> white) ---
name, s = make_player("M")
raw = recv_all_bytes(s)  # drain anything left from creation's auto-look
send_line(s, "look")
raw = recv_all_bytes(s)
print(f"=== mortal look, color on (raw bytes) ===\n{raw!r}")
check(b"\x1b[1;37m" in raw, "room name uses the bright (uppercase) white escape")
check(b"\x1b[0;37m" in raw, "room description uses the dim (lowercase) white escape")
check(b"<w>" not in raw and b"<W>" not in raw, "no raw sector-color tag leaks through")

send_line(s, "color off")
recv_all_bytes(s)
send_line(s, "look")
raw = recv_all_bytes(s)
check(b"\x1b[" not in raw, "no ANSI escapes at all with color off")
check(b"<w>" not in raw and b"<W>" not in raw, "sector-color tags are stripped, not leaked, with color off")
s.close()

# --- immortal at Imperia (DEAD WOODS -> default white) ---
nameI, si = make_player("I")
recv_all_bytes(si)
# "quit!" leaves to the account menu first (a real disconnect, character
# detached cleanly) -- an abrupt close while still playing would instead
# leave the character linkdead in the ordinary mortal room they were
# created in, bypassing the immortal-defaults-to-room-1 remap below
# (see world_find_linkdead_pc() / enter_world()).
send_line(si, "quit!")
recv_all_bytes(si)
si.close()
# The level change must happen AFTER quit! (which now auto-saves the
# live, pre-change progress via player_save()) and BEFORE reconnecting
# (a fresh player_load()) -- otherwise the auto-save would clobber it.
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")

si = socket.create_connection((host, port), timeout=5)
recv_all_bytes(si)
send_line(si, nameI); recv_all_bytes(si)
send_line(si, "sectorcolortest123"); recv_all_bytes(si)
send_line(si, "1"); recv_all_bytes(si)  # play character #1
raw = recv_all_bytes(si)  # drain any leftover
send_line(si, "look")
raw = recv_all_bytes(si)
print(f"=== immortal look, color on (raw bytes) ===\n{raw!r}")
check(b"[1] " in raw, "immortal header still shows the room vnum")
check(b"DEAD WOODS" in raw, "immortal header still shows the sector name")
check(b"\x1b[1;37m" in raw, "room name uses the bright white escape (DEAD WOODS default)")
check(b"\x1b[0;37m" in raw, "room description uses the dim white escape")
si.close()

announce_done("smoke_test_sector_color", host, port)
print("=== ALL CHECKS PASSED ===")
