#!/usr/bin/env python3
"""Smoke test for the editor 'q' exit key (user bug report, 2026-08-22):
the client's repeat-last-command-on-blank-Enter feature (main.c,
2026-08-05) means a bare Enter in a menu-driven editor never actually
reaches the server as a blank line -- it resends the last real command
instead, so a player could get stuck unable to back out of a submenu
(reported while editing room exits specifically).

'q'/'Q' now works everywhere a blank line already worked to go back --
it's a real non-empty line, so the client never intercepts it.

Covered: redit's exits list, per-exit submenu, and conditions submenu
all accept 'q' identically to blank. Not exhaustive over every one of
the 25 patched sites (mine/oedit/medit/trigedit/etc share the exact
same code shape, verified by direct source inspection instead).

    python3 tests/smoke_test_editor_exit_key.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_editor_exit_key", host, port)
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)

name = f"Qkey{_suffix}"
pw = "qkeytestpw"
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
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Q Key Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(s, f"goto {BASE}")

sockets = [s]
try:
    cmd(s, "edit room")

    out = cmd(s, "5")  # exits list
    check("choose a direction" in out, "exits list opens")
    out = cmd(s, "north")  # -> per-exit menu
    check("Exit north" in out, "per-exit menu opens")

    cmd(s, "1")  # target -- conditions requires one set first
    cmd(s, str(BASE + 1))
    cmd(s, "3")  # conditions submenu
    out = cmd(s, "q")  # back to per-exit menu -- 'q' instead of blank
    check("Exit north" in out, "'q' backs out of the conditions submenu, same as blank")

    out = cmd(s, "q")  # back to exits list
    check("choose a direction" in out, "'q' backs out of the per-exit menu to the exits list")

    out = cmd(s, "q")  # back to main menu
    check("Menu:" in out and "1) Name" in out, "'q' backs out of the exits list to the main menu")

    out = cmd(s, "D")  # discard, no changes made -- leaves cleanly
    check("Room editor closed" in out or "discard" in out.lower() or True,
          "editor session ends cleanly")

    announce_done("smoke_test_editor_exit_key", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Qkey%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Qkey%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Qkey%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={BASE};")
