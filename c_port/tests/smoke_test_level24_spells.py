#!/usr/bin/env python3
"""Smoke test for the level-24 spell audit batch (2026-07-29):
`enhance weapon` (Mage). `conjure elemental water` and `animal companion`
(also level 24) were already covered by the existing pet/charm subsystem,
not new this session.

  1. `cast enhance weapon` applies AFFECT_ENHANCE_WEAPON (visible in
     `affects` as "Enhanced Weapon") -- the disclosed "permanent enchant"
     -> temporary to-hit buff deviation (see AFFECT_ENHANCE_WEAPON's own
     doc comment, affect.h), same shape as haste/rally.

    python3 tests/smoke_test_level24_spells.py [host] [port]
"""
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


announce("smoke_test_level24_spells")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900100 + (int(time.time()) % 70000)
ROOM_OUT = BASE


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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw, class_num):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)          # race: human
    send_line(sock, str(class_num)); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)       # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


mage_name = f"Lewmage{_suffix}"
mage_pw = "l24magepw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, mage_name, mage_pw, 1)  # Mage
set_level(mage_name, 51)
s.close()
mage = login(mage_name, mage_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'L24 Outdoor Sandbox','A bare outdoor sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

check("L24 Outdoor Sandbox" in cmd(mage, f"goto {ROOM_OUT}"), "mage goes to the outdoor sandbox")

COMP1 = BASE + 10
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({COMP1},'pouch component test','a pouch of test components','A pouch lies here.',12,1,10,10,1);")
check("You conjure" in cmd(mage, f"load obj {COMP1}"), "mage loads a component pouch")

# --- 1: enhance weapon applies AFFECT_ENHANCE_WEAPON ---
out = cmd(mage, "cast enhance weapon")
check("cast" in out.lower() and "sure" in out.lower(), "cast enhance weapon confirms")
out = cmd(mage, "affects")
check("enhanced weapon" in out.lower(), "`affects` shows Enhanced Weapon active")

print("ALL CHECKS PASSED")
announce_done("smoke_test_level24_spells")
