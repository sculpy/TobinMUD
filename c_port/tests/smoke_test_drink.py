#!/usr/bin/env python3
"""Smoke test for `drink` (user, 2026-07-11: "yu should be able tto drink
from the pools, chance to get poisoned"). Drinking targets the ground
puddles obj_create_pool() makes (shared by `pee` and the blood-pool
bleeding reaction) -- never consumed/removed. There's a 30% chance per
drink of a non-lethal "poison" hit (2-8 HP, clamped so it can't drop the
drinker below 1 HP).

  1. `drink <puddle>` fails with nothing to drink from in an empty room.
  2. After `pee` leaves a puddle, a mortal can drink from it by any of its
     keywords ("puddle", "pool", "pee"), and it's never consumed.
  3. Across enough drinks, the poison message fires at least once (30%
     chance each try -- 30 tries is a statistical near-certainty, same
     determinism-via-repetition approach as smoke_test_mob_ai.py's aitick
     loop) and never drops HP below 1.

    python3 tests/smoke_test_drink.py [host] [port]
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


announce("smoke_test_drink")

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
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Drinkimm{_suffix}"
imm_pw = "drinkimmpw123"
mort_name = f"Drinkmort{_suffix}"
mort_pw = "drinkmortpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Drink Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Drink Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sm, "quit!")
sm.close()
sm = login(mort_name, mort_pw)
check("Drink Sandbox" in cmd(sm, "look"), "the mortal lands directly in the sandbox room")

# --- 1: nothing to drink yet ---
out = cmd(sm, "drink puddle")
check("don't see that here" in out.lower(), "drink fails when there's no puddle in the room")

# --- 2: pee creates a puddle, drinkable by any of its keywords, never consumed ---
check("You relieve yourself" in cmd(s, "pee"), "the immortal leaves a puddle via pee")

out = cmd(sm, "drink pool")
check("You scoop up some of a puddle of pee and drink it" in out,
      "drink resolves the 'pool' keyword alias to the puddle")

out = cmd(sm, "drink pee")
check("You scoop up some of a puddle of pee and drink it" in out,
      "drink also resolves the 'pee' keyword")

out = cmd(sm, "look")
check("puddle of pee" in out.lower(), "the puddle is still there after being drunk from (not consumed)")

# --- 3: across enough tries, poison fires at least once, never lethal ---
poisoned = False
for _ in range(30):
    out = cmd(sm, "drink puddle")
    if "poison courses through you" in out:
        poisoned = True
    hp_out = cmd(sm, "score")
    m = re.search(r"HP:\s*(-?\d+)/(\d+)", hp_out)
    if m:
        check(int(m.group(1)) >= 1, "poison never drops HP below 1")

check(poisoned, "the poison message fired at least once across 30 drinks (30% chance each)")

# --- 4: a real OBJ_CAT_DRINK fountain is drinkable too, no poison, never
# consumed (user bug report, 2026-07-11: "i just tried to drink from a
# fountain in the game, it failed with You don't see that here to drink").
# vnum 3 ("fountain water" / "a large fountain") is real seeded content.
FOUNTAIN_VNUM = 3
check("You conjure" in cmd(s, f"load obj {FOUNTAIN_VNUM}"), "the immortal loads a real seeded fountain")
out = cmd(sm, "drink fountain")
check("Refreshing!" in out, "drink resolves a real fountain, no longer 'You don't see that here to drink'")
out = cmd(sm, "look")
check("fountain" in out.lower(), "the fountain is still there after being drunk from (not consumed)")

s.close()
sm.close()
announce_done("smoke_test_drink")
print("=== ALL CHECKS PASSED ===")
