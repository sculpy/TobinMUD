#!/usr/bin/env python3
"""Smoke test for `continue` + targeted/breaking holy symbols (user
2026-07-12: "add a continue command so clerics that heal <target> can
continue automatically until the target is fully healed or thier holy
symbol breaks (holy symbols should use the same logic as components for
mages and druids)"). Covers:

  1. `pray heal light <target>` heals someone else in the room, not the
     caster, and the caster's own self-heal ("pray heal light" with no
     target) still works unchanged.
  2. `continue` with no prior heal is refused.
  3. `continue` repeats the last heal-type prayer on the same target,
     each round consuming one more holy symbol, until either the target
     is fully healed or the caster's holy symbols run out.
  4. Once continue reports "fully healed" (or symbols run out), a
     second `continue` call is refused again (state cleared).

    python3 tests/smoke_test_continue.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_continue", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
GM_CLERIC = ROOM + 1
SYMBOL_BASE = ROOM + 100


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
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
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def set_skill_pct(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"SELECT id, '{skill_name}', {pct}, 0 FROM player WHERE name='{name}' "
        f"ON DUPLICATE KEY UPDATE pct={pct}, last_gain_at=0;")


def make_guildmaster(vnum, keyword, class_mask, level=51):
    # `level` identifies the guildmaster's TIER to find_guildmaster()
    # (cmd_practice.c: 51=Basic, 80=Combat, 100=Advanced) -- this helper
    # used to hardcode `1`, well below GUILD_LEVEL_BASIC (51), so
    # `practice basic <n>` could never actually find this mob at all
    # ("You don't see a Basic guildmaster of your discipline here.",
    # even standing right next to it). Fixed to default to Basic (51),
    # matching smoke_test_practice.py's own make_guildmaster() precedent.
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'guildmaster {keyword}','a guildmaster of {keyword}',"
        f"'A guildmaster of {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,{class_mask},{level},0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


# --- Immortal setup: sandbox room, guildmaster, symbols, damaged target mob ---
imm_name = f"Contimm{_suffix}"
imm_pw = "contimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # territory: urban
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
s_imm.close()
set_level(imm_name, 51)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Continue Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Continue Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

make_guildmaster(GM_CLERIC, f"clerics{_suffix}", 2)
check("You conjure" in cmd(s_imm, f"load mob {GM_CLERIC}"), "the Cleric guildmaster is loaded")

for i in range(6):
    vnum = SYMBOL_BASE + i
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'symbol holy silver','a tarnished silver holy symbol',"
        f"'A tarnished silver holy symbol is lying here.',12,1,1);")

# --- Two Cleric characters: healer and a wounded patient ---
pw = "continuepw123"
healer_name = f"Conheal{_suffix}"
sh = make_char(healer_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{healer_name}';")
cmd(sh, "quit!")
sh.close()
set_level(healer_name, 40)  # mortal, above "heal light"'s min_level (1)
# Practice points are only ever awarded at the moment of an actual
# level-up (practice_points_for_level(), practice.c) -- set_level()'s
# raw SQL jump to 40 never ran that award loop retroactively, so the
# healer would otherwise have only the fresh-character stipend (7,
# being_create_pc()) to spend below. Grant more directly via SQL
# instead (same "raw SQL, must happen before the reconnect below"
# shape set_level()/the patient's hp bump already use) -- the in-game
# `set <name> practices <n>` command needs SET_MIN_LEVEL (58), above
# this test's own level-51 immortal.
sql(f"UPDATE player_progress SET practice_points=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{healer_name}');")

patient_name = f"Conpat{_suffix}"
sp = make_char(patient_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{patient_name}';")
# Wound the patient (a fraction of max_hp) BEFORE the final reconnect --
# same rule as level: a raw SQL change to an already-connected
# descriptor's row never reaches its live being_t, only a fresh login
# picks it up. Wounded (not full) so `continue`'s repeated-heal loop
# actually has to run more than zero rounds to reach "fully healed".
result = subprocess.run(
    ["mariadb", "tobin", "-N", "-e",
     f"SELECT max_hp FROM player_progress WHERE player_id="
     f"(SELECT id FROM player WHERE name='{patient_name}');"],
    check=True, capture_output=True, text=True)
patient_max_hp = int(result.stdout.strip())
cmd(sp, "quit!")
sp.close()
sql(f"UPDATE player_progress SET hp={max(1, patient_max_hp // 5)} WHERE player_id="
    f"(SELECT id FROM player WHERE name='{patient_name}');")

sh = socket.create_connection((host, port), timeout=5)
recv_all(sh)
send_line(sh, healer_name); recv_all(sh)
send_line(sh, pw); recv_all(sh)
send_line(sh, "1"); recv_all(sh)
cmd(sh, "color off")

sp = socket.create_connection((host, port), timeout=5)
recv_all(sp)
send_line(sp, patient_name); recv_all(sp)
send_line(sp, pw); recv_all(sp)
send_line(sp, "1"); recv_all(sp)
cmd(sp, "color off")

# `pray <spell> <target>`'s target resolution goes through
# combat_find_room_target() same as any attack command (cmd_pray.c) --
# gated on mutual PK consent between two non-immortal mortals, same as
# every other test that targets a different player. Missing here before
# (this test never targeted another PLAYER with a spell until section 3
# below was added), which is why "pray heal light <target>" couldn't
# find the patient at all ("You don't see them here.").
cmd(sh, "toggle pk")
cmd(sp, "toggle pk")

# Give the healer Basic discipline (practice at the Cleric guildmaster) --
# practice_points were already granted via SQL above, before the reconnect.
# Bare `practice basic` (no count) only shows the listing -- it never
# actually spends a point, same as smoke_test_practice.py's own
# `practice basic 1` precedent. This loop was silently a no-op before
# (10 iterations of the listing, never any real spend).
for _ in range(10):
    cmd(sh, "practice basic 1")

# `pray`/`cast` gate their own dispatch behind a SEPARATE per-skill
# learn-by-doing proficiency roll (skill_roll_success(skill_learn_from_
# doing(...)), cmd_pray.c/cmd_cast.c) on top of the discipline-percentage
# ACCESS gate just spent above -- a fresh attempt at "heal light" starts
# at 1% (skill.c's own floor), a ~1% chance to succeed, which is exactly
# why the self-pray/target-pray/continue checks below were failing
# ("You fumble the prayer... nothing happens"). Seed real proficiency
# once here (matching smoke_test_practice.py's own set_skill_pct()
# precedent) -- 100% covers every heal light cast for the rest of this
# test, `continue`'s repeats included, since a skill already at its
# ceiling always re-checks true regardless of the 30s gain cooldown.
set_skill_pct(healer_name, "heal light", 100)

# --- 1: continue with nothing to continue is refused ---
out = cmd(sh, "continue")
check("aren't healing anyone" in out, "continue with no prior heal is refused")

# --- 2: pray heal light with no target heals self; unaffected by this change ---
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE}"), "a holy symbol is loaded for the healer")
cmd(s_imm, "drop symbol")
out = cmd(sh, "get symbol")
check("you get" in out.lower(), "the healer picks up a holy symbol")

out = cmd(sh, "pray heal light")
check("You pray for heal light and feel restored" in out, "self-pray (no target) still works unchanged")

# --- 3: pray heal light <target> heals someone else ---
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE + 1}"), "a second holy symbol is loaded for the healer")
cmd(s_imm, "drop symbol")
out = cmd(sh, "get symbol")
check("you get" in out.lower(), "the healer picks up a second holy symbol")

out = cmd(sh, f"pray heal light {patient_name.lower()}")
check("is restored" in out, "pray heal light <target> reports the target being restored")
recv_all(sp, 0.3)  # drain the patient's private heal notification (not independently asserted here)

# --- 4: continue repeats the targeted heal until symbols run out ---
for i in range(2, 6):
    check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE + i}"), f"holy symbol #{i+1} is loaded for the healer")
    cmd(s_imm, "drop symbol")
    cmd(sh, "get symbol")

out = cmd(sh, "continue")
check("holy symbol breaks" in out or "fully healed" in out,
      "continue repeats the heal until symbols run out or the target is fully healed")

# --- 5: continue again afterward is refused (state cleared either way) ---
out = cmd(sh, "continue")
check("aren't healing anyone" in out, "continue after finishing is refused again")

s_imm.close()
sh.close()
sp.close()
announce_done("smoke_test_continue", host, port)
print("=== ALL CHECKS PASSED ===")
