#!/usr/bin/env python3
"""Smoke test for weapon depth (sharpness, dual-wield proficiency) --
this session's self-assigned backlog item. Both effects are flat +1/0/-1
modifiers folded into combat_strike()'s randomized damage roll.
Sharpness has no class dependency, so it's verified statistically using
an immortal attacker (immortal `hit` still runs real combat_strike(),
just without the mortal 1.2s post-swing cooldown, so a decent sample
size stays fast). Dual wield's ACTUAL damage effect is NOT statistically
verified live here -- confirming a ~1-point mean shift on a per-swing
1.2s real cooldown would need an impractically large, slow sample for
reliable confidence -- so it's checked the fast, deterministic way
instead: `skills` shows a fresh Warrior already knows "dual wield"
(Combat tier, unconditional) while a Cleric's roster doesn't have the
skill at all. Covers:

  1. A fresh Warrior's `skills` lists "dual wield"; a fresh Cleric's
     does not (their roster has no such entry).
  2. An immortal wielding a dagger (sharp -- weapon_verb() "stab") lands
     noticeably higher average damage per hit than bare hands, over 30
     samples each, against a very-high-HP, very-easy-to-hit dummy.

    python3 tests/smoke_test_weapon_depth.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_weapon_depth")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
# Uses microsecond precision (not just int(time.time())) -- this test
# gets re-run repeatedly in quick succession while debugging combat
# timing, and the usual whole-second vnum collided with a leftover row
# from the previous attempt.
ROOM = 900000 + (int(time.time() * 1000) % 70000)
DAGGER = ROOM + 1
DUMMY_A = ROOM + 2
DUMMY_B = ROOM + 3

SAMPLES = 30
WEAR_TAKE = 1
WEAR_HOLD = 16384  # obj.c's WEAR_HOLD -- the bit wear_slot_for_flag() maps to WEAR_SLOT_HELD (wield/hold)


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp):
    # A mob fights back once engaged (combat_process_run() strikes both
    # ways every round, no passivity check) -- a training dummy landing
    # real, if small, damage every round can and does kill a low-HP
    # attacker over enough rounds. Raising level alone does NOT
    # recompute max_hp (that only happens at character creation), so
    # this has to be set directly.
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_dex(name, dex):
    # Even with a huge overall max_hp, a raw SQL bump like set_hp()
    # above does NOT recompute each LIMB's own max_hp (being_limbs_
    # full_heal() only runs at character creation) -- limbs stay at
    # their original tiny (level-1) cap, so a lucky run of hits on one
    # limb can decapitate/dismember the attacker outright regardless of
    # overall HP. Rather than fix that separately, just make the dummy
    # (nearly) unable to land a hit at all: a huge DEX gap clamps the
    # dummy's to-hit modifier to its -44 floor, ~6% hit chance.
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def make_dummy(vnum, keyword):
    # Explicit name=value pairs (not a raw positional list) so every
    # column's intent is unambiguous: level 1, tohit/ac 0 (never
    # affects the attacker's to-hit roll), a huge hpbonus so it
    # survives dozens of real hits, standing so it's a normal target.
    cols = {
        "vnum": vnum,
        "name": f"'{keyword}'",
        "short_desc": f"'a {keyword}'",
        "long_desc": f"'A {keyword} stands here, unmoving.'",
        "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 5000,
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


def damages_from(text):
    # The dummy hits back every round too ("A dummy hits your leg for 3
    # damage!"), and that line matches "for N damage" just as well as
    # the player's own outgoing hit line -- only lines starting with
    # "You " are the player's own damage dealt, which is what sharpness/
    # dual-wield actually affects.
    dmgs = []
    for line in text.splitlines():
        if line.startswith("You "):
            m = re.search(r"for (\d+) damage", line)
            if m:
                dmgs.append(int(m.group(1)))
    return dmgs


def average_damage(sock, target_name, n):
    # `hit` only INITIATES a fight -- the actual strikes resolve
    # asynchronously, once every COMBAT_ROUND_PULSES (1.2s), via the
    # server's own pulse scheduler (combat.c's combat_process_run()),
    # not synchronously inside the command that started it. So: start
    # the fight once, then just listen for the periodic damage lines
    # rather than re-issuing `hit` (which mostly just re-attacks/resets
    # the wait without producing an immediate strike of its own).
    cmd(sock, f"hit {target_name}")
    dmgs = []
    polls = 0
    while len(dmgs) < n and polls < n * 3:
        polls += 1
        out = recv_all(sock, 1.5)
        dmgs.extend(damages_from(out))
    check(len(dmgs) >= n, f"collected at least {n} real hits against {target_name} ({len(dmgs)} got)")
    return sum(dmgs) / len(dmgs)


pw = "weapondepthpw123"

# --- 1: skills roster confirms dual-wield knowledge by class (fast,
#        deterministic -- see this file's header comment for why the
#        live combat effect isn't statistically verified here too) ---
warrior_name = f"Wdepthwar{_suffix}"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "skills")
check("dual wield" in out.lower(), "a fresh Warrior's skills list already includes dual wield")
sw.close()

cleric_name = f"Wdepthcle{_suffix}"
sc = make_char(cleric_name, pw, "2")
out = cmd(sc, "skills")
check("dual wield" not in out.lower(), "a Cleric's skills list has no dual wield entry at all")
sc.close()

# --- 2: sharp dagger vs bare hands, immortal attacker (no combat-round
#        cooldown, so a decent sample stays fast) ---
imm_name = f"Wdepthimm{_suffix}"
imm_pw = "wdepthimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "2"); recv_all(s_imm)
set_level(imm_name, 51)
set_hp(imm_name, 999999)
set_dex(imm_name, 900)
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Weapon Depth Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Weapon Depth Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({DAGGER},'dagger sharp','a sharp dagger','A sharp dagger is lying here.',"
    f"5,{WEAR_TAKE | WEAR_HOLD},1,0,0);")

dummy_a, dummy_b = f"dummy{_suffix}a", f"dummy{_suffix}b"
make_dummy(DUMMY_A, dummy_a)
make_dummy(DUMMY_B, dummy_b)
check("You conjure" in cmd(s_imm, f"load mob {DUMMY_A}"), "dummy A is loaded")
check("You conjure" in cmd(s_imm, f"load mob {DUMMY_B}"), "dummy B is loaded")
check("You conjure" in cmd(s_imm, f"load obj {DAGGER}"), "the sharp dagger is loaded")

bare_avg = average_damage(s_imm, dummy_a, SAMPLES)

out = cmd(s_imm, "get dagger")
check("you get" in out.lower(), "the immortal picks up the dagger")
out = cmd(s_imm, "wield dagger")
check("wield" in out.lower(), "wield equips the dagger")

dagger_avg = average_damage(s_imm, dummy_b, SAMPLES)

check(dagger_avg > bare_avg + 0.4,
      f"a sharp dagger's average damage ({dagger_avg:.2f}) beats bare hands ({bare_avg:.2f}) by a clear margin")

s_imm.close()
announce_done("smoke_test_weapon_depth")
print("=== ALL CHECKS PASSED ===")
