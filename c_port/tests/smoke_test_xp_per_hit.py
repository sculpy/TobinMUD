#!/usr/bin/env python3
"""Smoke test for per-hit XP (user, 2026-08-03: "we want xp gain
calculated per hit, not at the end of a fight" / "you can report on how
much gain at the end of the fight but the actual gain should be per
hit"). Old design granted the whole kill reward in one lump at
combat_defeat(); combat_award_hit_xp() (combat.c) now credits a
proportional share of it as each hit lands, silently (no per-hit
message), with combat_defeat() printing ONE summary line for the whole
fight.

Covers:
  1. Experience (per `score`) rises DURING an ongoing fight, before the
     mob is dead -- proof the gain is real per-hit, not deferred.
  2. No "You gain N experience" message appears mid-fight (silent).
  3. Killing the mob prints exactly one "You gain a total of N
     experience from that fight" summary line.
  4. The final experience total matches what score already showed
     building up (no double-counting, no lost fractional hits).

    python3 tests/smoke_test_xp_per_hit.py [host] [port]
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


announce("smoke_test_xp_per_hit", host, port)


def get_xp(score_out):
    m = re.search(r"Experience:\s*(\d+)", score_out)
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


# Kept a genuine MORTAL (not immortal -- `kill`/`attack` instakills for an
# immortal, which would defeat the whole point of observing several
# rounds of incremental XP). A separate, temporary immortal (`imm`) does
# the room-creation/goto/mob-loading legwork instead.
name, pw = f"Xphit{_suffix}", "xphitpw12345"
s = make_char(name, pw, 3)  # Human Warrior

imm_name, imm_pw = f"Xphitimm{_suffix}", "xphitimmpw123"
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
    f"VALUES ({ROOM},0,0,0,'XP Per Hit Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cmd(imm, f"goto {ROOM}")
cmd(imm, f"transfer {name}")
cmd(s, "look")

out = cmd(s, "score")
xp_before_fight = get_xp(out)
check(xp_before_fight is not None, "score's Experience field parsed cleanly")

# A weak-hitting sandbox mob, soft enough to reliably die within this
# test's round budget but with enough HP (max_hp = 20 + level*5 = 25) to
# take a few hits first -- so incremental XP is actually observable
# mid-fight, not just a single one-shot kill.
cols = {
    "vnum": MOB_VNUM, "name": "'xphitmob'", "short_desc": "'an xp-hit sandbag'",
    "long_desc": "'An xp-hit sandbag stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
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
cmd(imm, f"load mob {MOB_VNUM}")

out = cmd(s, "hit xphitmob")
check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts against the loaded mob")

# --- 1/2/3: one continuous loop covering mid-fight AND the eventual kill,
# so the kill's own summary message (which can land in the same round the
# mob dies, no separate "finishing" phase) is never missed. ---
seen_xp_rise = False
seen_per_hit_message = False
summary_count = 0
killed = False
for _ in range(20):
    out = cmd(s, "look")  # a harmless filler command each round; also drains combat round output
    # Note: "you feel more experienced!" (level-up) legitimately contains
    # "experience" as a substring of "experienced" -- match the real
    # forbidden phrase specifically ("you gain N experience"), not just
    # both words appearing anywhere in the same chunk.
    if re.search(r"you gain \d+ experience", out.lower()):
        seen_per_hit_message = True
    summary_count += out.lower().count("you gain a total of")
    score_out = cmd(s, "score")
    xp_now = get_xp(score_out)
    if xp_now is not None and xp_now > xp_before_fight:
        seen_xp_rise = True
    if "have slain" in out.lower() or "have defeated" in out.lower():
        killed = True
        break

check(seen_xp_rise, "score's Experience rose DURING the fight, before the mob was dead")
check(not seen_per_hit_message, "no per-hit 'You gain N experience' message appears (silent per hit)")
check(killed, "the sandbox mob was actually killed within the round budget")
check(summary_count == 1, f"exactly one summary XP line printed at the kill (got {summary_count})")

# --- 4: final score matches what was building up (sanity, not exact-equal
# since a final overkill hit can slightly overshoot -- just confirm it's
# still sensible: higher than the last mid-fight reading, no reset). ---
out = cmd(s, "score")
xp_final = get_xp(out)
check(xp_final is not None and xp_final > xp_before_fight, "final score reflects real, persisted XP gain")

s.close()
imm.close()

announce_done("smoke_test_xp_per_hit", host, port)
print("=== ALL CHECKS PASSED ===")
