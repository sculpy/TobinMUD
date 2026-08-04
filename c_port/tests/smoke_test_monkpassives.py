#!/usr/bin/env python3
"""Smoke test for jirin/kubo/oomlat (spell/skill functional-completeness
audit continued, Monk passive combat-math modifiers -- see combat.c's own
header comments at each hook point for the real-upstream research and
scope-down rationale). Unlike every other level-1 skill in this audit,
these three are wired directly into combat_strike()'s to-hit/damage
formula, not a new cmd_*.c -- there's no discrete "you succeeded" message
for kubo/oomlat to check deterministically, so those two are verified
statistically (a moderate, fixed dex mismatch gives a known baseline hit
rate; the skill's own ~12-point modifier shift should reliably move a
40-round hit count in the expected direction). This carries a small,
disclosed flake risk -- same character as every other probabilistic
combat check in this codebase (e.g. smoke_test_skillcombat.py's own
20-round parry check) -- not a bug if it's rare.

    python3 tests/smoke_test_monkpassives.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_monkpassives", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MONK = 5


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_discs(name):
    sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


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
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_pair(prefix, room=None):
    nameA, pwA = f"{prefix}a{_suffix}", f"{prefix}apw12345"
    nameB, pwB = f"{prefix}b{_suffix}", f"{prefix}bpw12345"
    sA0 = make_char(nameA, pwA)
    sB0 = make_char(nameB, pwB)
    cmd(sA0, "quit!"); sA0.close()
    cmd(sB0, "quit!"); sB0.close()
    for nm in (nameA, nameB):
        set_class(nm, CLASS_MONK)
        set_hp(nm, 5000, 5000)
        set_discs(nm)
        if room is not None:
            sql(f"UPDATE player SET load_room={room} WHERE name='{nm}';")
    sA = relog(nameA, pwA)
    sB = relog(nameB, pwB)
    cmd(sA, "toggle pk"); cmd(sB, "toggle pk")
    return (nameA, sA), (nameB, sB)


def attack_and_settle(sock, target_name):
    cmd(sock, f"attack {target_name}")
    time.sleep(1.3)


def run_rounds(sock, rounds):
    """Sleeps through `rounds` more combat exchanges (auto-continuing
    once `fighting` is set, same as smoke_test_skillcombat.py's own
    parry loop), returning the accumulated stripped text."""
    full = ""
    for _ in range(rounds):
        time.sleep(1.3)
        full += strip(recv_all(sock, 0.3))
    return full


ROOM = 974000 + (int(time.time()) % 10000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Monk Passives Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    # =================== 1. jirin (discrete, same shape as parry) ===================
    (nameA, sA), (nameB, sB) = make_pair("Jrn", room=ROOM)
    sockets += [sA, sB]
    seed_proficiency(nameB, "jirin", 100)
    attack_and_settle(sA, nameB)
    out = run_rounds(sA, 20) + run_rounds(sB, 0)
    check("deflect" in out.lower(),
          "a 100%-proficiency Monk eventually deflects an unarmed hit with jirin within 20 forced rounds")
    sA.close(); sB.close()

    # =================== 2. kubo (unarmed to-hit + damage bonus) ===================
    # Fixed -45 dex-derived modifier (clamps to combat.c's own -44 floor)
    # gives a known, reproducible ~6% baseline hit chance with 0% kubo;
    # kubo's own +12ish (100/8) modifier bonus un-clamps it to ~17%.
    (nameC, sC), (nameD, sD) = make_pair("Kbz", room=ROOM)  # control, no kubo
    sockets += [sC, sD]
    set_dex(nameC, 70); set_dex(nameD, 250)
    attack_and_settle(sC, nameD)
    out_control = run_rounds(sC, 40)
    control_misses = out_control.lower().count(f"you miss {nameD.lower()}")
    sC.close(); sD.close()

    (nameE, sE), (nameF, sF) = make_pair("Kbw", room=ROOM)  # kubo 100%
    sockets += [sE, sF]
    set_dex(nameE, 70); set_dex(nameF, 250)
    seed_proficiency(nameE, "kubo", 100)
    attack_and_settle(sE, nameF)
    out_kubo = run_rounds(sE, 40)
    kubo_misses = out_kubo.lower().count(f"you miss {nameF.lower()}")
    sE.close(); sF.close()

    kubo_hits = 40 - kubo_misses
    control_hits = 40 - control_misses
    check(kubo_hits > control_hits,
          f"100%-kubo lands more unarmed hits over 40 rounds than a 0%-kubo control "
          f"({kubo_hits} vs {control_hits})")

    # =================== 3. oomlat (unarmed AC/to-hit-denial bonus) ===================
    # Same dex mismatch, reversed roles: a fixed +45-derived modifier
    # (clamped to +44) gives the ATTACKER a known ~94% baseline hit
    # chance against a 0%-oomlat defender; oomlat's own -12ish bonus
    # should pull that down measurably over 40 rounds.
    (nameG, sG), (nameH, sH) = make_pair("Omz", room=ROOM)  # control, defender has no oomlat
    sockets += [sG, sH]
    set_dex(nameG, 250); set_dex(nameH, 70)
    attack_and_settle(sG, nameH)
    out_control2 = run_rounds(sG, 40)
    control_misses2 = out_control2.lower().count(f"you miss {nameH.lower()}")
    sG.close(); sH.close()

    (nameI, sI), (nameJ, sJ) = make_pair("Omw", room=ROOM)  # defender has oomlat 100%
    sockets += [sI, sJ]
    set_dex(nameI, 250); set_dex(nameJ, 70)
    seed_proficiency(nameJ, "oomlat", 100)
    attack_and_settle(sI, nameJ)
    out_oomlat = run_rounds(sI, 40)
    oomlat_misses = out_oomlat.lower().count(f"you miss {nameJ.lower()}")
    sI.close(); sJ.close()

    check(oomlat_misses > control_misses2,
          f"a 100%-oomlat defender gets missed more often over 40 rounds than a "
          f"0%-oomlat control ({oomlat_misses} vs {control_misses2})")

    sockets = []
    announce_done("smoke_test_monkpassives", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Jrn", "Kbz", "Kbw", "Omz", "Omw"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
