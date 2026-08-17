#!/usr/bin/env python3
"""Alignment-based mob assist now fires (data task 2026-08-17: seed cosmic
good/evil alignment onto mob protos -- db/tobin/mob_align.sql).

combat.c combat_recruit_assist() joins an aligned ALLY into a fight when
two mobs share the same NONZERO mob_align (distinct from the KIN path,
which keys off identical prototype vnum). Every proto shipped with align=0,
so this branch was provably dead code. mob_align.sql set undead/fiends to
-100 and celestials to +100.

This loads TWO DIFFERENT evil undead protos into a room -- a devilish
scarecrow (5431) and a zombie adventurer (12428), distinct vnums (so the
KIN branch can NOT explain any assist) but both align=-100 -- and has a
sturdy (9999 HP) but LOW-damage mortal attack ONE of them, so the fight
lasts many rounds and the ~35%/round alignment roll on the other has
several chances to land ("rushes to ...'s aid and joins the attack on
you"). A control check confirms the two are not the same vnum.

    python3 tests/smoke_test_mob_align_assist.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mob_align_assist", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 985400 + (int(time.time()) % 50)
EVIL_A = 5431    # a devilish scarecrow (undead, align -100), keyword "scarecrow"
EVIL_B = 12428   # a zombie adventurer  (undead, align -100) -- DIFFERENT vnum => not kin

check(EVIL_A != EVIL_B, "the two allies are DIFFERENT vnums -- any assist is alignment, not kin")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    create_character(s, name, send_line, recv_all, race="1", territory="1", char_class=cls)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "alignpw123"
imm = f"Alimm{_suffix}"
vic = f"Alvic{_suffix}"

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Align Arena','A bare sparring floor.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(imm, pw, "3")
    make_char(vic, pw, "3")
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    # Low level -> low damage -> the fight lasts many rounds (many assist
    # rolls); 9999 HP so the attacker never dies to the mobs' return blows.
    sql(f"UPDATE player_progress SET level=5, hp=9999, max_hp=9999, vit=9999, max_vit=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")

    si = login(imm, pw); sockets.append(si)
    sv = login(vic, pw); sockets.append(sv)

    cmd(si, f"goto {ROOM}")
    cmd(si, f"load mob {EVIL_A}")
    cmd(si, f"load mob {EVIL_B}")
    recv_all(si, 0.4)
    cmd(si, f"transfer {vic}")
    check("Align Arena" in cmd(sv, "look"), "mortal attacker is in the arena with the two aligned mobs")

    # Attack the scarecrow; the zombie (same align, different vnum) should join.
    send_line(sv, "kill scarecrow")
    buf = ""
    t0 = time.time()
    while time.time() - t0 < 25:
        buf += recv_all(sv, 0.5)
        if "joins the attack on you" in strip(buf):
            break
    out = strip(buf)
    joined = "joins the attack on you" in out and "rushes to" in out
    check(joined,
          "the aligned ally (different-vnum zombie, same align=-100) rushed in and joined the "
          "attack -- the previously-inert alignment-assist branch now fires")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
