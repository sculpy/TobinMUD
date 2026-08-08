#!/usr/bin/env python3
"""Smoke test for the 5 weapon/barehand proficiency skills (slash/blunt/
pierce/barehand -- "ranged" has no attack command yet, so it's untestable
live, same disclosed gap combat.c's own comment notes).

Was: an auto-track special case that mirrored combat_disc_pct 1:1 with
no real combat needed at all (user, 2026-08-03). Replaced (user,
2026-08-08: "all skills/spells should be learn by doing, linked to use
and player stats") with a real per-swing learn-by-doing hook in
combat_strike() (combat.c) -- weapon_verb()'s existing slice/chop/
bludgeon/stab/pierce/hit classification (already used for hit-message
flavor and the Warrior specialization bonus) picks which one of the 5
proficiency skills a given swing exercises, exactly like the
specialization bonus already did. combat_disc_pct is still this skill
tier's ceiling (skill_ceiling()), same relationship discipline has with
every other tier -- just no longer the live value itself.

Covers:
  1. Warrior still knows all 5 (unchanged from the old auto-track test).
  2. At 0% Combat discipline, still locked (unchanged).
  3. At 100% Combat discipline but zero real combat, proficiency now
     reads 0% for all 5 (NOT auto-1%+ like the old behavior) -- the
     ceiling no longer doubles as the live value.
  4. Wielding a sword and landing an attack round raises ONLY "slash
     proficiency" to its 1% floor; the other 3 stay at 0% -- confirms
     weapon_verb()-based targeting, not a blanket bump.
  5. A proficiency already seeded at its discipline ceiling stays there
     even after further combat (skill_learn_from_doing()'s own
     sp.pct >= ceiling short-circuit) -- discipline is a cap, not
     something combat alone can push past.

    python3 tests/smoke_test_proficiency_autotrack.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

PROF_SKILLS = ["slash proficiency", "blunt proficiency", "pierce proficiency", "barehand proficiency"]

CLASS_WARRIOR = 2
WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
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


def make_pair(prefix, room):
    nameA, pwA = f"{prefix}a{_suffix}", f"{prefix}apw12345"
    nameB, pwB = f"{prefix}b{_suffix}", f"{prefix}bpw12345"
    sA0 = make_char(nameA, pwA)
    sB0 = make_char(nameB, pwB)
    cmd(sA0, "quit!"); sA0.close()
    cmd(sB0, "quit!"); sB0.close()
    for nm in (nameA, nameB):
        set_class(nm, CLASS_WARRIOR)
        set_hp(nm, 5000, 5000)
        set_combat_disc(nm, 100)
        sql(f"UPDATE player SET load_room={room} WHERE name='{nm}';")
        sql(f"UPDATE player_progress SET level=20 WHERE player_id="
            f"(SELECT id FROM player WHERE name='{nm}');")
    sA = relog(nameA, pwA)
    sB = relog(nameB, pwB)
    cmd(sA, "toggle pk")
    cmd(sB, "toggle pk")
    return (nameA, pwA, sA), (nameB, pwB, sB)


def attack_and_settle(sock, target_name, rounds=1):
    cmd(sock, f"attack {target_name}")
    time.sleep(1.3 * rounds)


def profs_from_skills_output(out):
    """Parses cmd_practice.c's `  %-26s (%d/%d) %s` listing rows."""
    result = {}
    for m in re.finditer(r"(\w+ proficiency)\s+\((\d+)/(\d+)\)", out):
        result[m.group(1)] = int(m.group(2))
    return result


announce("smoke_test_proficiency_autotrack", host, port)

ROOM = 961000 + (int(time.time()) % 20000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Proficiency Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Proimm{_suffix}", "proimmpw123"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

# --- 1/2: roster + lock behavior (unchanged from the old auto-track) ---
name0, pw0 = f"Proflck{_suffix}", "proflckpw12"
s0 = make_char(name0, pw0)
cmd(s0, "quit!"); s0.close()
set_class(name0, CLASS_WARRIOR)
set_combat_disc(name0, 0)
s0 = relog(name0, pw0)
out = cmd(s0, "practice combat")
for sk in PROF_SKILLS:
    check(sk in out.lower(), f"Warrior's Combat listing includes '{sk}'")
check("unlock" in out.lower(), "at 0% Combat discipline, the skills show as locked")
s0.close()

# --- 3: 100% discipline, zero real combat -> proficiency reads 0, not auto-ceiling ---
(nameA, pwA, sA), (nameB, pwB, sB) = make_pair("Profa", ROOM)
out = cmd(sA, "practice combat")
profs = profs_from_skills_output(out)
for sk in PROF_SKILLS:
    check(profs.get(sk, -1) == 0,
          f"'{sk}' reads 0% at 100% Combat discipline before any real combat (got {profs.get(sk)})")

# --- 4: wielding a sword and landing a round raises ONLY slash proficiency ---
WEAPON_VNUM = ROOM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({WEAPON_VNUM},'sword','a steel sword','A steel sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},5,1);")
cmd(si, f"load obj {WEAPON_VNUM}")
cmd(si, "drop sword")
cmd(sA, "get sword")
cmd(sA, "wield sword")

attack_and_settle(sA, nameB)

out = cmd(sA, "practice combat")
profs = profs_from_skills_output(out)
check(profs.get("slash proficiency", 0) >= 1,
      f"'slash proficiency' reaches its 1% floor after a real swing (got {profs.get('slash proficiency')})")
for sk in ("blunt proficiency", "pierce proficiency", "barehand proficiency"):
    check(profs.get(sk, -1) == 0, f"'{sk}' stays at 0% -- only the swung weapon's own type gains (got {profs.get(sk)})")

sA.close(); sB.close()

# --- 5: a proficiency already at its discipline ceiling can't be pushed past it ---
(nameC, pwC, sC), (nameD, pwD, sD) = make_pair("Profc", ROOM)
cmd(sC, "quit!"); sC.close()
set_combat_disc(nameC, 5)
seed_proficiency(nameC, "slash proficiency", 5)
sC = relog(nameC, pwC)
WEAPON_VNUM2 = WEAPON_VNUM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({WEAPON_VNUM2},'saber','a curved saber','A curved saber is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},5,1);")
cmd(si, f"load obj {WEAPON_VNUM2}")
cmd(si, "drop saber")
cmd(sC, "get saber")
cmd(sC, "wield saber")

attack_and_settle(sC, nameD, rounds=3)

out = cmd(sC, "practice combat")
profs = profs_from_skills_output(out)
check(profs.get("slash proficiency", -1) == 5,
      f"'slash proficiency' stays capped at its 5% discipline ceiling despite further combat (got {profs.get('slash proficiency')})")

sC.close(); sD.close()
si.close()

announce_done("smoke_test_proficiency_autotrack", host, port)
print("=== ALL CHECKS PASSED ===")
