#!/usr/bin/env python3
"""Regression test for a user bug report: "animal races should not have
wealth, that doesnt make sense." AskUserQuestion-confirmed scope with
the user: gate the existing mob gold-drop-on-kill (combat.c's
combat_defeat()) by the mob's upstream mob.race column being a mundane
real-world creature (RODENT, FELINE, BEAR, DEER, BIRD, ...) rather than
touching Tobin's own 6 PLAYER races (Human/Elf/Ogre/Dwarf/Hobbit/Gnome,
none of which are animals) -- see mob_race_is_animal() (being.c) and
being_t.mob_race, newly copied from mob.race at spawn time
(being_create_mob(), previously wholly deferred/display-only).

Covers:
  1. Killing a RODENT-race (animal) mob awards 0 gold -- the PC's wallet
     is unchanged before/after.
  2. Killing an otherwise-identical NORACE (non-animal) mob still
     awards gold as before -- confirms the gate didn't break the
     ordinary case.
  3. XP is awarded in BOTH cases -- only the gold-wallet drop is gated,
     not XP.

    python3 tests/smoke_test_animal_no_gold.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done, drain

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_animal_no_gold", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 940000 + (int(time.time()) % 50000)
MOB_ANIMAL_VNUM = ROOM + 1
MOB_NORMAL_VNUM = ROOM + 2


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def xp_of(name):
    return int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_gold(name, amount):
    sql(f"UPDATE player_progress SET gold={amount} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "3"); recv_all(s)  # class: warrior (reliable damage)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Animal Gold Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


def mob_row(vnum, name_tag, race):
    """A weak, low-HP mob (mob.tohit/damage_level aren't wired into
    combat.c yet -- being_create_mob() derives everything from level
    instead -- so the PC's own HP is padded way up separately to
    guarantee it survives the fight regardless of what the mob lands).
    Only `race` differs between the two rows."""
    cols = {
        "vnum": vnum, "name": f"'{name_tag}'", "short_desc": f"'a {name_tag}'",
        "long_desc": f"'A {name_tag} is here.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 3, "tohit": 0, "ac": 0, "hpbonus": -1.7,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": race,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 3,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    return f"INSERT INTO mob ({col_names}) VALUES ({col_values});"


sql(mob_row(MOB_ANIMAL_VNUM, "goldrodent", 41))   # RODENT -- animal
sql(mob_row(MOB_NORMAL_VNUM, "goldnorace", 0))    # NORACE -- not animal


def fight_and_check(imm_sock, pc_sock, pc_name, vnum, mob_tag):
    cmd(imm_sock, f"load mob {vnum}")
    out = cmd(pc_sock, f"attack {mob_tag}")
    for _ in range(10):
        if "You have slain" in out or "You have defeated" in out:
            break
        out += recv_all(pc_sock, 1.5)
    check("You have slain" in out or "You have defeated" in out,
          f"the fight against {mob_tag} resolved with a kill")
    # Gold lands in the CORPSE as a real lootable OBJ_CAT_MONEY object
    # (2026-07-28 redesign, combat.c) -- it is not auto-credited to the
    # wallet on kill. autoloot is an opt-in toggle (default off,
    # smoke_test_autoloot.py), so this must loot explicitly or the
    # gold-awarded checks below always see 0 regardless of whether the
    # underlying drop actually worked (found live, Session 189 --this
    # test was silently failing on the non-animal case for that exact
    # reason before this fix).
    cmd(pc_sock, "get all corpse")
    # Combat/loot broadcasts (room echoes, corpse messages) land on every
    # occupant of the room, including the immortal (imm_sock) who is
    # standing right there -- an undrained backlog on THAT socket
    # desyncs its next cmd() call the same way mud_test_utils.py's
    # drain() doc comment warns about for a just-fought PC socket (see
    # smoke_test_corpse.py). Found live: the immortal socket occasionally
    # came back showing a stale account-menu screen on the very next
    # set-hp call for the SECOND fighter, meaning that set command
    # never actually landed and the second fight lost to real combat
    # damage against un-padded HP.
    drain(imm_sock)
    return out


imm_name, imm_pw = f"Golimm{_suffix}", "golimmpw123"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

nameA, pwA = f"Golanim{_suffix}", "golanimpw123"
sA = make_char(nameA, pwA)
cmd(sA, "quit!"); sA.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameA}';")
set_gold(nameA, 0)
sA = relog(nameA, pwA)
cmd(si, f"set {nameA} hp 5000 5000")

before_gold, before_xp = gold_of(nameA), xp_of(nameA)
fight_and_check(si, sA, nameA, MOB_ANIMAL_VNUM, "goldrodent")
after_gold, after_xp = gold_of(nameA), xp_of(nameA)
check(after_gold == before_gold, "killing an animal-race (RODENT) mob awards 0 gold")
check(after_xp > before_xp, "killing an animal-race mob still awards XP")

nameB, pwB = f"Golnorm{_suffix}", "golnormpw123"
sB = make_char(nameB, pwB)
cmd(sB, "quit!"); sB.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameB}';")
set_gold(nameB, 0)
sB = relog(nameB, pwB)
cmd(si, f"set {nameB} hp 5000 5000")

before_gold2, before_xp2 = gold_of(nameB), xp_of(nameB)
fight_and_check(si, sB, nameB, MOB_NORMAL_VNUM, "goldnorace")
after_gold2, after_xp2 = gold_of(nameB), xp_of(nameB)
check(after_gold2 > before_gold2, "killing a non-animal (NORACE) mob still awards gold as before")
check(after_xp2 > before_xp2, "killing a non-animal mob still awards XP")

cmd(sA, "quit!"); sA.close()
cmd(sB, "quit!"); sB.close()
si.close()
announce_done("smoke_test_animal_no_gold", host, port)
print("=== ALL CHECKS PASSED ===")
