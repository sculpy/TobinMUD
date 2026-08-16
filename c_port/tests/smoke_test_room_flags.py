#!/usr/bin/env python3
"""Room-flag effects on movement (user 2026-08-16: "make room flags and
sector types have intended affects from sneezy"). First slice -- the two
clearly-inert, low-blast-radius flags that only movement can exercise:

  * ROOM_FLAG_DEATH  (bit 1, value 2)   -- a death-trap room. A mortal who
    walks in is slain on the spot (combat_death_room_kill_pc, combat.c) and
    ejected to the account menu; an immortal passes through unharmed.
  * ROOM_FLAG_PRIVATE (bit 9, value 512) -- holds at most two players; a
    third mortal is refused entry, but once it drops back to one the next
    walker gets in. Immortals ignore the cap.

Both effects live in cmd_move.c's do_move(); this walks real characters
through real exits to exercise them (goto bypasses do_move, so it can't
test these -- goto is only used to POSITION immortal setup characters).

    python3 tests/smoke_test_room_flags.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_room_flags", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
BASE = 985000 + (int(time.time()) % 80)   # empty block in the proven test-room range
ORIGIN, DEATHR, PRIV = BASE, BASE + 1, BASE + 2

LIT = 1
DEATH_FLAG = 2       # bit 1
PRIVATE_FLAG = 512   # bit 9


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)   # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def mkroom(vnum, name, flag):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare test room.\\n',NULL,{flag},0,0,0,0,0,0,0,0,0);")


def mkexit(vnum, direction, dest):
    sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
        f"lock_difficulty,weight,key_num,destination) VALUES "
        f"({vnum},{direction},'','',0,0,0,0,0,{dest});")


pw = "roomflagpw1"
imm = f"Rfimm{_suffix}"    # immortal: setup + immortal-survives-death check + private occupant
occ = f"Rfocc{_suffix}"    # immortal: 2nd private occupant
mortd = f"Rfmd{_suffix}"   # mortal: death-trap victim
mortp = f"Rfmp{_suffix}"   # mortal: private-room walker

# Defensive: clear any leftover rows at these vnums from an aborted run.
sql(f"DELETE FROM roomexit WHERE vnum IN ({ORIGIN},{DEATHR},{PRIV});")
sql(f"DELETE FROM room WHERE vnum IN ({ORIGIN},{DEATHR},{PRIV});")

# Rooms first (mortals' load_room points at ORIGIN, so it must exist at login).
mkroom(ORIGIN, "Flag Test Origin", LIT)
mkroom(DEATHR, "Flag Test Deathtrap", LIT | DEATH_FLAG)
mkroom(PRIV, "Flag Test Private", LIT | PRIVATE_FLAG)
mkexit(ORIGIN, 0, DEATHR)   # north -> deathtrap
mkexit(DEATHR, 2, ORIGIN)   # south back
mkexit(ORIGIN, 1, PRIV)     # east -> private
mkexit(PRIV, 3, ORIGIN)     # west back

sockets = []
try:
    for n in (imm, occ, mortd, mortp):
        make_char(n, pw)
    # Immortals (goto-capable); mortals spawn straight into ORIGIN.
    for n in (imm, occ):
        sql(f"UPDATE player_progress SET level=60, true_level=60 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
    for n in (mortd, mortp):
        sql(f"UPDATE player_progress SET level=10, hp=500, max_hp=500, vit=500, max_vit=500 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{n}');")

    si = login(imm, pw); sockets.append(si)
    so = login(occ, pw); sockets.append(so)
    smd = login(mortd, pw); sockets.append(smd)
    smp = login(mortp, pw); sockets.append(smp)

    # Mortals can't `goto`, and login doesn't lazy-load an uncached load_room
    # (it falls back to Center Square), so position mortals by having an
    # immortal `goto` the room (loading it) and `transfer` the mortal in.
    # `goto` also warms the world cache for these fresh rooms.

    # ---- DEATH: a mortal who walks in is slain ----
    cmd(si, f"goto {ORIGIN}")
    cmd(si, f"transfer {mortd}")
    check("Flag Test Origin" in cmd(smd, "look"), "mortal death-tester is in ORIGIN")
    out = cmd(smd, "north")
    check("You have DIED" in out,
          "walking into a ROOM_FLAG_DEATH room kills the mortal on the spot")

    # ---- DEATH: an immortal passes through unharmed ----
    out = cmd(si, "north")   # imm is still in ORIGIN
    check("Flag Test Deathtrap" in out and "You have DIED" not in out,
          "an immortal walks into the death-trap room unharmed and lands in it")

    # ---- PRIVATE: a 3rd player is refused, but a 2nd gets in ----
    cmd(si, f"goto {PRIV}")   # occupant #1
    cmd(so, f"goto {PRIV}")   # occupant #2 -> PRIV now holds 2 players
    cmd(so, f"goto {ORIGIN}"); cmd(so, f"transfer {mortp}"); cmd(so, f"goto {PRIV}")  # park mortp in ORIGIN, occ back to PRIV
    check("Flag Test Origin" in cmd(smp, "look"), "mortal private-walker is in ORIGIN")
    out = cmd(smp, "east")
    check("private" in out.lower() and "Flag Test Private" not in out,
          "a 3rd player is refused entry to a full (2-occupant) PRIVATE room")
    check("Flag Test Origin" in cmd(smp, "look"),
          "the refused walker stays put in ORIGIN")

    cmd(so, f"goto {ORIGIN}")  # occupant #2 leaves -> PRIV now holds just imm (1)
    out = cmd(smp, "east")
    check("Flag Test Private" in out,
          "with only one occupant left, the next walker is allowed into the PRIVATE room")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ORIGIN},{DEATHR},{PRIV});")
    sql(f"DELETE FROM room WHERE vnum IN ({ORIGIN},{DEATHR},{PRIV});")
