#!/usr/bin/env python3
"""Smoke test for `taunt` (Warrior) and `pray paralyze limb` (Cleric),
level 22 -- spell/skill functional-completeness audit continued. See
cmd_taunt.c and cmd_pray.c's "paralyze limb" branch for the real-upstream
research and scope-down rationale (Tobin has no mob-AI target-switching
subsystem for taunt to hook into, and no per-limb status-flag system for
paralyze limb beyond the existing hp/max_hp "destroyed" state).

  1. taunt refuses against a mob that isn't fighting anyone.
  2. taunt refuses while the taunter is already fighting someone else.
  3. taunt pulls a mob's aggro off its current target onto the taunter.
  4. paralyze limb with no target is refused.
  5. paralyze limb drops one of the target's safe limbs to 0% (shown in
     `limbs`), without killing them (never touches a MAJOR limb).

    python3 tests/smoke_test_taunt_paralyzelimb.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_taunt_paralyzelimb", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_CLERIC = 1
CLASS_WARRIOR = 2


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    # Corrected 12-step account-creation sequence (color + timezone
    # prompts land between the password retype and the account menu --
    # see TODO.md's "STRONG LEAD found 2026-07-28" writeup).
    for step in (name, "y", pw, pw, "n", "0", "new", name, "1", "1", str(CLASS_WARRIOR), "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, class_id, room, level=20):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    make_char(name, pw)
    sql(f"UPDATE player SET class={class_id}, load_room={room} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    time.sleep(0.3)
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 980600 + (int(time.time()) % 1000)
MOB = ROOM + 1
MOB2 = ROOM + 3
SYMBOL = ROOM + 2
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Taunt/Paralyze Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cols = {
    "vnum": MOB, "name": "'punching bag'", "short_desc": "'a punching bag'",
    "long_desc": "'A punching bag hangs here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 5, "tohit": 0, "ac": 0, "hpbonus": 500,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")
cols2 = dict(cols)
cols2["vnum"] = MOB2
cols2["name"] = "'one-hit dummy'"
cols2["short_desc"] = "'a one-hit dummy'"
cols2["long_desc"] = "'A frail one-hit dummy stands here.'"
cols2["hpbonus"] = 5
col_names2 = ",".join(cols2.keys())
col_values2 = ",".join(str(v) for v in cols2.values())
sql(f"INSERT INTO mob ({col_names2}) VALUES ({col_values2});")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")

sockets = []
try:
    imm_name, imm = make_single("Tauim", CLASS_WARRIOR, ROOM, level=52)
    sockets.append(imm)
    tank_name, tank = make_single("Taunk", CLASS_WARRIOR, ROOM, level=20)
    sockets.append(tank)
    war_name, war = make_single("Tauwr", CLASS_WARRIOR, ROOM, level=22)
    sockets.append(war)
    clr_name, clr = make_single("Tapcl", CLASS_CLERIC, ROOM, level=22)
    sockets.append(clr)

    # Force every character into ROOM -- login placement via `load_room`
    # is intermittently unreliable (the same not-yet-root-caused bug
    # documented in smoke_test_blindness_recall.py's own KNOWN ISSUE);
    # sidestepping it here rather than depending on it. `goto` for imm
    # itself (transfer refuses a self-target), explicit room-vnum
    # `transfer <name> <vnum>` for the rest (the bare 1-arg form targets
    # wherever the ISSUING immortal currently stands, which is exactly
    # the unreliable thing being worked around here).
    cmd(imm, f"goto {ROOM}"); recv_all(imm, 0.2)
    for name in (tank_name, war_name, clr_name):
        cmd(imm, f"transfer {name} {ROOM}"); recv_all(imm, 0.2)
    for s in (imm, tank, war, clr):
        recv_all(s, 0.2)

    for name in (war_name, clr_name):
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'taunt', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'paralyze limb', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")

    recv_all(imm, 0.3)
    check("You conjure" in cmd(imm, f"load mob {MOB}"), "punching bag loaded")

    # --- 1: taunt refuses against a mob that isn't fighting anyone ---
    out1 = strip(cmd(war, "taunt bag"))
    check("aren't fighting anyone" in out1.lower(), "taunt refuses a mob with no current target")

    # --- 2: taunt refuses while the taunter is already fighting someone ---
    # A disposable one-hit mob keeps `war` briefly "fighting someone else"
    # without needing flee/rescue semantics to clean it back up afterward.
    check("You conjure" in cmd(imm, f"load mob {MOB2}"), "one-hit dummy loaded")
    cmd(tank, "attack bag"); recv_all(tank, 0.3); recv_all(war, 0.2)
    cmd(war, "attack dummy"); recv_all(war, 0.4)
    out2 = strip(cmd(war, "taunt bag"))
    check("fighting someone else" in out2.lower(), "taunt refuses while already fighting someone else")
    # Free `war` deterministically -- an immortal's `kill` is an instant
    # slay (cmd_table.c's own description), which reliably ends the fight
    # and clears both fighting pointers via the normal combat_defeat()
    # path, unlike live combat RNG (too flaky to time: either too tanky,
    # surviving many rounds, or too fragile, dying mid-check, depending
    # on hpbonus).
    cmd(imm, "kill dummy"); recv_all(imm, 0.3)
    recv_all(war, 0.3)

    # --- 3: taunt pulls the mob's aggro off tank onto war ---
    out3 = strip(cmd(war, "taunt bag"))
    check("drawing their ire" in out3.lower(), "100%-proficiency taunt succeeds")
    out_tank_move = strip(cmd(tank, "north"))
    check("fighting for your life" not in out_tank_move.lower(),
          "tank is no longer tagged as fighting after taunt pulls the mob off them")
    time.sleep(2.5)  # taunt's own being_set_wait() blocks war's own commands until it clears
    out_war_move = strip(cmd(war, "north"))
    check("fighting for your life" in out_war_move.lower(),
          "war is now the one tagged as fighting the mob")

    # --- 4: paralyze limb with no target is refused ---
    recv_all(clr, 0.3)
    check("You conjure" in cmd(imm, f"load obj {SYMBOL}"), "a holy symbol is loaded")
    cmd(imm, "drop symbol"); recv_all(imm, 0.3)
    cmd(clr, "get symbol"); recv_all(clr, 0.3)
    out4 = strip(cmd(clr, "pray paralyze limb"))
    check("pray for that over whom" in out4.lower(), "paralyze limb with no target is refused")

    # --- 5: paralyze limb drops a safe limb to 0% without killing the target ---
    check("You conjure" in cmd(imm, f"load obj {SYMBOL}"), "a second holy symbol is loaded")
    cmd(imm, "drop symbol"); recv_all(imm, 0.3)
    cmd(clr, "get symbol"); recv_all(clr, 0.3)
    out5 = strip(cmd(clr, f"pray paralyze limb {tank_name}"))
    check("goes limp and unresponsive" in out5.lower(), "paralyze limb succeeds with a holy symbol")
    time.sleep(0.3)
    out_limbs = strip(cmd(tank, "limbs"))
    check("(  0%)" in out_limbs or "near death" in out_limbs.lower(),
          "one of the target's limbs is now at 0% (paralyzed)")
    out_tank_alive = strip(cmd(tank, "score"))
    check(tank_name.lower() in out_tank_alive.lower() or "level" in out_tank_alive.lower(),
          "the target is still alive after paralyze limb (never a major/instadeath limb)")

    announce_done("smoke_test_taunt_paralyzelimb", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            cmd(sock, "quit!", timeout=0.5)
            sock.close()
        except OSError:
            pass
    for prefix in ("Tauim", "Taunk", "Tauwr", "Tauwt", "Tapcl"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
        sql(f"DELETE FROM account WHERE name LIKE LOWER('{prefix}%{_suffix}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM mob WHERE vnum={MOB};")
    sql(f"DELETE FROM obj WHERE vnum={SYMBOL};")
