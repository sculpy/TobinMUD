#!/usr/bin/env python3
"""Smoke test for the fourth reference-parity gap closed against
SneezyMUD's misc/immortal.cc (doGoto()/doTrans()), found by a prior
read-only audit; see smoke_test_group_reference_fixes.py for the other
three (being_in_group() mount/rider, cmd_group.c eligibility guards,
group <name> toggle).

  4a. `goto` drags along any IMMORTAL follower who was standing in the
      same room, recursively re-running THEIR OWN goto -- so a mortal
      follower (who'd fail their own goto's immortal check) is left
      behind instead of being unconditionally teleported too.
  4b. `transfer` drags the transferred target's own mount and rider
      along with them (but does NOT drag followers -- that's goto's
      job only).

    python3 tests/smoke_test_goto_transfer_reference_fixes.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_goto_transfer_reference_fixes", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_START = 900000 + (int(time.time()) % 60000)
ROOM_DEST = ROOM_START + 1
ROOM_TIMM = ROOM_START + 2
ROOM_TTGT = ROOM_START + 3
HORSE_VNUM = ROOM_START + 4


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    send_line(s, "quit!"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def mkroom(vnum, name):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")


# =================== 4a: goto drags immortal followers, not mortals ===================
ldr_name, ldr_pw = f"Gtfxldr{_suffix}", "gtfxldrpw123"
ifol_name, ifol_pw = f"Gtfxifo{_suffix}", "gtfxifopw123"
mfol_name, mfol_pw = f"Gtfxmfo{_suffix}", "gtfxmfopw123"
pw = "gtfxpw123456"

make_char(ldr_name, ldr_pw, "1")
make_char(ifol_name, ifol_pw, "1")
make_char(mfol_name, mfol_pw, "1")

mkroom(ROOM_START, "Goto Fixes Start")
mkroom(ROOM_DEST, "Goto Fixes Destination")

set_level(ldr_name, 51)
set_level(ifol_name, 51)  # immortal follower -- should get dragged along
# mfol_name stays mortal (level 1, the default) -- should NOT be dragged

sql(f"UPDATE player SET load_room={ROOM_START} WHERE name IN "
    f"('{ldr_name}','{ifol_name}','{mfol_name}');")

ldr = login(ldr_name, ldr_pw)
ifol = login(ifol_name, ifol_pw)
mfol = login(mfol_name, mfol_pw)
recv_all(ldr, 0.3); recv_all(ifol, 0.3); recv_all(mfol, 0.3)

check("Goto Fixes Start" in cmd(ldr, "look"), "leader starts in the sandbox start room")

out = strip(cmd(ifol, f"follow {ldr_name}"))
check("now follow" in out.lower(), "immortal follower attaches via follow")
out = strip(cmd(mfol, f"follow {ldr_name}"))
check("now follow" in out.lower(), "mortal follower attaches via follow")

recv_all(ifol, 0.2); recv_all(mfol, 0.2)
out = strip(cmd(ldr, f"goto {ROOM_DEST}"))
check("goto fixes destination" in out.lower(), "leader's own goto lands in the destination room")

out_ifol = strip(recv_all(ifol, timeout=1.5))
check("you follow" in out_ifol.lower(), "the immortal follower is told they're following")
check("goto fixes destination" in out_ifol.lower(),
      "the immortal follower's OWN goto ran and landed them in the same destination room")

out_ifol_look = strip(cmd(ifol, "look"))
check("goto fixes destination" in out_ifol_look.lower(),
      "the immortal follower is genuinely standing in the destination room now")

out_mfol_look = strip(cmd(mfol, "look"))
check("goto fixes start" in out_mfol_look.lower(),
      "the MORTAL follower was left behind in the start room -- goto only drags IMMORTAL "
      "followers, and each one is still subject to their own permission checks "
      "(a mortal's own goto would itself be refused)")

ldr.close(); ifol.close(); mfol.close()

# =================== 4b: transfer drags the target's mount + rider ===================
timm_name, timm_pw = f"Gtfxtim{_suffix}", "gtfxtimpw123"
ttgt_name, ttgt_pw = f"Gtfxtgt{_suffix}", "gtfxtgtpw123"

make_char(timm_name, timm_pw, "1")
make_char(ttgt_name, ttgt_pw, "1")

mkroom(ROOM_TIMM, "Transfer Fixes Immortal Room")
mkroom(ROOM_TTGT, "Transfer Fixes Target Room")

set_level(timm_name, 51)
# ttgt is also made immortal -- irrelevant to what's under test (transfer
# dragging a target's mount/rider along), but it makes `ride` succeed
# unconditionally (being_is_immortal() skips the skill roll in
# cmd_ride.c) instead of being a random skill-learn chance for a
# non-Deikhan class.
set_level(ttgt_name, 51)
sql(f"UPDATE player SET load_room={ROOM_TTGT} WHERE name='{ttgt_name}';")

timm = login(timm_name, timm_pw)
ttgt = login(ttgt_name, ttgt_pw)
recv_all(timm, 0.3); recv_all(ttgt, 0.3)

check("Transfer Fixes Immortal Room" in cmd(timm, f"goto {ROOM_TIMM}"),
      "the transferring immortal lands in their own sandbox room")
check("Transfer Fixes Target Room" in cmd(ttgt, "look"), "the target lands in the target room")

cols = {
    "vnum": HORSE_VNUM, "name": "'horse steed'", "short_desc": "'a lone steed'",
    "long_desc": "'A lone steed stands here, saddled and ready.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 47,  # HORSE
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

cmd(timm, f"goto {ROOM_TTGT}")
cmd(timm, f"load mob {HORSE_VNUM}")
recv_all(timm, 0.3)
out = cmd(ttgt, "ride steed")
check("you mount" in out.lower(), "the target mounts the horse before being transferred")
cmd(timm, f"goto {ROOM_TIMM}")
recv_all(timm, 0.3)

out = cmd(timm, f"transfer {ttgt_name}")
check(f"you transfer {ttgt_name}".lower() in out.lower(), "the immortal is told the transfer happened")

out_ttgt = strip(recv_all(ttgt, timeout=1.5))
check("yanked through space" in out_ttgt.lower(), "the target is told something happened to them")
check("transfer fixes immortal room" in out_ttgt.lower(),
      "the target's own look shows the immortal's room")

out_timm_look = strip(cmd(timm, "look"))
check(ttgt_name.lower() in out_timm_look.lower(), "the target is genuinely standing in the immortal's room")
check("steed" in out_timm_look.lower(),
      "the target's MOUNT came along too -- transfer drags mount/rider (matching doTrans()), "
      "even though it does NOT drag followers the way goto does")

timm.close(); ttgt.close()
announce_done("smoke_test_goto_transfer_reference_fixes", host, port)
print("=== ALL CHECKS PASSED ===")
