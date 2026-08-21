#!/usr/bin/env python3
"""Smoke test for door mechanics (cmd_open.c, the movement-blocking check
in cmd_move.c, and secret-exit hiding in cmd_look.c/cmd_exits.c):
  1. A secret exit (no door needed) is hidden from `look`'s "Obvious exits"
     line and from `exits`, but still walkable if you know the direction.
  2. A closed door blocks a MORTAL's movement ("The door is closed.")
     until `open` -- but an IMMORTAL walks straight through any exit,
     closed, locked, or otherwise (user 2026-08-21: handy for manually
     verifying/recalculating the map -- see cmd_move.c's own comment).
  3. `open <dir>` / `close <dir>` work, persist to the DB, and reject
     sensible error cases (no exit that way, no door there, already
     open/closed).
  4. Door state SYNCS to the reverse exit once BOTH sides genuinely have
     a door of their own (user 2026-07-30, an explicit reversal of the
     earlier per-exit-independence decision -- see STATUS.md Session
     104/105) -- but a doorless reverse exit (edroom's own default for an
     auto-created reverse exit) is left completely alone, no door forced
     onto it.

All setup happens in SQL-bootstrapped sandbox rooms at high vnums
(900000+); the seeded world is never touched.

    python3 tests/smoke_test_doors.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_doors", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, char_name, char_pw):
    recv_all(sock)
    send_line(sock, char_name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, char_pw); recv_all(sock)
    send_line(sock, char_pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, char_name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(char_name, char_pw):
    sock = socket.create_connection((host, port), timeout=5)
    recv_all(sock)
    send_line(sock, char_name); recv_all(sock)
    send_line(sock, char_pw); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    cmd(sock, "color off")
    return sock


name = f"Doortest{_suffix}"
pw = "doortestpw123"
mortal_name = f"Doormrtl{_suffix}"
mortal_pw = "doormortalpw12"

s = socket.create_connection((host, port), timeout=5)
make_char(s, name, pw)
set_level(name, 51)
s.close()
s = login(name, pw)

# A genuinely mortal second character (no set_level -- stays at the
# default post-creation level, well under IMMORTAL_LEVEL_MIN) so the
# closed-door-blocks-movement checks below prove real mortal behavior,
# not just "an immortal chose not to test the bypass."
m = socket.create_connection((host, port), timeout=5)
make_char(m, mortal_name, mortal_pw)
m.close()
m = login(mortal_name, mortal_pw)

# --- bootstrap: origin room with a closed door north, a secret exit east ---
for vnum, desc in ((BASE, "Sandbox Origin"), (BASE + 1, "Beyond the Door"), (BASE + 2, "The Secret Room")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{desc}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

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
check("You transfer" in cmd(s, f"transfer {mortal_name}"), "transfer brings the mortal into the sandbox room")
check("Sandbox Origin" in cmd(m, "look"), "the mortal really landed in the sandbox room")

# --- 1: secret exit hidden from look/exits, but still walkable ---
out = cmd(s, "look")
check("[Exits:] North" in out, "look's [Exits:] shows North but hides the secret east")
out = cmd(s, "exits")
check("north" in out and "east" not in out, "exits also hides the secret east exit")

# --- 2: an immortal walks straight through the closed door; a mortal
# stays blocked ---
out = cmd(s, "north")
check("Beyond the Door" in out, "an immortal walks straight through a closed door")
out = cmd(s, "south")
check("Sandbox Origin" in out, "the immortal walks back through the plain reverse exit")

out = cmd(m, "north")
check("The door is closed" in out, "a closed door still blocks a mortal's movement")

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

out = cmd(m, "north")
check("The door is closed" in out, "the door blocks the mortal's movement again after closing")

# --- 'door <direction>' / bare 'door' phrasing (user report: "open
# dootr doesnt work" -- Sneezy's documented syntax, lib/help/open,
# is "open door <direction>", which Tobin had never actually ported) ---
out = cmd(s, "open door north")
check("You open the door to the north" in out, "'open door north' works")
out = cmd(s, "close door north")
check("You close the door to the north" in out, "'close door north' works")

out = cmd(s, "open door")
check("You open the door to the north" in out, "bare 'open door' opens the room's one door")
out = cmd(s, "close door")
check("You close the door to the north" in out, "bare 'close door' closes the room's one door")

# --- persistence: the closed condition actually landed in the DB ---
cond = query(f"SELECT condition_flag FROM roomexit WHERE vnum={BASE} AND direction=0;")
check(cond == "1", "the Closed condition persisted to roomexit.condition_flag")

# --- the secret exit is still walkable even though never listed ---
out = cmd(s, "east")
check("The Secret Room" in out, "the secret exit is still walkable if you know the direction")

# --- door sync: BOTH sides have a real door -> opening/closing one
# side affects the other (user 2026-07-30) ---
SYNC_A, SYNC_B = BASE + 3, BASE + 4
for vnum, desc in ((SYNC_A, "Sync Room A"), (SYNC_B, "Sync Room B")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{desc}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# north from A: a real door, open
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({SYNC_A},0,'','',1,0,0,0,0,{SYNC_B});")
# south from B back to A: ALSO a real door, open -- the "same physical door"
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({SYNC_B},2,'','',1,0,0,0,0,{SYNC_A});")

cmd(s, f"goto {SYNC_A}")
out = cmd(s, "close north")
check("You close the door to the north" in out, "close north in room A succeeds")

cond_b = query(f"SELECT condition_flag FROM roomexit WHERE vnum={SYNC_B} AND direction=2;")
check(cond_b == "1", "closing from room A also marks room B's south exit Closed in the DB")

cmd(s, f"goto {SYNC_B}")
cmd(s, f"transfer {mortal_name}")
out = cmd(m, "south")
check("The door is closed" in out, "room B's south door is ALSO closed -- movement blocked for the mortal from either side")

out = cmd(s, "open south")
check("You open the door to the south" in out, "opening from room B succeeds")
cond_a = query(f"SELECT condition_flag FROM roomexit WHERE vnum={SYNC_A} AND direction=0;")
check(cond_a == "0", "opening from room B also marks room A's north exit open in the DB")

cmd(s, f"goto {SYNC_A}")
cmd(s, f"transfer {mortal_name}")
out = cmd(m, "north")
check("Sync Room B" in out, "room A's door reopened too -- movement now succeeds for the mortal from either side")

s.close()
m.close()
announce_done("smoke_test_doors", host, port)
print("=== ALL CHECKS PASSED ===")
