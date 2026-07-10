#!/usr/bin/env python3
"""Smoke test for zone identity/ownership (Session 43, user: "add identity
to zones... builder gets assigned a zone then... a 51-54 wants to edit
gets rejected except for those assigned to that zone"). Covers:
  1. A builder (51-54) is refused editing a room in a zone they aren't
     assigned to.
  2. A builder CAN edit a room with no zone at all (unrestricted).
  3. edzone's builder-assign menu item (55+ only) assigns a builder to a
     zone -- they can then edit rooms in that zone.
  4. Selecting an already-assigned name toggles the assignment back off --
     refused again.
  5. A 55+ immortal can always edit any zone, assigned or not.
  6. More than one builder can be assigned to the SAME zone at once (user:
     "make it so more than 1 builder can be assigned to a zone to edit") --
     assigning a second builder doesn't displace the first; both can edit,
     and un-assigning one leaves the other intact.

    python3 tests/smoke_test_zone_identity.py [host] [port]
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


announce("smoke_test_zone_identity")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ZONED_ROOM = 900000 + (int(time.time()) % 60000)
UNZONED_ROOM = ZONED_ROOM + 1
ZONE = 90000 + (int(time.time()) % 9000)


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
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


builder_name = f"Zoneb{_suffix}"
builder_pw = "zonebpw123"
builder2_name = f"Zonec{_suffix}"
builder2_pw = "zonecpw123"
senior_name = f"Zones{_suffix}"
senior_pw = "zonespw123"

sql(f"INSERT INTO zone (zone_nr,zone_name,zone_enabled,bottom,top,reset_mode,lifespan,age,util_flag) "
    f"VALUES ({ZONE},'Zone Identity Test Sandbox',1,{ZONED_ROOM},{ZONED_ROOM},2,999999,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ZONED_ROOM},0,0,0,'Zoned Sandbox','A bare sandbox room.\\n',{ZONE},0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({UNZONED_ROOM},0,0,0,'Unzoned Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

s = socket.create_connection((host, port), timeout=5)
make_char(s, builder_name, builder_pw)
set_level(builder_name, 52)
s.close()
s = login(builder_name, builder_pw)

s2 = socket.create_connection((host, port), timeout=5)
make_char(s2, senior_name, senior_pw)
set_level(senior_name, 55)
s2.close()
s2 = login(senior_name, senior_pw)

s3 = socket.create_connection((host, port), timeout=5)
make_char(s3, builder2_name, builder2_pw)
set_level(builder2_name, 53)
s3.close()
s3 = login(builder2_name, builder2_pw)

# --- 1: builder refused on a zoned room they aren't assigned to ---
out = cmd(s, f"edroom {ZONED_ROOM}")
check("you aren't assigned to that zone" in out.lower(), "builder is refused editing an unassigned zone's room")

# --- 2: builder CAN edit an unzoned room ---
out = cmd(s, f"edroom {UNZONED_ROOM}")
check("Room Name:" in out, "builder can edit a room with no zone at all")
cmd(s, "Q")  # leave the editor cleanly

# --- 3: edzone's builder-assign menu item (55+) assigns the builder;
#     now they can edit ---
cmd(s2, f"edzone {ZONE}")
out = cmd(s2, "5")
check("Enter a builder name" in out, "edzone menu item 5 prompts for a builder name")
out = cmd(s2, builder_name)
check("Assigned." in out, "assigning from edzone's menu confirms")
cmd(s2, "Q")

out = cmd(s, f"edroom {ZONED_ROOM}")
check("Room Name:" in out, "the now-assigned builder can edit the zoned room")
cmd(s, "Q")

# --- 6 (user: "make it so more than 1 builder can be assigned to a zone"):
#     a SECOND builder assigned to the SAME zone -- both can edit it,
#     neither displaces the other (zone_owner's PK is (zone_nr, player_id),
#     a real many-to-many, not a single-owner slot). ---
out = cmd(s3, f"edroom {ZONED_ROOM}")
check("you aren't assigned to that zone" in out.lower(), "the second builder isn't assigned yet, so is refused")

cmd(s2, f"edzone {ZONE}")
out = cmd(s2, "5")
out = cmd(s2, builder2_name)
check("Assigned." in out, "assigning the SECOND builder confirms")
check(f"{builder_name}, {builder2_name}" in out or f"{builder2_name}, {builder_name}" in out,
      "edzone's menu lists BOTH assigned builders -- assigning #2 didn't displace #1")
cmd(s2, "Q")

out = cmd(s3, f"edroom {ZONED_ROOM}")
check("Room Name:" in out, "the second builder can now edit the zone too")
cmd(s3, "Q")

out = cmd(s, f"edroom {ZONED_ROOM}")
check("Room Name:" in out, "the FIRST builder can still edit it too -- unaffected by the second assignment")
cmd(s, "Q")

# --- 4: toggling the first builder's assignment back off (via edzone,
#     selecting an already-assigned name) refuses them again, while the
#     second builder remains assigned ---
cmd(s2, f"edzone {ZONE}")
out = cmd(s2, "5")
out = cmd(s2, builder_name)
check("Un-assigned." in out, "selecting an already-assigned name un-assigns them")
cmd(s2, "Q")

out = cmd(s, f"edroom {ZONED_ROOM}")
check("you aren't assigned to that zone" in out.lower(), "the un-assigned first builder is refused again")
out = cmd(s3, f"edroom {ZONED_ROOM}")
check("Room Name:" in out, "the second builder is STILL assigned and can still edit")
cmd(s3, "Q")

# --- 5: a 55+ immortal can always edit any zone ---
out = cmd(s2, f"edroom {ZONED_ROOM}")
check("Room Name:" in out, "a 55+ immortal edits any zone regardless of assignment")
cmd(s2, "Q")

s.close()
s2.close()
s3.close()
announce_done("smoke_test_zone_identity")
print("=== ALL CHECKS PASSED ===")
