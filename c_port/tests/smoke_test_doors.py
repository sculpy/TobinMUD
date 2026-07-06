#!/usr/bin/env python3
"""Smoke test for door mechanics (cmd_open.c, the movement-blocking check
in cmd_move.c, and secret-exit hiding in cmd_look.c/cmd_exits.c):
  1. A secret exit (no door needed) is hidden from `look`'s "Obvious exits"
     line and from `exits`, but still walkable if you know the direction.
  2. A closed door blocks movement ("The door is closed.") until `open`.
  3. `open <dir>` / `close <dir>` work, persist to the DB, and reject
     sensible error cases (no exit that way, no door there, already
     open/closed).
  4. Door state is per-exit/per-direction, NOT mirrored to the reverse
     exit -- a deliberate simplification matching how edroom's own
     auto-created reverse exits already work (see STATUS.md).

All setup happens in SQL-bootstrapped sandbox rooms at high vnums
(900000+); the seeded world is never touched.

    python3 tests/smoke_test_doors.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)


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


def query(stmt):
    return subprocess.run(["mariadb", "-N", "sneezy", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


name = f"Doortest{_suffix}"
pw = "doortestpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "done"); recv_all(s)

set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

# --- bootstrap: origin room with a closed door north, a secret exit east ---
for vnum, desc in ((BASE, "Sandbox Origin"), (BASE + 1, "Beyond the Door"), (BASE + 2, "The Secret Room")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{desc}','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

# north (dir 0): a real Door (type 1), Closed (condition bit 0 = 1)
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({BASE},0,'','',1,1,0,0,0,{BASE + 1});")
# reverse (south from BASE+1 back): plain exit, no door -- per-exit independence
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({BASE + 1},2,'','',0,0,0,0,0,{BASE});")
# east (dir 1): no door, but Secret (condition bit 2 = 4)
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({BASE},1,'','',0,4,0,0,0,{BASE + 2});")

check("Sandbox Origin" in cmd(s, f"goto {BASE}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: secret exit hidden from look/exits, but still walkable ---
out = cmd(s, "look")
check("Obvious exits: north" in out, "look's Obvious exits shows north but hides the secret east")
out = cmd(s, "exits")
check("north" in out and "east" not in out, "exits also hides the secret east exit")

# --- 2: the door blocks movement until opened ---
out = cmd(s, "north")
check("The door is closed" in out, "a closed door blocks movement")

# --- 3: open/close error cases ---
check("don't see an exit" in cmd(s, "open west"), "open rejects a direction with no exit")
check("no door there" in cmd(s, "open east").lower(), "open rejects a direction with no door (the secret exit)")

# --- open, walk through, come back, close ---
out = cmd(s, "open north")
check("You open the door to the north" in out, "open confirms")
check("It's already open" in cmd(s, "open north"), "opening an already-open door is rejected")

out = cmd(s, "north")
check("Beyond the Door" in out, "movement succeeds once the door is open")

out = cmd(s, "south")
check("Sandbox Origin" in out, "walking back through the plain (doorless) reverse exit works")

out = cmd(s, "close north")
check("You close the door to the north" in out, "close confirms")
check("It's already closed" in cmd(s, "close north"), "closing an already-closed door is rejected")

out = cmd(s, "north")
check("The door is closed" in out, "the door blocks movement again after closing")

# --- persistence: the closed condition actually landed in the DB ---
cond = query(f"SELECT condition_flag FROM roomexit WHERE vnum={BASE} AND direction=0;")
check(cond == "1", "the Closed condition persisted to roomexit.condition_flag")

# --- the secret exit is still walkable even though never listed ---
out = cmd(s, "east")
check("The Secret Room" in out, "the secret exit is still walkable if you know the direction")

s.close()
print("=== ALL CHECKS PASSED ===")
