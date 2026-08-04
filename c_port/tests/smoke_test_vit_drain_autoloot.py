#!/usr/bin/env python3
"""Smoke test for two related user requests (2026-08-03):

  1. "vitality should decrease when fighting to about .75 of a point
     per round" -- combat_process_run() (combat.c) now drains a PC
     fighter's Vitality by ~0.75/round while actively fighting (via a
     float accumulator, being_t.vit_fatigue_accum, that spends off
     whole vit points as they cross 1.0).
  2. "when looting a mob the game should report any inventory changes"
     -- autoloot (combat.c, PLR_AUTOLOOT) used to print one generic
     "You automatically loot X's corpse." line with no breakdown; now
     reports gold (and any items) actually gained, one line each.

Covers:
  1. Vitality (per `score`) measurably drops over several real combat
     rounds against a durable sandbox mob.
  2. Killing a mob with autoloot on prints a "You loot N gold from
     ...'s corpse." line naming a real, positive gold amount -- not
     just the old generic sentence.

    python3 tests/smoke_test_vit_drain_autoloot.py [host] [port]
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
ROOM = 900000 + (int(time.time() * 1000) % 70000)
MOB_VNUM = ROOM + 1
MOB2_VNUM = ROOM + 2


announce("smoke_test_vit_drain_autoloot", host, port)


def get_vit(score_out):
    m = re.search(r"Move:\s*(\d+)\s*\((\d+)\s*Max", score_out)
    return int(m.group(1)) if m else None


# Territory (2026-08-03) is a forced step right after race: race, then
# homeland (1-3), then class.
def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


name, pw = f"Vitloot{_suffix}", "vitlootpw12345"
s = make_char(name, pw, 3)  # Human Warrior
cmd(s, "toggle pk")
cmd(s, "toggle autoloot")

imm_name, imm_pw = f"Vitlootim{_suffix}", "vitlootimpw12"
imm = make_char(imm_name, imm_pw, 1)
cmd(imm, "quit!")
imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (imm_name, imm_pw, "1"):
    send_line(imm, step)
    recv_all(imm)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'VitLoot Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(imm, f"goto {ROOM}")
cmd(imm, f"transfer {name}")
cmd(s, "look")

# --- 1: vitality drains ~0.75/round while fighting a durable dummy ---
dummy_cols = {
    "vnum": MOB_VNUM, "name": "'vitdummy'", "short_desc": "'a vitality-drain dummy'",
    "long_desc": "'A vitality-drain dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": -50, "ac": 0, "hpbonus": 5000,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(dummy_cols.keys())
col_values = ",".join(str(v) for v in dummy_cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")
cmd(imm, f"load mob {MOB_VNUM}")

out = cmd(s, "score")
vit_before = get_vit(out)
check(vit_before is not None, "score's Vitality field parsed cleanly")

out = cmd(s, "hit vitdummy")
check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts against the durable dummy")

vit_after = vit_before
for _ in range(8):
    cmd(s, "look")
    score_out = cmd(s, "score")
    v = get_vit(score_out)
    if v is not None:
        vit_after = v

check(vit_after < vit_before,
      f"Vitality dropped while fighting ({vit_before} -> {vit_after})")
# Combat resolves on its own ~1.2s pulse, independent of this loop's own
# command round-trip time, so the exact round count elapsed during the
# 8 iterations above isn't directly observable here -- just confirm the
# drop is a real, gradual drain (not an instant wipe to 0/near-0, which
# would indicate a much larger per-round or per-hit rate than intended).
check(0 < (vit_before - vit_after) < vit_before,
      f"Vitality drop is gradual, not an instant wipe ({vit_before} -> {vit_after})")

cmd(s, "flee")
recv_all(s, 1.0)

# --- 2: autoloot reports a real gold amount, not a generic sentence ---
mob_cols = dict(dummy_cols)
mob_cols["vnum"] = MOB2_VNUM
mob_cols["name"] = "'lootmob'"
mob_cols["short_desc"] = "'a loot-report mob'"
mob_cols["long_desc"] = "'A loot-report mob stands here.'"
mob_cols["hpbonus"] = 0
mob_cols["tohit"] = -50
col_names = ",".join(mob_cols.keys())
col_values = ",".join(str(v) for v in mob_cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")
cmd(imm, f"load mob {MOB2_VNUM}")

out = cmd(s, "kill lootmob")
looted_gold = None
for _ in range(25):
    m = re.search(r"You loot (\d+) gold from .+'s corpse\.", out)
    if m:
        looted_gold = int(m.group(1))
        break
    if "have slain" in out.lower() or "have defeated" in out.lower():
        break
    out = cmd(s, "look")

check(looted_gold is not None and looted_gold > 0,
      "autoloot printed a specific 'You loot N gold from ...' line with a real amount")
check("you automatically loot" not in out.lower(),
      "the old generic 'You automatically loot ...' sentence is gone")

s.close()
imm.close()

announce_done("smoke_test_vit_drain_autoloot", host, port)
print("=== ALL CHECKS PASSED ===")
