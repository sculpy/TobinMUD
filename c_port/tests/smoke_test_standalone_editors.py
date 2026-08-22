#!/usr/bin/env python3
"""Smoke test for the standalone per-noun editor verbs (user decision,
2026-08-22: "decouple the edit commands into individual parts: redit
for rooms oedit for objs medit for mobs trigedit for triggers etc"),
reversing the 2026-08-02 cmd_table.c audit note that had deliberately
left these unregistered in favor of `edit <noun>` alone (real Sneezy
names -- redit/medit/oedit/trigedit -- were already documented there
as intentionally not given their own entry).

Each verb forwards to the EXACT same handler `edit <noun>` already
calls -- both forms work side by side. Covers just reachability (each
verb opens its real editor, not "Command not found" / "Huh?") and that
`edit room` still works unchanged -- the underlying editor behavior
itself is already covered by smoke_test_redit.py and friends.

    python3 tests/smoke_test_standalone_editors.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_standalone_editors", host, port)
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)

name = f"Sted{_suffix}"
pw = "stedtestpw"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Standalone Editors Sandbox','A bare sandbox room.\\n',"
    f"NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(s, f"goto {BASE}")

sockets = [s]
try:
    out = cmd(s, "redit")
    check(f"Number: {BASE}" in out, "redit opens the room editor for the current room")
    cmd(s, "q")

    out = cmd(s, "oedit")
    check("Command not found" not in out and "Huh?" not in out, "oedit is reachable")
    cmd(s, "q")

    out = cmd(s, "medit")
    check("Command not found" not in out and "Huh?" not in out, "medit is reachable")
    cmd(s, "q")

    out = cmd(s, "trigedit")
    check("Command not found" not in out and "Huh?" not in out, "trigedit is reachable")
    cmd(s, "q")

    out = cmd(s, "zedit")
    check("Command not found" not in out and "Huh?" not in out, "zedit is reachable")
    cmd(s, "q")

    out = cmd(s, "socedit")
    check("Socials" in out, "socedit (renamed from edsocial, level-55 gated) is reachable to a level-60 immortal")
    cmd(s, "q")

    # The unified `edit <noun>` form still works unchanged, side by side.
    out = cmd(s, "edit room")
    check(f"Number: {BASE}" in out, "the unified 'edit room' form still works unchanged")
    cmd(s, "q")

    announce_done("smoke_test_standalone_editors", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Sted%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Sted%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Sted%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={BASE};")
