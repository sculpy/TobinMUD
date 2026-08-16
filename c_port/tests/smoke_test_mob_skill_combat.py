#!/usr/bin/env python3
"""Non-caster mobs use their combat skills (user 2026-08-16, second half of
"mobs use skills/spells available to them" -- the Warrior/Thief/Monk half;
the caster half is smoke_test_mob_caster_combat.py).

combat.c's new mob_skill_combat() gives a Warrior/Thief/Monk mob a ~1-in-3
per-round chance to use a real class maneuver (Warrior: bash/kick/trip;
Monk: kick; Thief: knife stab) against its PC opponent, on top of its
melee, reusing the same str/dex-scaled damage + knockdown effect the PC
commands (cmd_bash/cmd_kick/cmd_trip) apply.

This pins the Warrior case end to end: a real Warrior mob (vnum 1745,
"testmob l45", level 45, class=4 -> Tobin CLASS_WARRIOR) actually uses one
of its maneuvers during a fight. Setup mirrors the caster test -- a
level-60 immortal spawns the mob, a high-HP mortal (level 40, hp 9999)
engages it and soaks the incoming blows long enough to observe a maneuver.
We require at least one Warrior maneuver line ("bashes into you" /
"boots you in the head" / "sweeps your legs out from under you"). The mob
is reloaded and the fight restarted for a few attempts if the mortal's
melee kills it before a maneuver lands.

    python3 tests/smoke_test_mob_skill_combat.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mob_skill_combat", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 976000 + (int(time.time()) % 1000)
WAR_MOB = 1745  # "testmob l45", level 45, class=4 (Warrior)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw, char_class):
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
    send_line(s, char_class); recv_all(s)
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


pw = "mobskillpw123"
imm = f"Msimm{_suffix}"
vic = f"Msvic{_suffix}"

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Mob Skill Arena','A bare sparring floor.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(imm, pw, "3")
    make_char(vic, pw, "3")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{imm}';")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic}';")
    sql(f"UPDATE player_progress SET level=60, true_level=60, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=40, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")

    si = login(imm, pw); sockets.append(si)
    sv = login(vic, pw); sockets.append(sv)

    maneuver_re = re.compile(
        r"(bashes into you, knocking you to the ground|"
        r"boots you in the head|"
        r"sweeps your legs out from under you)", re.I)

    hit_line = None
    for attempt in range(5):
        cmd(si, f"load mob {WAR_MOB}")
        recv_all(si, 0.4); recv_all(sv, 0.4)
        send_line(sv, "kill testmob")
        buf = ""
        t0 = time.time()
        while time.time() - t0 < 15:
            buf += recv_all(sv, 0.5)
            if maneuver_re.search(strip(buf)):
                break
        m = maneuver_re.search(strip(buf))
        if m:
            hit_line = m.group(0)
            break
        time.sleep(0.5)
        recv_all(sv, 0.5); recv_all(si, 0.5)

    check(hit_line is not None,
          f"a Warrior mob used a real combat maneuver on its opponent during a fight "
          f"(matched: {hit_line!r})")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
