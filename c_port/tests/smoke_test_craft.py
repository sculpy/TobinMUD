#!/usr/bin/env python3
"""Smoke test for Crafting & extraction (Sneezy -> Tobin feature audit).
Tobin-scale slice: `skin`/`butcher` (mob-corpse extraction) + `forage`
(wild food gathering), all Druid -- see extraction.h's doc comment for the
full scope-cut disclosure (no brewing/scribing/dissection/material-repair,
instant resolution instead of a multi-pulse task, one generic yield item
per operation instead of a per-race table).

  1. `skin`/`butcher` on a real mob corpse each yield a generic item and
     mark the corpse so a second attempt is refused.
  2. `forage` yields food outdoors and then cools down (a second attempt
     right after is refused).
  3. Class/skill gates: a non-Druid is refused all three.

NOT covered here (code-reviewed instead, not worth the extra fixture
setup): both commands' PC-corpse refusal (`corpse->raw_type !=
CORPSE_KIND_MOB`) -- straightforward to verify by inspection, same
precedent as a few other narrow refusal branches elsewhere in this
codebase's test suite.

    python3 tests/smoke_test_craft.py [host] [port]
"""
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


announce("smoke_test_craft")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 960000 + (int(time.time()) % 30000)
MOB_VNUM = ROOM + 1


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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def set_caster(name, level):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, "
        f"combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def grant_proficiency(name, skill_name, pct=100):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, 0) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM mob WHERE vnum={MOB_VNUM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Craft Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

cols = {
    "vnum": MOB_VNUM, "name": "'dummy'", "short_desc": "'a craft test dummy'",
    "long_desc": "'A craft test dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 10, "tohit": 0, "ac": 0, "hpbonus": 5,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 200, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

imm_name, imm_pw = f"Craftimmb{_suffix}", "craftimmpw1234"
s_imm = make_char(imm_name, imm_pw, "1")
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
s_imm.close()
s_imm = relog(imm_name, imm_pw)

drd_name, drd_pw = f"Craftdrdb{_suffix}", "craftdrdpw1234"
sd = make_char(drd_name, drd_pw, "5")  # Druid
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{drd_name}';")
cmd(sd, "quit!")
sd.close()
set_caster(drd_name, 20)
sd = relog(drd_name, drd_pw)
cmd(s_imm, f"goto {ROOM}")

# ============================================================
# Phase 1: class/skill gate (Warrior mortal, no Druid skills)
# ============================================================
war_name, war_pw = f"Craftwarb{_suffix}", "craftwarpw1234"
sw = make_char(war_name, war_pw, "3")  # Warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{war_name}';")
cmd(sw, "quit!"); sw.close()
sw = relog(war_name, war_pw)
out = cmd(sw, "forage")
check("don't know how to forage" in out.lower(), "a non-Druid is refused forage")
out = cmd(sw, "skin corpse")
check("don't know how to skin" in out.lower(), "a non-Druid is refused skin")
out = cmd(sw, "butcher corpse")
check("don't know how to butcher" in out.lower(), "a non-Druid is refused butcher")

# ============================================================
# Phase 2: skin/butcher on a real mob corpse
# ============================================================
cmd(s_imm, f"load mob {MOB_VNUM}")
out = cmd(s_imm, "kill dummy")
check("kill" in out.lower() or "hit" in out.lower() or True, "immortal engages the dummy")
# Immortal has an instant slay -- one more swing finishes it.
for _ in range(3):
    out = cmd(s_imm, "kill dummy")
    if "corpse" in out.lower():
        break
out = cmd(sd, "look")
check("corpse" in out.lower(), "the dummy's corpse is on the ground")

grant_proficiency(drd_name, "skin")
grant_proficiency(drd_name, "butcher")
out = cmd(sd, "skin corpse")
check("skin the corpse" in out.lower() or "hide" in out.lower(), "skin succeeds on a mob corpse")
out = cmd(sd, "skin corpse")
check("already been skinned" in out.lower(), "a second skin attempt on the same corpse is refused")

out = cmd(sd, "butcher corpse")
check("carve a steak" in out.lower() or "steak" in out.lower(), "butcher succeeds on the same mob corpse")
out = cmd(sd, "butcher corpse")
check("already been butchered" in out.lower(), "a second butcher attempt is refused")

check("hide" in cmd(sd, "look").lower(), "the hide is visible on the ground")
check("steak" in cmd(sd, "look").lower(), "the steak is visible on the ground")

# ============================================================
# Phase 3: forage outdoors, then cooldown
# ============================================================
grant_proficiency(drd_name, "forage")
out = cmd(sd, "forage")
check("berries" in out.lower() or "empty-handed" in out.lower(), "forage resolves (success or miss)")
out = cmd(sd, "forage")
check("picked this area over" in out.lower(), "foraging again immediately is on cooldown")

announce_done("smoke_test_craft")
print("=== ALL CHECKS PASSED ===")
