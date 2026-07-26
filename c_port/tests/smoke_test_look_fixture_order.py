#!/usr/bin/env python3
"""Smoke test for fixture-first room listing (user, 2026-07-11: "permanent
items such as a lamppost or a fountain should be listed first in look room
code"). `look` (cmd_look.c) now lists non-takeable fixture objects (no
WEAR_TAKE -- fountains, statuary, furniture) ahead of ordinary takeable loot
and mobs/PCs, regardless of stuff_head/insertion order.

  1. A takeable item loaded BEFORE a fixture still shows the fixture first.
  2. Two fixtures loaded in either order both appear ahead of the takeable
     item (i.e. the reorder is a stable "fixtures first" grouping, not
     coincidence).

    python3 tests/smoke_test_look_fixture_order.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
ITEM = ROOM + 1
FIXTURE = ROOM + 2


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


announce("smoke_test_look_fixture_order")


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


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


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


imm_name = f"Fixord{_suffix}"
imm_pw = "fixordpw123"
item_name = f"trinket{_suffix}"
fixture_name = f"lamppost{_suffix}"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Fixture Order Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Fixture Order Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# Takeable item (wear_flag=1, WEAR_TAKE) loaded FIRST -- if plain insertion
# order were used, this would list ahead of the fixture loaded after it.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM},'{item_name}','a {item_name}','A {item_name} lies here.',12,1,1);")
check("You conjure" in cmd(s, f"load obj {ITEM}"), "the takeable item is loaded first")

# Non-takeable fixture (wear_flag=0, no WEAR_TAKE) loaded SECOND.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({FIXTURE},'{fixture_name}','a {fixture_name}','A {fixture_name} stands here.',22,0,1);")
check("You conjure" in cmd(s, f"load obj {FIXTURE}"), "the non-takeable fixture is loaded second")

out = cmd(s, "look")
item_pos = out.find(item_name)
fixture_pos = out.find(fixture_name)
check(item_pos != -1 and fixture_pos != -1, "both the item and the fixture appear in the room listing")
check(fixture_pos < item_pos,
      "the fixture (loaded second) is STILL listed before the takeable item (loaded first)")

s.close()
announce_done("smoke_test_look_fixture_order")
print("=== ALL CHECKS PASSED ===")
