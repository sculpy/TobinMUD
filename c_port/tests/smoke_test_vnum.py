#!/usr/bin/env python3
"""Smoke test for `show <room|obj|mob> <pattern>` (cmd_show.c,
renamed from `vnum`, user 2026-08-22):
  1. Lists obj/mob/room vnums whose name matches a substring.
  2. Bad/empty usage is rejected; a no-match pattern says "none".
  3. Builder-gated (51+): a mortal can't see it (Command not found).

    python3 tests/smoke_test_vnum.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_vnum", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def set_level(name, level):
    subprocess.run(["mariadb", "tobin", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{name}');"], check=True)


def vnum_read(sock, line):
    """Run a vnum command and fully drain its pager (ENTER through every page)
    so the next command isn't swallowed by a pending 'more' prompt. Returns
    (first_page, whole_listing)."""
    first = cmd(sock, line)
    full = first
    resp = first
    guard = 0
    while "ENTER for more" in resp and guard < 60:
        resp = cmd(sock, "")
        full += resp
        guard += 1
    return first, full


name = f"Vnum{_suffix}"
pw = "vnumpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral
cmd(s, "color off")

# --- mortal is gated out ---
check("Command not found" in cmd(s, "show obj sword"), "a mortal cannot see vnum (Command not found)")

# --- promote to builder (51) and relog ---
set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

first, full = vnum_read(s, "show obj sword")
check("object vnums matching" in full.lower(), "vnum obj shows the object header")
check("sword" in full.lower(), "vnum obj sword lists objects whose name contains 'sword'")
check("ENTER for more" in first,
      "a long vnum list is paged, not capped (more prompt on the first page)")

_, full = vnum_read(s, "show mob demon")
check("mobile vnums matching" in full.lower() and "demon" in full.lower(),
      "vnum mob demon lists matching mobiles")

_, full = vnum_read(s, "show room square")
check("room vnums matching" in full.lower() and "square" in full.lower(),
      "vnum room square lists matching rooms")

check("Usage:" in cmd(s, "show"), "bare vnum shows usage")
check("Usage:" in cmd(s, "show obj"), "vnum with no pattern shows usage")
check("none" in cmd(s, "show obj zzqxnomatchzz").lower(), "a no-match pattern reports none")

# --- a bare vnum or a vnum range browses by vnum instead of by name
# (TODO.md "mlist/olist/rlist" ask, folded into `vnum` instead of three
# near-duplicate new commands) ---
_, full = vnum_read(s, "show obj 3")
check("object vnums 3-3" in full.lower(), "a bare vnum shows the vnums-N-N header")
check("fountain" in full.lower(), "vnum obj 3 finds the real seeded fountain (vnum 3)")

_, full = vnum_read(s, "show obj 1-10")
check("object vnums 1-10" in full.lower(), "a vnum range shows the vnums-N-M header")
check("fountain" in full.lower(), "vnum obj 1-10 includes vnum 3's fountain in range")

set_level(name, 1)
s.close()
announce_done("smoke_test_vnum", host, port)
print("=== ALL CHECKS PASSED ===")
