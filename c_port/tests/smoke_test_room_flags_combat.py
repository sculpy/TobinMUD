#!/usr/bin/env python3
"""Room-flag effects that only combat/regen can exercise -- the second
slice of "make room flags have their intended Sneezy effects" (user
2026-08-16), following smoke_test_room_flags.py's movement slice:

  * ROOM_FLAG_NO_FLEE (bit 12, value 4096) -- a magically-sealed room. A
    fighting mortal who types `flee` is refused with "a strange power
    prevents you from escaping" (cmd_flee.c). Distinct from NO_ESCAPE
    (bit 6), which only blocks teleport/recall, not the flee command.
  * ROOM_FLAG_ARENA (bit 14, value 16384) -- a sporting ring. A PC who
    loses a fight here is NOT really killed: knocked to half HP, left
    STANDING in the room ("knocked senseless"), no menu eject, no corpse,
    no XP loss (combat.c's arena-knockout branch in combat_defeat()).
ROOM_FLAG_HOSPITAL's doubled HP/vit/mana recovery (regen.c) is a plain
multiplier inside already-exercised regen paths; score reports health as
words not numbers, so a wall-clock regen delta can't be asserted cleanly
on a live shared box -- it's covered by build + review, not here.

Uses an immortal to build/position and to `load` a mob as a combat
partner; `set <m> hp` to script the arena death deterministically.

    python3 tests/smoke_test_room_flags_combat.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, read_until, drain, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_room_flags_combat", host, port)

# Letters-only unique token (char names reject digits) seeded from both
# the high-res clock AND the pid, so back-to-back reruns never collide on
# names or room vnums the way a pure time-derived suffix does.
import os
_seed = time.time_ns() ^ (os.getpid() << 20)
_sfx = "".join(chr(ord("a") + (_seed // 26 ** i) % 26) for i in range(7))
BASE = 985100 + (_seed % 700)
ORIGIN, NOFLEE, ARENA = BASE, BASE + 1, BASE + 2

LIT = 1
NOFLEE_FLAG = 4096     # bit 12
ARENA_FLAG = 16384     # bit 14
ZOMBIE = 40            # "zombie follower", level 1 -- a punching bag for the flee fight
TANK = 1735            # "testmob l35" -- survives the 1-hp victim so it lands the kill


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


pw = "rfcombatpw1"
imm = f"Rfc{_sfx}i"
mflee = f"Rfc{_sfx}f"
mviu = f"Rfc{_sfx}a"    # arena victim

vnums = (ORIGIN, NOFLEE, ARENA)
sql(f"DELETE FROM room WHERE vnum IN {vnums};")
mkroom(ORIGIN, "RFC Origin", LIT)
mkroom(NOFLEE, "RFC No-Flee Cell", LIT | NOFLEE_FLAG)
mkroom(ARENA, "RFC Arena Ring", LIT | ARENA_FLAG)

sockets = []
try:
    for n in (imm, mflee, mviu):
        make_char(n, pw)
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    for n in (mflee, mviu):
        sql(f"UPDATE player_progress SET level=10, hp=300, max_hp=300, vit=300, max_vit=300 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{n}');")

    si = login(imm, pw); sockets.append(si)
    sf = login(mflee, pw); sockets.append(sf)
    sv = login(mviu, pw); sockets.append(sv)

    # All reads below use cmd_until (drains stale backlog, then reads until
    # the real response text appears) instead of a bare cmd(): combat keeps
    # a socket "warm", so a plain recv would routinely return a round echo
    # before the response we're checking for renders.

    # ---- NO_FLEE: a fighting mortal cannot flee ----
    cmd(si, f"goto {NOFLEE}")
    cmd(si, f"load mob {ZOMBIE}")
    cmd(si, f"transfer {mflee}")
    check("RFC No-Flee Cell" in cmd_until(sf, "look", "RFC No-Flee Cell"),
          "flee-tester is in the NO_FLEE room")
    cmd_until(sf, "kill zombie", "zombie")   # confirm the fight is engaged
    # A single flee issued mid-combat is occasionally dropped by the
    # input/pulse timing, so retry until the refusal shows (or we run out
    # of tries) rather than trusting one send.
    fled = ""
    for _ in range(6):
        send_line(sf, "flee")
        fled += read_until(sf, "escaping", deadline=3.0).lower()
        if "strange power prevents you from escaping" in fled:
            break
    check("strange power prevents you from escaping" in fled,
          "a fighting mortal is refused `flee` in a ROOM_FLAG_NO_FLEE room")
    # End the fight so the room-presence read isn't racing combat spam.
    cmd(si, "purge"); cmd(si, f"restore {mflee}"); drain(sf)
    check("RFC No-Flee Cell" in cmd_until(sf, "look", "RFC No-Flee Cell"),
          "the blocked fleer stays in the NO_FLEE room (flee didn't move them)")

    # ---- ARENA: a defeated PC is knocked out, not killed ----
    cmd(si, f"goto {ARENA}")
    cmd(si, f"load mob {TANK}")   # a lvl-35 tank the 1-hp victim can't kill first
    cmd(si, f"transfer {mviu}")
    check("RFC Arena Ring" in cmd_until(sv, "look", "RFC Arena Ring"),
          "arena victim is in the ARENA room")
    # 1 hp so the tank's first retaliation drops them; max 300 so the
    # knockout has something to heal back to half of.
    cmd(si, f"set {mviu} hp 1 300")
    ko = cmd_until(sv, "kill testmob", "knocked senseless", deadline=25.0).lower()
    check("knocked senseless" in ko,
          "a PC defeated in a ROOM_FLAG_ARENA room is knocked out, not slain")
    check("you are dead" not in ko and "account menu" not in ko,
          "the arena knockout does NOT run the normal death/menu-eject path")
    # Still in the ring and still playing: a real death would have ejected
    # them to the account menu, so this look would time out instead.
    cmd(si, "purge"); drain(sv)
    check("RFC Arena Ring" in cmd_until(sv, "look", "RFC Arena Ring"),
          "the knocked-out PC is still standing in the arena, not ejected")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum IN {vnums};")
    # Delete the throwaway test characters too, so repeated runs don't
    # pile up orphan player rows (and can't collide on a reused name).
    for n in (imm, mflee, mviu):
        try:
            sql(f"DELETE FROM player WHERE name='{n}';")
        except Exception:
            pass
