#!/usr/bin/env python3
"""Smoke test for the starter trigger content (user, 2026-07-11: "and
convert what sneezy has into a starter set of db data for tobin"),
db/sneezy/trigger_seed.sql -- verifies the seeded rows actually fire, not
just that they exist in the table.

  1. The real "dirty refuse hauler" (vnum 33271) mutters something rude
     when a nearby player says "hello" (insulter-inspired speech trigger).
  2. Picking up the new "tangle of thorny brambles" (vnum 1000001) prints
     the scratch echo AND applies the 2-point damage -- both halves of the
     two-line script fire, not just the first (stickerBush-inspired).

    python3 tests/smoke_test_trigger_seed.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

MOB_VNUM = 33271
BRAMBLE_VNUM = 1000001


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


announce("smoke_test_trigger_seed")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)


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
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Seedimm{_suffix}"
imm_pw = "seedimmpw123"
mort_name = f"Seedmort{_suffix}"
mort_pw = "seedmortpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Seed Content Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Seed Content Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sm, "quit!")
sm.close()
sm = login(mort_name, mort_pw)
check("Seed Content Sandbox" in cmd(sm, "look"), "the mortal lands directly in the sandbox room")

# --- 1: insulter-inspired speech trigger on the real dirty refuse hauler ---
check("You conjure" in cmd(s, f"load mob {MOB_VNUM}"), "the real dirty refuse hauler is loaded")
out = cmd(sm, "say hello")
check("mutters something rude" in out, "the hauler's seeded speech trigger fires on 'hello'")

# --- 2: stickerBush-inspired get trigger on the new brambles ---
check("You conjure" in cmd(s, f"load obj {BRAMBLE_VNUM}"), "the new tangle of thorny brambles is loaded")
hp_before_m = re.search(r"HP:\s*(-?\d+)/(\d+)", cmd(sm, "score"))
out = cmd(sm, "get brambles")
check("thorns prick your fingers" in out, "the get trigger's echo action fires")
hp_after_m = re.search(r"HP:\s*(-?\d+)/(\d+)", cmd(sm, "score"))
check(int(hp_after_m.group(1)) == int(hp_before_m.group(1)) - 2,
      "the get trigger's damage action ALSO fires (both script lines ran)")

s.close()
sm.close()
announce_done("smoke_test_trigger_seed")
print("=== ALL CHECKS PASSED ===")
