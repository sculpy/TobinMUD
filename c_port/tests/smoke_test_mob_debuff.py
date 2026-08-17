#!/usr/bin/env python3
"""Caster-mob per-spell affect fidelity (2026-08-17): a caster mob now
inflicts its class debuffs (blindness/curse/fear/silence) as the SAME
affect the PC cast applies, instead of resolving every spell as raw damage.

combat.c mob_cast_combat() gained a debuff pass: ~45% of the caster rounds
where a fresh debuff is available, the mob applies it (via mob_apply_debuff)
rather than a damage spell. Mage mobs draw from {fear, faerie fog, silence},
Cleric mobs from {curse, blindness}; each names a real roster spell gated by
the mob's level and mirrors the PC affect + magnitude.

This spawns the same level-45 Mage mob the caster-combat test uses (6454,
"visiting student"), which qualifies for fear (14) and faerie fog / blind
(18) but NOT silence (48). A high-HP low-threat mortal engages it so it
survives many rounds; over the window we require at least one debuff line
"<mob> intones <fear|faerie fog> at you, ...!" -- the message mob_apply_debuff
emits immediately after applying the affect.

    python3 tests/smoke_test_mob_debuff.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mob_debuff", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 977500 + (int(time.time()) % 400)
MAGE_MOB = 6454  # "visiting student", level 45, class=1 (Mage)

DEBUFF_SPELLS = ["fear", "faerie fog"]   # the two a level-45 Mage mob qualifies for
debuff_re = re.compile(r"intones ([a-z' ]+?) at you", re.I)


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


pw = "mobdbfpw123"
imm = f"Mdimm{_suffix}"
vic = f"Mdvic{_suffix}"

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Mob Debuff Arena','A bare sparring floor.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(imm, pw, "3")
    make_char(vic, pw, "3")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{imm}';")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic}';")
    sql(f"UPDATE player_progress SET level=60, true_level=60, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    # Level 40 so it can't one-shot the level-45 mob; 9999 HP soaks the fight.
    sql(f"UPDATE player_progress SET level=40, hp=9999, max_hp=9999, vit=9999, max_vit=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")

    si = login(imm, pw); sockets.append(si)
    sv = login(vic, pw); sockets.append(sv)

    debuff_line = None
    for attempt in range(6):
        cmd(si, f"load mob {MAGE_MOB}")
        recv_all(si, 0.4); recv_all(sv, 0.4)
        send_line(sv, "kill student")
        buf = ""
        t0 = time.time()
        while time.time() - t0 < 16:
            buf += recv_all(sv, 0.5)
            if debuff_re.search(strip(buf)):
                break
        m = debuff_re.search(strip(buf))
        if m:
            debuff_line = m.group(0)
            break
        time.sleep(0.5)
        recv_all(sv, 0.5); recv_all(si, 0.5)

    check(debuff_line is not None,
          f"a Mage mob inflicted a class DEBUFF spell (not just damage) during combat "
          f"(matched: {debuff_line!r})")
    if debuff_line:
        named = any(sp in debuff_line.lower() for sp in DEBUFF_SPELLS)
        check(named,
              f"the debuff was a genuine roster spell the mob qualifies for at level 45 "
              f"(fear/faerie fog), not a made-up name (line: {debuff_line!r})")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
