#!/usr/bin/env python3
"""Smoke test for `stat` (user, 2026-07-12: "add stat command so an
immortal of level 55+ can see everything about the mob obj or room with a
vnum argument (Ex.: stat obj 101) from sneezy"). Covers:

  1. Below 55, `stat` is invisible ("Command not found").
  2. `stat obj <vnum>` dumps every column of the object row, plus any
     objaffect rows.
  3. `stat mob <vnum>` dumps every column of the mob row.
  4. `stat room <vnum>` dumps every column of the room row, plus its
     exits.
  5. A nonexistent vnum reports "No such <table> vnum <n>." instead of
     an empty/blank dump.
  6. `stat player <name>` (user 2026-07-12: "stat player <name> to stat
     a player") dumps the player/player_progress/player_attrs rows,
     decoded the same way (class/race/gender as words, alignment tier
     alongside the raw number), and reports plainly for a name that
     doesn't exist.

    python3 tests/smoke_test_stat.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_stat", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
OBJ_VNUM = ROOM + 1
MOB_VNUM = ROOM + 2


def cmd(sock, line, timeout=1.0, max_pages=15):
    """Drains a paginated reply too (`stat` now pages past ~20 lines,
    2026-07-17 general-pagination sweep) -- a no-op for anything short."""
    send_line(sock, line)
    out = recv_all(sock, timeout)
    pages = 0
    while "ENTER" in out and "more" in out and pages < max_pages:
        send_line(sock, "")
        out += recv_all(sock, timeout)
        pages += 1
    return out


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "statpw123"

# --- 1: below 55, stat is invisible ---
mort_name = f"Statmor{_suffix}"
s_mort = make_char(mort_name, pw)
out = cmd(s_mort, f"stat obj 1")
check("Command not found" in out, "stat is invisible below level 55")

imm_name = f"Statimm{_suffix}"
s_imm = make_char(imm_name, pw, "3")
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 55)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Stat Sandbox','A bare sandbox room, good for testing.\\n',NULL,8,3,0,0,0,0,0,0,0,0);")
    # room_flag=8 -> INDOORS bit; sector=3 -> ARCTIC ROAD

# --- 2: stat obj dumps every column, plus objaffect rows ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,action_flag,can_be_seen) "
    f"VALUES ({OBJ_VNUM},'trinket silver','a small silver trinket',"
    f"'A small silver trinket is lying here.',12,1,65,1);")  # type=OTHER, action_flag=GLOW|MAGIC
sql(f"INSERT INTO objaffect (vnum,type,mod1,mod2) VALUES ({OBJ_VNUM},15,7,0);")
out = cmd(s_imm, f"stat obj {OBJ_VNUM}")
check(f"Object {OBJ_VNUM}" in out, "stat obj shows the object header")
check("trinket silver" in out, "stat obj shows the object's name column")
check("short_desc" in out and "a small silver trinket" in out, "stat obj shows the short_desc column")
check("Affects" in out and "mod1" in out and "7" in out, "stat obj shows the objaffect row")
check("[ TAKE ]" in out and "[ BODY ]" not in out, "stat obj shows wear_flag decoded to readable flag names")
check("type" in out and "OTHER" in out, "stat obj shows type as a readable name (OTHER), not the raw number")
check("[ GLOW ]" in out and "[ MAGIC ]" in out, "stat obj shows action_flag decoded to readable flag names")

# --- 3: stat mob dumps every column, with readable class/race/actions,
#     no faction line, and only the 6 stats Tobin actually models ---
cols = {
    "vnum": MOB_VNUM, "name": "'stat dummy'", "short_desc": "'a stat test dummy'",
    "long_desc": "'A stat test dummy stands here.'", "description": "'It looks numeric.'",
    "actions": 6, "affects": 0, "faction": 3, "fact_perc": 50,  # actions=SENTINEL|SCAVENGER
    "letter": "'A'", "attacks": 1.0,
    "class": 2, "level": 5, "tohit": 0, "ac": 0, "hpbonus": 100,  # class=2 -> Cleric
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 30,  # race=30 -> GOBLIN
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
sql(f"INSERT INTO mob ({','.join(cols.keys())}) VALUES ({','.join(str(v) for v in cols.values())});")
out = cmd(s_imm, f"stat mob {MOB_VNUM}")
check(f"Mobile {MOB_VNUM}" in out, "stat mob shows the mobile header")
check("stat dummy" in out, "stat mob shows the mob's name column")
check("level" in out and "5" in out, "stat mob shows the level column")
check("class" in out and "Cleric" in out,
      "stat mob shows class as readable text (Cleric), not the raw number")
check("race" in out and "GOBLIN" in out, "stat mob shows race as a readable name (GOBLIN)")
check("actions" in out and "[ SENTINEL ]" in out and "[ SCAVENGER ]" in out,
      "stat mob shows actions as readable flag names")
check("faction" not in out and "fact_perc" not in out,
      "stat mob has no faction line at all -- factions aren't supported")
for unused_stat in (" bra ", " agi ", " foc ", " per ", " kar ", " spe "):
    check(unused_stat not in out, f"stat mob hides the unused '{unused_stat.strip()}' attribute column")
for used_stat in (" str ", " con ", " dex ", " intel ", " wis ", " cha "):
    check(used_stat in out, f"stat mob still shows the used '{used_stat.strip()}' attribute column")

# --- 4: stat room dumps every column, plus its exits ---
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES ({ROOM},0,'','',"
    f"0,0,0,0,0,{ROOM});")
out = cmd(s_imm, f"stat room {ROOM}")
check(f"Room {ROOM}" in out, "stat room shows the room header")
check("Stat Sandbox" in out, "stat room shows the room's name column")
check("Exits" in out and f"-> {ROOM}" in out, "stat room shows its own exit row")
check("dir=north" in out, "stat room shows exit direction as a word, not a number")
check("door=None" in out, "stat room shows exit door type as a word, not a number")
check("cond=none" in out, "stat room shows exit condition as a word, not a number")
check("sector" in out and "ARCTIC ROAD" in out, "stat room shows sector as a readable name, not a number")
check("room_flag" in out and "[ INDOORS ]" in out, "stat room shows room_flag decoded to readable flag names")

# --- 5: a nonexistent vnum is reported plainly ---
out = cmd(s_imm, "stat obj 999999999")
check("No such obj vnum 999999999" in out, "a nonexistent vnum is reported plainly, not a blank dump")

# --- 6: stat player dumps the player/progress/attrs tables ---
# imm_name already went through a real quit! (above, before promotion),
# so player_save() persisted it; the later set_level() SQL edit ran AFTER
# that close, the safe order (see the quit!-autosave regression notes),
# so player_progress genuinely holds level=55 on disk.
out = cmd(s_imm, f"stat player {imm_name}")
check(f"Player {imm_name}" in out, "stat player shows the player header")
check("class" in out and "Warrior" in out, "stat player shows class as a readable name (Warrior)")
check("race" in out and "Human" in out, "stat player shows race as a readable name (Human)")
check("gender" in out and "neuter" in out, "stat player shows gender as a readable name (neuter default)")
check("Progress" in out and "level" in out and "55" in out,
      "stat player shows the player_progress section with the persisted level")
check("alignment_tier" in out and "neutral" in out,
      "stat player shows a decoded alignment tier alongside the raw value")
check("Attributes" in out and "strength" in out,
      "stat player shows the player_attrs section")

out = cmd(s_imm, "stat player NoSuchPlayerAtAll")
check("No such player 'NoSuchPlayerAtAll'" in out,
      "a nonexistent player name is reported plainly, not a blank dump")

s_imm.close()
s_mort.close()
announce_done("smoke_test_stat", host, port)
print("=== ALL CHECKS PASSED ===")
