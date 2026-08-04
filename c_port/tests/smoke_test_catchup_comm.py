#!/usr/bin/env python3
"""Smoke test for catchup's communication-vs-ambient split (user
2026-07-26: "catchup command should only record communications not
theme messages"). descriptor_notify() (ambient: room echoes, combat,
mob AI, weather, ...) now DROPS instead of holding while the recipient
is mid-editor; descriptor_notify_comm() (tell/say/shout/whisper/wiznet/
newbie channel) still holds and replays via `catchup`, same as before.
Covers:

  1. While A is mid-editor, a `tell` and a `say` from B are held and
     show up in A's `catchup` afterward.
  2. An ambient room echo (B picking up a dropped item) during the same
     window does NOT show up in catchup -- it was silently dropped, not
     recorded at all.

    python3 tests/smoke_test_catchup_comm.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_catchup_comm", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 940000 + (int(time.time() * 1000) % 60000)
ITEM = ROOM + 1


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    send_line(s, "quit!")
    recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


a_name, a_pw = f"Ccua{_suffix}", "ccuapw123456"
b_name, b_pw = f"Ccub{_suffix}", "ccubpw123456"

sockets = []
try:
    make_char(a_name, a_pw, "3")
    make_char(b_name, b_pw, "3")
    sql(f"UPDATE player_progress SET level=56 WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{a_name}','{b_name}'));")

    a = login(a_name, a_pw); sockets.append(a)
    b = login(b_name, b_pw); sockets.append(b)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Catchup Comm Sandbox','A bare sandbox room.\\n',"
        f"NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,decay) "
        f"VALUES ({ITEM},'rock plain','a plain rock','A plain rock is lying here.',5,1,1,-1);")
    check("Catchup Comm Sandbox" in cmd(a, f"goto {ROOM}"), "A reaches the sandbox room")
    cmd(a, f"transfer {b_name}")
    recv_all(b)
    cmd(a, f"load obj {ITEM}")
    cmd(a, "drop rock")

    # A enters an editor -- from here on, A's own notify calls should
    # either hold (real comm) or silently drop (ambient) until A leaves.
    out = cmd(a, "edit room")
    check("Room Name" in out, "A is now mid-editor (edit room menu shown)")

    cmd(b, f"tell {a_name} hello there")
    cmd(b, "say this is a room broadcast")
    cmd(b, "get rock")  # ambient -- must NOT be recorded

    cmd(a, "q")  # leave the editor cleanly

    out = cmd(a, "catchup")
    check("hello there" in out, "the held tell shows up in catchup")
    check("room broadcast" in out, "the held say shows up in catchup")
    check("rock" not in out.lower(),
          "the ambient 'gets a rock' room echo was never recorded at all")

    for s in sockets:
        s.close()
    sockets = []

    announce_done("smoke_test_catchup_comm", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            s.close()
        except OSError:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum={ITEM};")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{a_name}','{b_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{a_name}','{b_name}');")
