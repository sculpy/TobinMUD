#!/usr/bin/env python3
"""Smoke test for `cudgel` (Thief, missing-skill audit backlog, skill.c
level 41, SKILL_TIER_ADVANCED). See cmd_cudgel.c's own header comment for
the real-upstream research and scope-down rationale (no damage either
way -- a pure stun skill -- one skill_roll_success() roll, success sets
POSITION_STUNNED, an existing enum value nothing had ever actually
transitioned a being into before this).

    python3 tests/smoke_test_cudgel.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_cudgel", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
CLASS_THIEF = 3  # being.h: CLASS_MAGE=0, CLERIC=1, WARRIOR=2, THIEF=3 (0-indexed;
                 # the 1-based creation-menu choice "4" below maps to this)
DAGGER_VNUM = 305  # real seeded content: "dagger small normal"


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "4", "done", "done"):
        send_line(s, step)
        recv_all(s)
    # A raw close() leaves them linkdead, and a linkdead reconnect resumes
    # the OLD in-memory character -- ignoring the level/discipline/room
    # UPDATEs done below entirely (same trap smoke_test_peek.py's own
    # comment already documents). Clean logout instead.
    cmd(s, "quit!")
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, room):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    make_char(name, pw)
    sql(f"UPDATE player SET class={CLASS_THIEF}, load_room={room} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level=45, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=5000, max_hp=5000, vit=5000, max_vit=5000 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 976500 + (int(time.time()) % 1000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Cudgel Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Cgimm{_suffix}", "cgimmpw12345"
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
for step in (imm_name, "y", imm_pw, imm_pw, "new", imm_name, "1", "1", "1", "done", "done"):
    send_line(si, step)
    recv_all(si)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

sockets = []
try:
    # --- 1: no wielded weapon is refused ---
    nameA, sA = make_single("Cgtw", room=ROOM)
    sockets.append(sA)
    seed_proficiency(nameA, "cudgel", 100)
    recv_all(sA)
    cmd(si, f"goto {ROOM}")
    outNoWpn = strip(cmd(sA, "cudgel someone"))
    check("wield a weapon" in outNoWpn.lower(), "cudgel with no wielded weapon is refused")

    check("You conjure" in cmd(si, f"load obj {DAGGER_VNUM}"), "imm loads a dagger")
    cmd(si, "drop dagger")
    check("you get" in cmd(sA, "get dagger").lower(), "attacker picks up the dagger")
    cmd(sA, "wield dagger")

    # --- 2: no target is refused ---
    out1 = strip(cmd(sA, "cudgel"))
    check("cudgel whom" in out1.lower(), "cudgel with no target is refused")

    # --- 3: nonexistent target reports cleanly ---
    out2 = strip(cmd(sA, "cudgel NoSuchPersonAtAll"))
    check("aren't here" in out2.lower(), "cudgeling a nonexistent target reports cleanly")

    # --- 4: immortal target is refused ---
    out3 = strip(cmd(sA, f"cudgel {imm_name}"))
    check("no effect on your immortal target" in out3.lower(), "cudgel refuses an immortal target")

    # --- 5: 100%-proficiency cudgel succeeds, knocks the target out ---
    nameB, sB = make_single("Cgtwo", room=ROOM)
    sockets.append(sB)
    recv_all(sB)
    time.sleep(1.3)  # clear the attacker's own wait from checks above
    recv_all(sA, 0.3)
    out4 = strip(cmd(sA, f"cudgel {nameB}"))
    check("knocking them unconscious" in out4.lower(), "100%-proficiency cudgel succeeds")
    out4v = strip(recv_all(sB, 0.3))
    check("knocking you unconscious" in out4v.lower(), "the target is told they were knocked out")

    # --- 6: 0%-proficiency cudgel always misses, deals no damage ---
    nameC, sC = make_single("Cgtz", room=ROOM)
    nameD, sD = make_single("Cgtzo", room=ROOM)
    sockets += [sC, sD]
    seed_proficiency(nameC, "cudgel", 0)
    recv_all(sC); recv_all(sD)
    cmd(si, f"goto {ROOM}")
    check("You conjure" in cmd(si, f"load obj {DAGGER_VNUM}"), "imm loads a second dagger")
    cmd(si, "drop dagger")
    check("you get" in cmd(sC, "get dagger").lower(), "second attacker picks up the dagger")
    cmd(sC, "wield dagger")
    out5 = strip(cmd(sC, f"cudgel {nameD}"))
    check("miss your attempt" in out5.lower(), "0%-proficiency cudgel always misses")

    # --- help ---
    check("non-lethal" in cmd(sA, "help cudgel").lower(), "help cudgel describes the command")

    announce_done("smoke_test_cudgel", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    si.close()
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Cgimm", "Cgtw", "Cgtz"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
