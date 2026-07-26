#!/usr/bin/env python3
"""Smoke test for room extra descriptions (`roomextra` table, classic
Diku-family "look <keyword>" reveals a hidden room detail -- a wall
poster, a bed, never a real object). 8,861 real seeded rows already
existed across the live DB with no Tobin code reading them at all until
this (room_repo_extra_desc(), room_repo.c, wired into look_at_target(),
cmd_look.c). Covers:
  1. `look <keyword>` on a real seeded extra-desc keyword reveals its
     description.
  2. Matching is case-insensitive and prefix-based, same convention as
     every other object/mob keyword match in this codebase.
  3. A keyword with no matching extra description (and no PC/mob/object
     either) still falls through to the ordinary "You don't see that
     here." -- this doesn't swallow real 404s.
  4. An extra description is room-scoped: the same keyword in a
     DIFFERENT room doesn't leak across.

    python3 tests/smoke_test_extra_desc.py [host] [port]
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


announce("smoke_test_extra_desc")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 950000 + (int(time.time()) % 60000)
ROOM_B = ROOM_A + 1


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


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


mort_name, mort_pw = f"Xdmortb{_suffix}", "xdmortpw1234"

s = make_char(mort_name, mort_pw)
cmd(s, "quit!")
s.close()

for vnum in (ROOM_A, ROOM_B):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'Extra Desc Sandbox {vnum}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sql(f"INSERT INTO roomextra (vnum, name, description) VALUES "
    f"({ROOM_A}, 'poster posters sign', 'A faded poster advertises a play that closed years ago.');")
sql(f"INSERT INTO roomextra (vnum, name, description) VALUES "
    f"({ROOM_B}, 'different-keyword', 'This should never show up in room A.');")

sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mort_name}';")

s = relog(mort_name, mort_pw)

# --- 1: a real seeded-style extra desc is revealed by look <keyword> ---
out = cmd(s, "look poster")
check("faded poster" in out, "look <keyword> reveals the room's extra description")

# --- 2: case-insensitive, prefix-based, matches any of several keywords ---
out = cmd(s, "look POSTERS")
check("faded poster" in out, "matching is case-insensitive")
out = cmd(s, "look sig")
check("faded poster" in out, "matching is prefix-based, same as every other keyword match in this codebase")

# --- 3: a real miss still falls through to the ordinary not-found message ---
out = cmd(s, "look nonexistentthingxyz")
check("You don't see that here" in out, "a genuine miss still reports the ordinary not-found message")

# --- 4: extra descriptions are room-scoped, not global ---
out = cmd(s, "look different-keyword")
check("You don't see that here" in out, "a different room's extra-desc keyword does not leak into this room")

s.close()
announce_done("smoke_test_extra_desc")
print("=== ALL CHECKS PASSED ===")
