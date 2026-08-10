#!/usr/bin/env python3
"""Smoke test for the generic combat-passive batch (missing-skill audit,
generic/cross-class, 2026-08-10): offense, advanced offense, advanced
defense, tactics, inevitability, and fast heal. See skill.c/combat.c/
regen.c.

Proves each passive's learn-by-doing hook actually FIRES in real play
(skill starts at 0; a real swing / a real hit taken / a real rest tick
trains it to the floor -- the deterministic first-attempt guarantee
skill_learn_from_doing() documents), and that every new help topic loads.

    python3 tests/smoke_test_combat_passives_generic.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 953000 + (int(time.time()) % 30000)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
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


def set_level_class(name, level, cls):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=500, max_hp=500 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def poke_fight(sock, target_name, settle=1.6):
    send_line(sock, f"hit {target_name}")
    time.sleep(settle)
    recv_all(sock, 0.2)


announce("smoke_test_combat_passives_generic", host, port)

imm_name, imm_pw = f"Cpimm{_suffix}", "cpimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Passive Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# =====================================================================
# 1: attacker-side passives (offense/tactics/advanced offense/
#    inevitability) train from a real swing the PC lands.
# =====================================================================
at_name, at_pw = f"Cpat{_suffix}", "cpatpw12345"
sat = make_char(at_name, at_pw)
cmd(sat, "quit!"); sat.close()
set_level_class(at_name, 40, 2)  # Warrior, level 40 (>=30 so advanced-tier known)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{at_name}';")
sat = relog(at_name, at_pw)
for sk in ("offense", "tactics", "advanced offense", "inevitability"):
    check(skill_pct(at_name, sk) == 0, f"{sk} starts untrained")

cmd(si, f"goto {ROOM}")
poke_fight(sat, imm_name, settle=1.8)   # PC attacks the immortal -> PC is attacker
time.sleep(1.2)
recv_all(sat, 0.3)
for sk in ("offense", "tactics", "advanced offense", "inevitability"):
    check(skill_pct(at_name, sk) >= 1, f"{sk} trains from a real swing the PC lands")
cmd(sat, "quit!"); sat.close()

# =====================================================================
# 2: advanced defense (defender-side) trains from a real hit taken.
# =====================================================================
df_name, df_pw = f"Cpdf{_suffix}", "cpdfpw12345"
sdf = make_char(df_name, df_pw)
cmd(sdf, "quit!"); sdf.close()
set_level_class(df_name, 40, 2)  # Warrior, level 40
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{df_name}';")
sql(f"UPDATE player_progress SET hp=500, max_hp=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{df_name}');")
sdf = relog(df_name, df_pw)
check(skill_pct(df_name, "advanced defense") == 0, "advanced defense starts untrained")
cmd(si, f"goto {ROOM}")
poke_fight(si, df_name, settle=1.8)      # immortal attacks the PC -> PC is defender
time.sleep(1.0)
recv_all(sdf, 0.3)
check(skill_pct(df_name, "advanced defense") >= 1,
      "advanced defense trains from a real hit taken")
cmd(sdf, "quit!"); sdf.close()

# =====================================================================
# 3: fast heal trains from a real rest tick while wounded.
# =====================================================================
fh_name, fh_pw = f"Cpfh{_suffix}", "cpfhpw12345"
sfh = make_char(fh_name, fh_pw)
cmd(sfh, "quit!"); sfh.close()
set_level_class(fh_name, 20, 2)  # Warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{fh_name}';")
# wounded: current hp well below max so was_short_hp is true on the next tick
sql(f"UPDATE player_progress SET hp=50, max_hp=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{fh_name}');")
sfh = relog(fh_name, fh_pw)
check(skill_pct(fh_name, "fast heal") == 0, "fast heal starts untrained")
cmd(sfh, "rest")
time.sleep(6.0)          # let a couple of regen ticks land while wounded + resting
recv_all(sfh, 0.3)
check(skill_pct(fh_name, "fast heal") >= 1,
      "fast heal trains from a real rest tick taken while wounded")
cmd(sfh, "quit!"); sfh.close()

# =====================================================================
# 4: every new help topic loads.
# =====================================================================
for topic in ("offense", "advanced offense", "advanced defense",
              "tactics", "inevitability", "fast heal"):
    out = strip(cmd(si, f"help {topic}", timeout=1.5))
    check("passive" in out.lower(),
          f"help {topic} loads a real body: {out[:60]!r}")

si.close()
announce_done("smoke_test_combat_passives_generic", host, port)
print("=== ALL CHECKS PASSED ===")
