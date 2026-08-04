#!/usr/bin/env python3
"""Smoke test for Body types (Sneezy -> Tobin feature audit, user
2026-07-26: "creatures have different limb sets", full 60-type parity).

  1. A real seeded mob classified as BODY_SPIDER (vnum 948, "a giant
     spider", tobin_migrations.sql's name-matching pass) actually gets
     combat hits on its mob-only EX_* limb slots (extra leg/foot) --
     these are always inactive (max_hp=0) on every ordinary humanoid,
     confirming being_limbs_full_heal()/combat.c's pick_weighted_limb()
     genuinely vary by body_type, not just a fixed humanoid table.
  2. An ordinary default (BODY_HUMANOID) mob never shows an EX_* hit
     over the same number of rounds -- the mechanism doesn't leak extra
     limbs onto creatures that shouldn't have them.

    python3 tests/smoke_test_body_types.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_body_types", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 972000 + (int(time.time()) % 20000)
SPIDER_VNUM = 948   # "a giant spider" -- real seeded mob, classified BODY_SPIDER
HUMANOID_VNUM = 1   # Grimhaven/Tobin City's own long-standing sandbox test mob


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Body Types Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name = f"Bodyimm{_suffix}"
imm_pw = "bodyimmpw1234"
make_char(imm_name, imm_pw)
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")

EX_PATTERN = re.compile(r"extra (left|right) (leg|foot)")
# Arthropods have no feet (user 2026-07-26: "spiders dont have feet") --
# body_limb_name_override() (body.c) relabels a spider's foot/EX_foot
# slots "leg"/"extra leg" instead. "foot" should never appear at all for
# a BODY_SPIDER mob.
FOOT_WORD = re.compile(r"\bfoot\b")


def fight_and_scan_for_extra_limb(vnum, rounds):
    cmd(s, f"load mob {vnum}")
    out = cmd(s, "hit " + ("spider" if vnum == SPIDER_VNUM else "puff"))
    found = out
    for _ in range(rounds):
        time.sleep(1.5)
        found += cmd(s, "", 0.5)
        if not cmd(s, "score", 0.3):
            break
    return found


spider_log = fight_and_scan_for_extra_limb(SPIDER_VNUM, 25)
check(EX_PATTERN.search(spider_log) is not None,
      "a BODY_SPIDER mob (vnum 948) takes real hits on its extra leg/foot slots")
check(FOOT_WORD.search(spider_log) is None,
      "the spider's foot-like slots are relabeled 'leg', not 'foot' (spiders don't have feet)")

# Clean up the spider (or its corpse) and the room before the humanoid phase.
cmd(s, "purge")

humanoid_log = fight_and_scan_for_extra_limb(HUMANOID_VNUM, 25)
check(EX_PATTERN.search(humanoid_log) is None,
      "an ordinary BODY_HUMANOID mob never takes a hit on an extra leg/foot slot")

announce_done("smoke_test_body_types", host, port)
print("=== ALL CHECKS PASSED ===")
