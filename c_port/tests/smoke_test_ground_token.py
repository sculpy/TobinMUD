#!/usr/bin/env python3
"""Smoke test for the `$$g`/`$g` ground-surface token (Session 43, user:
"investigate <d> and $$g tags from sneezy and implement in tobin"). Sneezy's
misc/show.cc replaces this token in an object's description with the
current room's ground-surface word (TRoom::describeGroundType()) -- "the
sword lies half-buried in the $$g" reads "...in the sand" on a beach,
"...in the street" in a city. No weather-prefix component (Tobin has no
weather system yet) -- see room_ground_type()'s doc comment.

  1. TEMPERATE CITY -> "street"
  2. TEMPERATE BEACH -> "sand"
  3. TEMPERATE SWAMP -> "mud"
  4. PLAINS + the INDOORS room flag -> "floor"
  5. PLAINS with no INDOORS flag -> "ground" (the default)
  6. The bare `$g` (single-dollar) form works too, not just `$$g`.
  7. The same substitution applies to the room-floor listing (`look`),
     not just `look <object>`.

    python3 tests/smoke_test_ground_token.py [host] [port]
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


announce("smoke_test_ground_token")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 60000)
ROOM_CITY, ROOM_BEACH, ROOM_SWAMP, ROOM_INDOOR, ROOM_PLAIN = \
    BASE, BASE + 1, BASE + 2, BASE + 3, BASE + 4
OBJ_DOUBLE, OBJ_SINGLE, OBJ_FLOOR = BASE + 5, BASE + 6, BASE + 7

SECTOR_TEMPERATE_CITY = 18
SECTOR_TEMPERATE_BEACH = 28
SECTOR_TEMPERATE_SWAMP = 24
SECTOR_PLAINS = 17
ROOM_FLAG_INDOORS = 1 << 3


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


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def room_insert(vnum, name, sector, room_flag=0):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,{room_flag},{sector},"
        f"0,0,0,0,0,0,0,0);")


room_insert(ROOM_CITY, "Ground Token City", SECTOR_TEMPERATE_CITY)
room_insert(ROOM_BEACH, "Ground Token Beach", SECTOR_TEMPERATE_BEACH)
room_insert(ROOM_SWAMP, "Ground Token Swamp", SECTOR_TEMPERATE_SWAMP)
room_insert(ROOM_INDOOR, "Ground Token Indoors", SECTOR_PLAINS, ROOM_FLAG_INDOORS)
room_insert(ROOM_PLAIN, "Ground Token Plain", SECTOR_PLAINS)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ_DOUBLE},'coin{_suffix}','a coin','A coin lies half-buried in the $$g.',12,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ_SINGLE},'ring{_suffix}','a ring','A ring rests on the $g.',12,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ_FLOOR},'gem{_suffix}','a gem','A gem glints against the $$g.',12,1,1);")

imm_name = f"Groundt{_suffix}"
imm_pw = "groundtpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- 1: TEMPERATE CITY -> "street" ---
cmd(s, f"goto {ROOM_CITY}")
check("You conjure" in cmd(s, f"load obj {OBJ_DOUBLE}"), "the double-dollar fixture is loaded (city)")
out = cmd(s, f"look coin{_suffix}")
check("half-buried in the street." in out, "$$g resolves to 'street' in a TEMPERATE CITY room")

# --- 2: TEMPERATE BEACH -> "sand" ---
cmd(s, f"goto {ROOM_BEACH}")
check("You conjure" in cmd(s, f"load obj {OBJ_DOUBLE}"), "the double-dollar fixture is loaded (beach)")
out = cmd(s, f"look coin{_suffix}")
check("half-buried in the sand." in out, "$$g resolves to 'sand' on a TEMPERATE BEACH")

# --- 3: TEMPERATE SWAMP -> "mud" ---
cmd(s, f"goto {ROOM_SWAMP}")
check("You conjure" in cmd(s, f"load obj {OBJ_DOUBLE}"), "the double-dollar fixture is loaded (swamp)")
out = cmd(s, f"look coin{_suffix}")
check("half-buried in the mud." in out, "$$g resolves to 'mud' in a TEMPERATE SWAMP")

# --- 4: PLAINS + INDOORS -> "floor" ---
cmd(s, f"goto {ROOM_INDOOR}")
check("You conjure" in cmd(s, f"load obj {OBJ_DOUBLE}"), "the double-dollar fixture is loaded (indoors)")
out = cmd(s, f"look coin{_suffix}")
check("half-buried in the floor." in out, "$$g resolves to 'floor' in an INDOORS-flagged room")

# --- 5: PLAINS, no flag -> "ground" (the default) ---
cmd(s, f"goto {ROOM_PLAIN}")
check("You conjure" in cmd(s, f"load obj {OBJ_DOUBLE}"), "the double-dollar fixture is loaded (plain)")
out = cmd(s, f"look coin{_suffix}")
check("half-buried in the ground." in out, "$$g falls back to 'ground' with no special sector/flag match")

# --- 6: the single-dollar $g form works too ---
check("You conjure" in cmd(s, f"load obj {OBJ_SINGLE}"), "the single-dollar fixture is loaded (plain)")
out = cmd(s, f"look ring{_suffix}")
check("rests on the ground." in out, "the single-dollar $g form is also substituted")

# --- 7: the room-floor listing (`look`) applies the same substitution ---
check("You conjure" in cmd(s, f"load obj {OBJ_FLOOR}"), "the floor-listing fixture is loaded (plain)")
out = cmd(s, "look")
check("glints against the ground." in out, "$$g is substituted in the room-floor listing too, not just look <obj>")

s.close()
announce_done("smoke_test_ground_token")
print("=== ALL CHECKS PASSED ===")
