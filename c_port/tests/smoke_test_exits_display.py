#!/usr/bin/env python3
"""Smoke test for the `look` exits line reformat (user, 2026-07-11:
"Obvious exits: north east south west southwest change to [Exits:] North
East South West Southwest and colorize the string appropriatly").

  1. With color off, the line reads "[Exits:] North East ..." (capitalized
     directions), not the old "Obvious exits: north east ...".
  2. With color on, the "[Exits:]" label and the direction list are wrapped
     in real ANSI escapes (translated from color tags), not left as
     literal tag text leaking through.
  3. A dead-end room still falls back to "[Exits:] none".
  4. The exits list is colored by the room's SECTOR, the same bright tag
     the room NAME uses -- not a fixed color (user follow-up, 2026-07-11:
     "the exit messages in a room should reflect the sector type and be
     colored like name").

    python3 tests/smoke_test_exits_display.py [host] [port]
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


announce("smoke_test_exits_display")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OPEN = 900000 + (int(time.time()) % 70000)
ROOM_DEADEND = ROOM_OPEN + 1


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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OPEN},0,0,0,'Exits Display Open','A bare sandbox room.\\n',NULL,1,43,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_DEADEND},0,0,0,'Exits Display Deadend','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_OPEN}, 0, '', '', 0, 0, 0, 0, 0, {ROOM_DEADEND});")

name = f"Exitdisp{_suffix}"
pw = "exitdisppw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, name, pw)
sql(f"UPDATE player SET load_room={ROOM_OPEN} WHERE name='{name}';")
cmd(s, "quit!")
s.close()

# --- 1 & 2: color off vs on, in the same open room ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)

out = cmd(s, "color off")
out = cmd(s, "look")
check("[Exits:] North" in out, "color-off look shows the reformatted, capitalized exits line")
check("Obvious exits" not in out, "the old 'Obvious exits' wording is gone")

out = cmd(s, "color on")
out = cmd(s, "look")
check("<c>" not in out and "<g>" not in out, "color-on look has no literal leaked color tags")
check("\x1b[" in out, "color-on look contains real ANSI escapes")
check("North" in out, "the direction text itself still reads correctly with color on")

# --- 4: the exits list is colored by SECTOR, matching the room name's own
# color, not a fixed color -- ROOM_OPEN was seeded with sector 43 (VOLCANO
# LAVA), which maps to bright red (1;31m) via sector_color(). Checked
# against the SAME `out` above -- a room's sector is only read at load
# time (world.c never refreshes an already-loaded room from a later SQL
# UPDATE), so this has to be set at INSERT time, not toggled mid-test. ---
name_start = out.find("Exits Display Open")
exits_start = out.find("North")
name_color = out[max(0, name_start - 15):name_start]
exits_color = out[max(0, exits_start - 15):exits_start]
check("\x1b[1;31m" in name_color, "the room name renders in bright red for the lava sector")
check("\x1b[1;31m" in exits_color, "the exits list ALSO renders in bright red, matching the name's sector color")

# --- 3: a dead-end room falls back to "none" ---
cmd(s, "color off")
check("Exits Display Deadend" in cmd(s, "north"), "walked into the dead-end room")
out = cmd(s, "look")
check("[Exits:] none" in out, "a dead-end room shows '[Exits:] none'")

s.close()
announce_done("smoke_test_exits_display")
print("=== ALL CHECKS PASSED ===")
