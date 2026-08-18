#!/usr/bin/env python3
"""Smoke test for `search` (Unimplemented skills/spells backlog, Session
158 audit: Thief, skill.c level 1). See cmd_search.c.

Covers:
  1. A hidden (SECRET) exit is omitted from the room's normal exit list.
  2. `search` reveals that hidden passage to the searcher (an immortal
     Thief bypasses the roll, so discovery is deterministic).
  3. In a room with no secret exit, `search` turns up nothing.
  4. A character who doesn't know the skill is refused.

    python3 tests/smoke_test_search.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 954000 + (int(time.time()) % 20000)   # has a secret east exit
ROOM2 = ROOM + 1                              # the hidden destination
ROOM3 = ROOM + 2                              # no secrets

SECRET = 4  # EXIT_COND_SECRET = (1 << 2)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def mkroom(vnum, name):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


announce("smoke_test_search", host, port)

mkroom(ROOM, "Search Sandbox")
mkroom(ROOM2, "A Hidden Alcove")
mkroom(ROOM3, "Plain Sandbox")
# A SECRET (no-door) exit east from ROOM -> ROOM2.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},1,'','',0,{SECRET},0,0,0,{ROOM2});")

# --- 1 & 2 & 3: immortal Thief searches ---
iname, ipw = f"Sct{_suffix}", "sctpw1234567"
s = make_char(iname, ipw, 4)  # Thief
cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{iname}');")
s = relog(iname, ipw)

check("Search Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the search sandbox")
# The SECRET exit is omitted from the obvious-exits list.
check("east" not in cmd(s, "exits").lower(),
      "the secret east exit is hidden from the obvious-exits list")

out = cmd(s, "search")
check("hidden passage to the east" in out.lower(), f"search reveals the secret exit: {out[:90]!r}")

cmd(s, f"goto {ROOM3}")
out = cmd(s, "search")
check("nothing hidden" in out.lower(), f"search in a plain room turns up nothing: {out[:90]!r}")
cmd(s, "quit!"); s.close()

# --- 4: a character who doesn't know search is refused ---
mname, mpw = f"Scm{_suffix}", "scmpw1234567"
sm = make_char(mname, mpw, 1)  # Mage -- never gets search
cmd(sm, "quit!"); sm.close()
sql(f"UPDATE player SET load_room={ROOM3} WHERE name='{mname}';")
sm = relog(mname, mpw)
check("Plain Sandbox" in cmd(sm, "look"), "the non-thief lands in the sandbox room")
out = cmd(sm, "search")
check("don't know how to search" in out.lower(),
      f"a character without the skill is refused: {out[:80]!r}")
cmd(sm, "quit!"); sm.close()

announce_done("smoke_test_search", host, port)
print("=== ALL CHECKS PASSED ===")
