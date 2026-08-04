#!/usr/bin/env python3
"""Smoke test for `lock`/`unlock` (cmd_lock.c) on both doors and containers.
Keys are matched by the KEY object's OWN vnum against a door's
roomexit.key_num / a container's val[2] -- see cmd_lock.c's header comment.
Covers:
  1. A locked, keyless door refuses lock/unlock with "no keyhole" messages.
  2. A closed+locked door: unlock fails without the key, succeeds once
     carried, `open` then works; re-closing and `lock`ing without the key
     removed still requires it to be present again (has_key re-checked).
  3. `lock` on an open door is refused ("close it first").
  4. Door state (the Locked condition bit) persists to roomexit.condition_flag.
  5. Same flow for a closed+locked container (val[1] CONT_LOCKED, val[2] key
     vnum): unlock without the key, with the key, lock requires closed first.
  6. The wrong key (a different KEY object) does not work on either.

All setup is SQL-bootstrapped sandbox rooms/objects at high vnums (900000+);
the seeded world is never touched.

    python3 tests/smoke_test_keys.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_keys", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 80000)

ROOM = BASE          # origin room
BEYOND = BASE + 1    # through the keyed door
KEYLESS = BASE + 2   # through the keyless-but-locked door
GOODKEY = BASE + 3   # the real key for the door AND the chest
WRONGKEY = BASE + 4  # a different key -- must NOT work
CHEST = BASE + 5     # closed+locked container, keyed to GOODKEY
GEM = BASE + 6       # something to stash in the chest

WEAR_TAKE = 1
TYPE_KEY = 18      # ITEM_KEY -> OBJ_CAT_KEY
TYPE_CHEST = 15    # ITEM_CHEST -> OBJ_CAT_CONTAINER
TYPE_TRINKET = 5

CONT_CLOSEABLE = 1 << 0
CONT_CLOSED = 1 << 2
CONT_LOCKED = 1 << 3


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def obj_insert(vnum, name, short_desc, long_desc, item_type, wear_flag,
               val0=0, val1=0, val2=0, weight=0):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"val0,val1,val2,weight,can_be_seen) VALUES ({vnum},'{name}','{short_desc}',"
        f"'{long_desc}',{item_type},{wear_flag},{val0},{val1},{val2},{weight},1);")


name = f"Keytest{_suffix}"
pw = "keytestpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral

set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

# --- bootstrap ---
for vnum, desc in ((ROOM, "Keyroom Origin"), (BEYOND, "Beyond the Locked Door"),
                    (KEYLESS, "Beyond the Keyless Door")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{desc}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# north (dir 0): Door, Closed+Locked (1|2=3), keyed to GOODKEY
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},0,'','',1,3,0,0,{GOODKEY},{BEYOND});")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({BEYOND},2,'','',0,0,0,0,0,{ROOM});")
# east (dir 1): Door, Closed+Locked, key_num=0 -- no real keyhole
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},1,'','',1,3,0,0,0,{KEYLESS});")

obj_insert(GOODKEY, "brass", "a brass key", "A brass key is lying here.", TYPE_KEY, WEAR_TAKE)
obj_insert(WRONGKEY, "iron", "an iron key", "An iron key is lying here.", TYPE_KEY, WEAR_TAKE)
obj_insert(CHEST, "chest", "a heavy chest", "A heavy chest sits here.",
           TYPE_CHEST, WEAR_TAKE, val0=100, val1=CONT_CLOSEABLE | CONT_CLOSED | CONT_LOCKED,
           val2=GOODKEY, weight=40)
obj_insert(GEM, "gem", "a shiny gem", "A shiny gem is lying here.", TYPE_TRINKET, WEAR_TAKE, weight=2)

check("Keyroom Origin" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

# --- 1: keyless locked door ---
check("keyhole" in cmd(s, "unlock east").lower(), "unlock refuses a keyhole-less locked door")
check("keyhole" in cmd(s, "lock east").lower(), "lock also refuses a keyhole-less door")

# --- 2: keyed door, no key carried yet ---
check("door is closed" in cmd(s, "north"), "the closed+locked door blocks movement")
check("don't have the proper key" in cmd(s, "unlock north"), "unlock refuses without the key")

cmd(s, f"load obj {WRONGKEY}"); cmd(s, "get iron")
check("don't have the proper key" in cmd(s, "unlock north"), "the WRONG key does not unlock the door")

cmd(s, f"load obj {GOODKEY}"); cmd(s, "get brass")
out = cmd(s, "unlock north")
check("*Click*" in out and "unlock" in out.lower(), "the right key unlocks the door")
check("already unlocked" in cmd(s, "unlock north"), "unlocking an already-unlocked door is rejected")

check("It's locked" not in cmd(s, "open north"), "open now succeeds (door was only locked, still closed)")
out = cmd(s, "north")
check("Beyond the Locked Door" in out, "movement succeeds once unlocked and opened")
cmd(s, "south")

# --- 3: lock requires closed first ---
check("close it first" in cmd(s, "lock north").lower(), "lock on an open door demands it be closed first")
cmd(s, "close north")
out = cmd(s, "lock north")
check("*Click*" in out, "lock succeeds once closed and the key is carried")
check("already locked" in cmd(s, "lock north"), "locking an already-locked door is rejected")
check("door is closed" in cmd(s, "north"), "the door blocks movement again once relocked")

# --- 4: persistence ---
cond = query(f"SELECT condition_flag FROM roomexit WHERE vnum={ROOM} AND direction=0;")
check(cond == "3", "the relocked Closed+Locked state persisted to roomexit.condition_flag")

# --- 5/6: container lock/unlock, wrong key rejected ---
cmd(s, f"load obj {CHEST}")
check("locked" in cmd(s, "open chest").lower(), "a closed+locked chest refuses open")

# drop both keys (still carried from the door section above) to isolate the no-key case
cmd(s, "drop brass")
cmd(s, "drop iron")
check("don't seem to have the proper key" in cmd(s, "unlock chest"), "unlock refuses the chest with no key carried")

cmd(s, "get iron")
check("don't seem to have the proper key" in cmd(s, "unlock chest"), "the WRONG key does not unlock the chest")
cmd(s, "drop iron")

cmd(s, "get brass")
out = cmd(s, "unlock chest")
check("*Click*" in out, "the right key unlocks the chest")
check("You open" in cmd(s, "open chest"), "the chest opens once unlocked")
cmd(s, f"load obj {GEM}"); cmd(s, "get gem")
check("You put" in cmd(s, "put gem chest"), "an item can be stashed once open")

check("close it first" in cmd(s, "lock chest").lower(), "lock on an open chest demands it be closed first")
cmd(s, "close chest")
out = cmd(s, "lock chest")
check("*Click*" in out, "lock re-locks the chest once closed and the key is carried")
check("locked" in cmd(s, "open chest").lower(), "the relocked chest refuses open again")

s.close()
announce_done("smoke_test_keys", host, port)
print("=== ALL CHECKS PASSED ===")
