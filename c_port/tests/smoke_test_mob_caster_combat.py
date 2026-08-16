#!/usr/bin/env python3
"""Caster mobs cast spells in combat (user 2026-08-16: "Mage and druid
mobs arent casting, mages druids and clerics should be taking advantage of
their spells ... fix it so mobs use skills/spells available to them" --
casters-first slice).

combat.c's new mob_cast_combat() gives a Mage/Druid/Cleric mob a ~1-in-3
per-round chance to throw a real class spell (a roster entry its class +
level qualifies for) at whoever it's fighting, on top of its melee, with
damage from the same spell_damage_for_level() a PC casting that spell
uses.

This pins the load-bearing behavior end to end against a live server: a
real Mage mob (vnum 6454, "visiting student", level 45, class=1 -> Tobin
CLASS_MAGE) actually casts at its opponent during a fight. A level-60
immortal spawns the mob; a separate high-HP MORTAL (level 40, hp 9999)
engages it. The mortal is used deliberately: an immortal attacker would
one-shot the level-45 mob on its own opening strike each round (combat
resolves the PC's strike before the mob ever reaches its cast branch), so
the mob would never get to cast. A level-40 mortal cannot one-shot it, so
the mob survives round after round and casts, while the mortal's 9999 HP
soaks the incoming spells/melee for the duration. Over the window we
require the mob to have cast at least once: a class verb ("hurls", a
Mage's) + a real Mage spell name + "at you". The mob is reloaded and the
fight restarted for a few attempts if the mortal's own melee kills it
before a cast is seen.

    python3 tests/smoke_test_mob_caster_combat.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mob_caster_combat", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 977000 + (int(time.time()) % 1000)
MAGE_MOB = 6454  # "visiting student", level 45, class=1 (Mage)


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


pw = "mobcastpw123"
imm = f"Mcimm{_suffix}"
vic = f"Mcvic{_suffix}"

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Mob Caster Arena','A bare sparring floor.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(imm, pw, "3")   # Warrior body; promoted to immortal below (spawns the mob)
    make_char(vic, pw, "3")   # Warrior; the high-HP mortal that engages the mob
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{imm}';")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic}';")
    sql(f"UPDATE player_progress SET level=60, true_level=60, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=40, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")

    si = login(imm, pw); sockets.append(si)
    sv = login(vic, pw); sockets.append(sv)

    MAGE_SPELLS = ["gust", "flare", "mystic darts", "fireball", "ice storm", "lightning bolt"]
    cast_re = re.compile(r"(hurls|calls down|invokes) ([a-z ]+?) at you", re.I)

    cast_line = None
    for attempt in range(5):
        # (Re)spawn the mage mob into the room (immortal loads it), then the
        # mortal engages it.
        cmd(si, f"load mob {MAGE_MOB}")
        recv_all(si, 0.4); recv_all(sv, 0.4)
        send_line(sv, "kill student")
        buf = ""
        t0 = time.time()
        while time.time() - t0 < 15:     # ~12 combat rounds
            buf += recv_all(sv, 0.5)
            if cast_re.search(strip(buf)):
                break
        out = strip(buf)
        m = cast_re.search(out)
        if m:
            cast_line = m.group(0)
            break
        # Mob died to the mortal's melee before casting -- reload and retry.
        time.sleep(0.5)
        recv_all(sv, 0.5); recv_all(si, 0.5)

    check(cast_line is not None,
          "a Mage mob cast a real spell at its opponent during combat "
          f"(matched: {cast_line!r})")
    if cast_line:
        spell_named = any(sp in cast_line.lower() for sp in MAGE_SPELLS)
        check(spell_named,
              f"the mob cast a genuine Mage roster spell it qualifies for at level 45, "
              f"not a made-up name (line: {cast_line!r})")
        check("hurls" in cast_line.lower(),
              f"the Mage mob used the Mage casting verb 'hurls' (line: {cast_line!r})")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
