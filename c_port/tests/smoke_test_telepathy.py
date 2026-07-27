#!/usr/bin/env python3
"""Smoke test for `telepathy` (Mage, level 16) -- spell/skill functional-
completeness audit continued, level-5+ list. See cmd_cast.c's telepathy
branch for the real-upstream research (disc_mage_spirit.cc's telepathy())
and scope-down rationale.

  1. `cast telepathy <message>` with no message is refused.
  2. A successful cast reaches every OTHER connected character in the
     world -- even one asleep, unlike `shout` (telepathy is mind-to-mind,
     not sound).

    python3 tests/smoke_test_telepathy.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_telepathy")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 974500 + (int(time.time()) % 1000)
COMPONENT = ROOM + 1

CLASS_MAGE = 0
CLASS_WARRIOR = 2
WEAR_TAKE = 1


def recv_all(sock, timeout=1.0):
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
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM},0,0,0,'Telepathy Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0),"
    f"({ROOM + 100},1,0,0,'Telepathy Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")

pw = "telepw123456"
mage_name = f"Telmag{_suffix}"
listener_name = f"Tellst{_suffix}"

sockets = []
try:
    make_char(mage_name, pw)
    make_char(listener_name, pw)
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{mage_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{listener_name}';")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
    sql(f"UPDATE player_progress SET level=51, basic_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{mage_name}');")
    # A DIFFERENT room from the mage (telepathy reaches the whole world,
    # not just the room/nearby exits) -- and left asleep on purpose, since
    # telepathy is mind-to-mind and deliberately does NOT skip a sleeping
    # listener the way `shout` does.
    sql(f"UPDATE player SET load_room={ROOM + 100} WHERE name='{listener_name}';")
    sql(f"UPDATE player_progress SET level=20 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{listener_name}');")

    sm = relog(mage_name, pw); sockets.append(sm)
    sl = relog(listener_name, pw); sockets.append(sl)
    cmd(sm, "toggle pk"); cmd(sl, "toggle pk")
    cmd(sl, "sleep"); recv_all(sl, 0.3)

    # --- 1: no message is refused ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out1 = strip(cmd(sm, "cast telepathy"))
    check("need to send some sort of message" in out1.lower(), "telepathy with no message is refused")

    # --- 2: a real message reaches a sleeping listener in a different room ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out2 = strip(cmd(sm, "cast telepathy is anyone else awake out there"))
    check("telepathically send" in out2.lower(), "telepathy succeeds with a message and a component")
    out2b = strip(recv_all(sl, 0.5))
    check("telepathic message" in out2b.lower() and "is anyone else awake out there" in out2b.lower(),
          "a sleeping listener in a different room still receives the telepathic message")

    announce_done("smoke_test_telepathy")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Telmag", "Tellst"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM}, {ROOM + 100});")
    sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")
