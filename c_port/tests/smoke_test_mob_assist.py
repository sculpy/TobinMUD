#!/usr/bin/env python3
"""Mobs assist their friends / kin join a fight (user 2026-08-16: "Mobs
should have a chance to join the fights depending on alignment, assist
friends" + "Guard mobs ... assist each other. When you attack a guard you
attack ALL guards").

combat.c's new combat_recruit_assist() pulls a beleaguered mob's allies
into the fight: KIN (same prototype vnum -- how identical guards recognize
each other) always pile in, same-ALIGNMENT allies join ~half the time.
They become EXTRA attackers on the PC, resolved by the new multi-attacker
pass in combat_process_run() (Tobin's 1v1 `fighting` pointer means the PC
still swings at one foe, but can now be ganged up on).

This loads THREE copies of one mob (vnum 108, "citizen male" -- kin of one
another) into a room, has a high-HP mortal attack ONE of them, and checks
the other two announce they rush to its aid and join the attack -- i.e. an
attack on one guard/kin drew all of them in.

    python3 tests/smoke_test_mob_assist.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mob_assist", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 985300 + (int(time.time()) % 50)
KIN_MOB = 108   # "citizen male", level 4 -- three copies are kin of each other


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw, cls):
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
    send_line(s, cls); recv_all(s)
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


pw = "assistpw123"
imm = f"Asimm{_suffix}"
vic = f"Asvic{_suffix}"

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Assist Arena','A bare sparring floor.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(imm, pw, "3")
    make_char(vic, pw, "3")
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=45, hp=9999, max_hp=9999, vit=9999, max_vit=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")

    si = login(imm, pw); sockets.append(si)
    sv = login(vic, pw); sockets.append(sv)

    cmd(si, f"goto {ROOM}")
    for _ in range(3):
        cmd(si, f"load mob {KIN_MOB}")   # three kin copies of vnum 108
    recv_all(si, 0.4)
    cmd(si, f"transfer {vic}")           # bring the mortal into the room
    check("Assist Arena" in cmd(sv, "look"), "mortal attacker is in the arena with the three kin mobs")

    # Attack ONE citizen -- its two kin should rush in.
    send_line(sv, "kill citizen")
    buf = ""
    t0 = time.time()
    while time.time() - t0 < 8:
        buf += recv_all(sv, 0.5)
        if strip(buf).count("joins the attack on you") >= 2:
            break
    out = strip(buf)
    joins = out.count("joins the attack on you")

    check(joins >= 1,
          f"attacking one mob drew a kin mob in to assist (saw {joins} 'joins the attack' message(s))")
    check(joins >= 2,
          f"BOTH kin copies joined the fight -- attacking one of a set pulls in all of them "
          f"(saw {joins} join messages, expected 2)")
    check("rushes to" in out,
          "the assist is announced with the 'rushes to X's aid' flavor")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
