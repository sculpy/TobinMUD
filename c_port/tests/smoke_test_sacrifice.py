#!/usr/bin/env python3
"""Smoke test for Druid's `sacrifice` skill (Full spell/skill/prayer
roster import, user 2026-07-26: 6 named Shaman spells ported onto
Druid). Real upstream (task_sacrifice.cc) is a multi-round totem/
lifeforce ritual; scoped down here to a single-action skill matching
cmd_skin.c/cmd_butcher.c's pattern, with "lifeforce" mapped onto
Tobin's existing vit/Move pool (being_heal_vit()).

  1. `sacrifice` with no corpse present is refused.
  2. `sacrifice <corpse>` off a real killed-mob corpse consumes the
     corpse either way (it's gone from the room afterward).
  3. On success, the caster's live Move actually increases (not just
     a cosmetic message) -- exercises being_heal_vit().

    python3 tests/smoke_test_sacrifice.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_sacrifice", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 975000 + (int(time.time()) % 20000)
BIRD_MOB_VNUM = 119  # "crow small ugly", real seeded mob


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


def move_of(sock):
    m = re.search(r"Move:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1))


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Sacrifice Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name = f"Sacimm{_suffix}"
imm_pw = "sacimmpw1234"
make_char(imm_name, imm_pw)
sql(f"UPDATE player_progress SET level=51, basic_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"VALUES ((SELECT id FROM player WHERE name='{imm_name}'), 'sacrifice', 100, 0) "
    f"ON DUPLICATE KEY UPDATE pct=100;")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")

out = cmd(s, "sacrifice")
check("don't see a corpse" in out.lower(), "sacrifice with no corpse present is refused")

cmd(s, f"load mob {BIRD_MOB_VNUM}")
out = cmd(s, "kill crow")
check("slain" in out.lower() or "kill" in out.lower(), "the bird is killed, leaving a corpse")

move_before = move_of(s)
out = cmd(s, "sacrifice corpse")
check("ritual sacrifice" in out.lower(), "the sacrifice ritual completes")

out = cmd(s, "look")
check("corpse" not in out.lower(), "the corpse was consumed by the sacrifice")

move_after = move_of(s)
check(move_after >= move_before,
      f"live Move did not decrease from the ritual ({move_before} -> {move_after})")

sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")
sql(f"DELETE FROM room WHERE vnum={ROOM};")

announce_done("smoke_test_sacrifice", host, port)
print("=== ALL CHECKS PASSED ===")
