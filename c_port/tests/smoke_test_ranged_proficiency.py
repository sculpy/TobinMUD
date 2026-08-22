#!/usr/bin/env python3
"""Smoke test for `ranged proficiency` (all 6 classes) and `ranged
specialization` (Warrior only) actually applying a combat bonus via
`shoot` (TODO.md's "Common skills" backlog item). See cmd_shoot.c's own
doc comment for the damage-only shape (no hitroll analog -- `shoot` has
no separate to-hit roll to begin with) and combat.c's updated comment
on why the melee proficiency/specialization block itself never trains
or applies these two skills (weapon_verb() has no "ranged" bucket).

Proves the learn-by-doing hook actually FIRES for both skills on a real
shot (same "starts at 0, one real attempt trains it to the floor"
guarantee smoke_test_combat_passives_generic.py / smoke_test_advanced_
berserking.py already exercise for their own passives), and that both
help topics load real bodies.

    python3 tests/smoke_test_ranged_proficiency.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
CLASS_WARRIOR = 2  # being.h: MAGE=0, CLERIC=1, WARRIOR=2 (0-indexed)
ROOM = 977500 + (int(time.time()) % 1000)
BOW, ARROW = 170, 166


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", cls, "done", "done"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "quit!")  # clean logout -- a raw close() resumes the OLD
    s.close()          # in-memory character on relog, ignoring DB updates.


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_ranged_proficiency", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Ranged Prof Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Rpimm{_suffix}", "rpimmpw12345"
make_char(imm_name, imm_pw, "1")
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

war_name, war_pw = f"Rpwar{_suffix}", "rpwarpw12345"
make_char(war_name, war_pw, "3")  # Warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war_name}';")
# ADVANCED tier (both ranged proficiency and ranged specialization) needs
# basic/combat at 100 and advanced_disc_pct > 0.
sql(f"UPDATE player_progress SET level=40, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100, hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{war_name}');")
sw = relog(war_name, war_pw)

sockets = [si, sw]
try:
    check(skill_pct(war_name, "ranged proficiency") == 0, "ranged proficiency starts untrained")
    check(skill_pct(war_name, "ranged specialization") == 0, "ranged specialization starts untrained")

    cmd(si, f"load obj {BOW}")
    cmd(si, f"give bow {war_name}")
    cmd(si, f"load obj {ARROW}")
    cmd(si, f"give arrow {war_name}")
    cmd(sw, "wield bow")
    cmd(si, "load mob 1735")  # a real seeded tank mob, same one smoke_test_shoot.py uses
    recv_all(sw, 0.3)

    out = cmd(sw, "shoot testmob", 1.5)
    check("You loose" in out, "the shot actually lands")

    check(skill_pct(war_name, "ranged proficiency") >= 1,
          "ranged proficiency trains from a real shot fired")
    check(skill_pct(war_name, "ranged specialization") >= 1,
          "ranged specialization (Warrior) trains from a real shot fired")

    # --- help ---
    check("shoot" in cmd(si, "help ranged proficiency").lower(),
          "help ranged proficiency describes the real mechanic")
    check("shoot" in cmd(si, "help ranged specialization").lower(),
          "help ranged specialization describes the real mechanic")

    announce_done("smoke_test_ranged_proficiency", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Rp%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Rp%{_suffix}');")
    sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Rp%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Rp%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
