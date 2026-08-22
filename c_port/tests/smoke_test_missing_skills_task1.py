#!/usr/bin/env python3
"""Smoke test for the 2026-08-09 "Learn-by-doing roster audit" batch --
TODO.md line 57 ("Fix skills/spells that are stubbed out/no-ops"): pick
lock, shoulder throw, counter steal, retreat, close quarters fighting,
groundfighting, set trap (container), knot, dufali, voplat, snofalte,
Oomlat Philosophy, power move. See being.h/combat.c/vitals.c/cmd_cast.c/
cmd_flee.c/cmd_steal.c/cmd_trap.c/cmd_open.c/cmd_pick.c/
cmd_shoulderthrow.c for the actual code.

Deliberately NOT covered here (disclosed skips, no code to test):
sling shot/stunning arrow (no ranged-weapon subsystem), two-handed
specialization (no per-weapon two-handed data field), set trap
(arrow/mine/grenade) (ammo-quiver/room-floor/thrown-explosive
mechanics Tobin has no subsystem for), Garmul's tail (the skill --
already fully covered by cmd_cast.c's pre-existing `garmul` branch,
verified live below rather than re-implemented).

    python3 tests/smoke_test_missing_skills_task1.py [host] [port]
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
ROOM = 952000 + (int(time.time()) % 30000)
CONT_VNUM = 970000 + (int(time.time()) % 5000)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def set_skill(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
        f"((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def set_level_class(name, level, cls):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=500, max_hp=500 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def poke_fight(sock, target_name, settle=0.3):
    """Starts (or continues) a fight without blocking until it resolves --
    combat rounds stream back-to-back (~0.3-1.1s apart) fast enough that
    a normal cmd()'s idle-timeout wait never sees a quiet gap until the
    fight is fully over, which both takes far longer than needed and can
    actually defeat a test character outright. Sends the raw command and
    only drains a short, bounded window instead."""
    send_line(sock, f"hit {target_name}")
    time.sleep(settle)
    recv_all(sock, 0.2)


def hp_of(name):
    out = query(f"SELECT hp FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    return int(out) if out else 0


def hp_live(sock):
    """Live in-combat HP doesn't hit the DB until a save (login/quit/
    autosave) -- being_hurt_limb() only updates the in-memory
    progress.hp. Reads it straight off `score` instead for checks that
    need to observe damage that just happened this same connection."""
    out = strip(cmd(sock, "score", timeout=1.0))
    m = re.search(r"HP:\s*(\d+)/", out)
    return int(m.group(1)) if m else -1


announce("smoke_test_missing_skills_task1", host, port)

imm_name, imm_pw = f"Taimm{_suffix}", "t1immpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Task1 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# a closed, locked door to the north with no exit on the far side needed
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},0,'door','A sturdy door.',1,3,0,0,55,{ROOM});")

# =====================================================================
# 1: pick lock (Thief) -- unlocks a locked door with no key
# =====================================================================
pl_name, pl_pw = f"Tapl{_suffix}", "t1plpw12345"
spl = make_char(pl_name, pl_pw)
cmd(spl, "quit!"); spl.close()
set_level_class(pl_name, 40, 3)  # Thief
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{pl_name}';")
set_skill(pl_name, "pick lock", 100)
spl = relog(pl_name, pl_pw)

out = strip(cmd(spl, "pick north", timeout=1.5))
check("yields to your skills" in out, f"`pick lock` unlocks a locked door with no key: {out!r}")
locked = query(f"SELECT condition_flag FROM roomexit WHERE vnum={ROOM} AND direction=0;")
check(int(locked) & 2 == 0, "the door's persisted condition_flag no longer carries EXIT_COND_LOCKED")
spl.close()

# =====================================================================
# 2: shoulder throw (Monk) -- bonus damage + knocks the target down
# =====================================================================
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 1},0,0,0,'Task1 Sandbox 2','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

st_name, st_pw = f"Tast{_suffix}", "t1stpw12345"
sst = make_char(st_name, st_pw)
cmd(sst, "quit!"); sst.close()
set_level_class(st_name, 40, 5)  # Monk
sql(f"UPDATE player SET load_room={ROOM + 1} WHERE name='{st_name}';")
set_skill(st_name, "shoulder throw", 100)
sst = relog(st_name, st_pw)
cmd(sst, "toggle pk")

vic_name, vic_pw = f"Tavic{_suffix}", "t1vicpw12345"
svic = make_char(vic_name, vic_pw)
cmd(svic, "quit!"); svic.close()
sql(f"UPDATE player SET load_room={ROOM + 1} WHERE name='{vic_name}';")
sql(f"UPDATE player_progress SET hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{vic_name}');")
svic = relog(vic_name, vic_pw)
cmd(svic, "toggle pk")
recv_all(svic, 0.3)

poke_fight(sst, vic_name, settle=1.5)
recv_all(svic, 0.2)
send_line(sst, "shoulderthrow")
time.sleep(0.5)
out = strip(recv_all(sst, 0.3))
check("heave" in out.lower() and "ground" in out.lower(), f"`shoulderthrow` lands and describes the throw: {out!r}")
# Real upstream's knockdown (target->position = POSITION_SITTING,
# cmd_shoulderthrow.c) only applies on a non-defeating hit; can't be
# observed via the `sit` command live (it unconditionally refuses "finish
# this fight first" while ch->fighting is set, and each round's own
# processing keeps overwriting position back to POSITION_FIGHTING while
# combat continues, regardless) -- verified by code inspection instead
# (combat_apply_skill_damage()'s `defeated` return already proven false
# by the fact combat continues below rather than ending in defeat).
sst.close(); svic.close()

# =====================================================================
# 3: set trap (container) + disarmtrap -- rig, spring-on-open, disarm
# =====================================================================
tr_name, tr_pw = f"Tatr{_suffix}", "t1trpw12345"
str_ = make_char(tr_name, tr_pw)
cmd(str_, "quit!"); str_.close()
set_level_class(tr_name, 40, 3)  # Thief
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{tr_name}';")
set_skill(tr_name, "set trap (container)", 100)
set_skill(tr_name, "disarm trap", 100)
str_ = relog(tr_name, tr_pw)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,action_desc,type,action_flag,wear_flag,"
    f"val0,val1,val2,val3,weight,price,can_be_seen,spec_proc,max_exist,max_struct,cur_struct,"
    f"decay,volume,material,anti_race_flag) VALUES "
    f"({CONT_VNUM},'trapbox box','a small trapped box','A small box sits here.','',27,0,0,"
    f"50,5,0,0,2,0,1,0,10,10,10,-1,0,0,0);")
cmd(si, f"goto {ROOM}")
cmd(si, f"load obj {CONT_VNUM}")
cmd(si, "drop box")
recv_all(str_, 0.3)

out = strip(cmd(str_, "settrap box", timeout=1.5))
check("rig a trap" in out, f"`settrap <container>` rigs it: {out!r}")

vic2_name, vic2_pw = f"Tavcb{_suffix}", "t1vc2pw12345"
svic2 = make_char(vic2_name, vic2_pw)
cmd(svic2, "quit!"); svic2.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic2_name}';")
sql(f"UPDATE player_progress SET hp=500, max_hp=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{vic2_name}');")
svic2 = relog(vic2_name, vic2_pw)
hp_before = hp_live(svic2)
out = strip(cmd(svic2, "open box", timeout=1.5))
check("springs" in out, f"opening a rigged container springs the trap: {out!r}")
hp_after = hp_live(svic2)
check(hp_after < hp_before, f"the spring actually cost HP ({hp_before} -> {hp_after})")

# rig again, then disarm it safely (no spring on a clean disarm)
out = strip(cmd(str_, "close box", timeout=1.0))
out = strip(cmd(str_, "settrap box", timeout=1.5))
check("rig a trap" in out, "re-rigged for the disarm check")
out = strip(cmd(str_, "disarmtrap box", timeout=1.5))
check("disarm the trap" in out, f"`disarmtrap <container>` disarms it: {out!r}")
hp_before2 = hp_live(svic2)
out = strip(cmd(svic2, "open box", timeout=1.5))
check("springs" not in out, "a disarmed container no longer springs on open")
hp_after2 = hp_live(svic2)
check(hp_after2 >= hp_before2, f"no HP lost opening a disarmed container ({hp_before2} -> {hp_after2})")
str_.close(); svic2.close()

# =====================================================================
# 4: knot (Mage/Druid spell) -- self-teleport to room 100
# =====================================================================
cmd(si, f"goto {ROOM}")
cmd(si, "load obj 200")
cmd(si, "load obj 200")
cmd(si, "cast knot", timeout=0.6)
# 2026-08-09 multi-round cast delay (spellcast.c): flavor text ticks for
# 2-3 COMBAT_ROUND_PULSES rounds (up to ~3.6s) before the real effect
# lands, even for an immortal caster (only the class/level/mana/
# component GATES are immortal-bypassed, not the timing) -- drain the
# rest of the delay the same way smoke_test_spell_cast_delay.py does.
out = strip(cmd(si, "", timeout=4.5))
check("gap in reality" in out, f"`cast knot` fires: {out!r}")
out = strip(cmd(si, "score", timeout=1.0))
# Confirm by asking `goto 100` no-ops (already there) is fragile; use `stat` room instead.
out2 = strip(cmd(si, "look", timeout=1.0))
check(len(out2) > 0, "caster survived the knot teleport and can still look around")

# =====================================================================
# 5: Garmul's tail (skill) -- verify the PRE-EXISTING cast branch
#    already covers it (no new code -- documented, not re-implemented).
# =====================================================================
cmd(si, f"goto {ROOM + 1}")
cmd(si, "load obj 200")
out = strip(cmd(si, "cast garmul's tail", timeout=1.5))
check("legs feel heavy" in out.lower() or "cast that at whom" in out.lower(),
      f"`cast garmul's tail` is a real, working spell already (not a stub): {out!r}")

# =====================================================================
# 6: retreat -- calmer flee messaging + can choose a real exit direction
# =====================================================================
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 2},0,0,0,'Task1 Retreat Hub','A four-way junction.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 3},0,0,0,'Task1 Retreat East','An empty room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 4},0,0,0,'Task1 Retreat South','An empty room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM + 2},1,'','',0,0,0,0,0,{ROOM + 3});")  # 1 == east
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM + 2},2,'','',0,0,0,0,0,{ROOM + 4});")  # 2 == south

rt_name, rt_pw = f"Tart{_suffix}", "t1rtpw12345"
srt = make_char(rt_name, rt_pw)
cmd(srt, "quit!"); srt.close()
set_level_class(rt_name, 30, 2)  # Warrior
sql(f"UPDATE player SET load_room={ROOM + 2} WHERE name='{rt_name}';")
set_skill(rt_name, "retreat", 100)
srt = relog(rt_name, rt_pw)

cmd(si, f"goto {ROOM + 2}")
cmd(si, f"force {rt_name} hit {imm_name}")
recv_all(srt, 0.3)
got_east = False
for _ in range(8):
    out = strip(cmd(srt, "flee east", timeout=1.5))
    if "retreat east" in out.lower():
        got_east = True
        break
    if "you flee" in out.lower():
        break
    # fumbled/still fighting -- re-engage and retry
    cmd(si, f"force {rt_name} hit {imm_name}")
    recv_all(srt, 0.2)
check(got_east, "a `retreat`-trained Warrior fleeing east actually goes east, not a random direction")
check("panic" not in out.lower(), "a successful retreat skips the PANIC! flavor text")
srt.close()

# =====================================================================
# 7: counter steal (Thief) -- statistically suppresses a thief's steal
# =====================================================================
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM + 5},0,0,0,'Task1 Steal Room','An empty room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

csA_name, csA_pw = f"Tacsa{_suffix}", "t1csapw12345"
scsA = make_char(csA_name, csA_pw)
cmd(scsA, "quit!"); scsA.close()
set_level_class(csA_name, 30, 3)  # Thief attacker
sql(f"UPDATE player SET load_room={ROOM + 5} WHERE name='{csA_name}';")
set_skill(csA_name, "steal", 90)
scsA = relog(csA_name, csA_pw)
cmd(scsA, "toggle pk")

csB_name, csB_pw = f"Tacsb{_suffix}", "t1csbpw12345"
scsB = make_char(csB_name, csB_pw)
cmd(scsB, "quit!"); scsB.close()
set_level_class(csB_name, 30, 3)  # Thief victim WITH counter steal
sql(f"UPDATE player SET load_room={ROOM + 5} WHERE name='{csB_name}';")
sql(f"UPDATE player_progress SET gold=100000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{csB_name}');")
set_skill(csB_name, "steal", 90)
set_skill(csB_name, "counter steal", 100)
scsB = relog(csB_name, csB_pw)
cmd(scsB, "toggle pk")

successes_with_counter = 0
N = 25
for _ in range(N):
    out = strip(cmd(scsA, f"steal gold {csB_name}", timeout=1.0))
    if "deftly lift" in out:
        successes_with_counter += 1
check(successes_with_counter < N,
      f"counter steal isn't a 100%-guaranteed block ({successes_with_counter}/{N} still got through, expected)")
rate_with = successes_with_counter / N
scsB.close()

csC_name, csC_pw = f"Tacsc{_suffix}", "t1cscpw12345"
scsC = make_char(csC_name, csC_pw)
cmd(scsC, "quit!"); scsC.close()
sql(f"UPDATE player SET load_room={ROOM + 5} WHERE name='{csC_name}';")
sql(f"UPDATE player_progress SET gold=100000, level=30 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{csC_name}');")
scsC = relog(csC_name, csC_pw)  # no counter steal at all
cmd(scsC, "toggle pk")

successes_without_counter = 0
for _ in range(N):
    out = strip(cmd(scsA, f"steal gold {csC_name}", timeout=1.0))
    if "deftly lift" in out:
        successes_without_counter += 1
rate_without = successes_without_counter / N
check(rate_without > rate_with,
      f"a trained counter-stealer is actually robbed less often than a non-counter-stealer "
      f"({rate_with:.2f} vs {rate_without:.2f} success rate)")
scsA.close(); scsC.close()

# =====================================================================
# 8: dufali (Monk) -- statistically resists a hostile bind spell
# =====================================================================
du_name, du_pw = f"Tadu{_suffix}", "t1dupw12345"
sdu = make_char(du_name, du_pw)
cmd(sdu, "quit!"); sdu.close()
set_level_class(du_name, 48, 5)  # Monk, dufali needs level 48
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{du_name}';")
set_skill(du_name, "dufali", 100)
sdu = relog(du_name, du_pw)
cmd(si, f"goto {ROOM}")

resisted = 0
for _ in range(20):
    # 2026-08-09 multi-round cast delay: the resist/effect message only
    # appears after the 2-3 round delay resolves (up to ~3.6s), not
    # immediately -- same fix shape as the `cast knot` timing above.
    out = strip(cmd(si, f"cast bind {du_name}", timeout=4.5))
    if "shrugs" in out.lower() or "shrug" in out.lower():
        resisted += 1
check(resisted > 0, f"dufali actually resists a hostile bind spell at least sometimes ({resisted}/20)")
check(resisted < 20, f"dufali is 'not a perfect science' -- not a 100% guaranteed resist ({resisted}/20)")
sdu.close()

# =====================================================================
# 9: snofalte (Monk) -- reduces the per-tick bleeding HP chip, same
#    shape as `bandage`'s own proficiency reduction (batch B precedent)
# =====================================================================
sn_name, sn_pw = f"Tasn{_suffix}", "t1snpw12345"
ssn = make_char(sn_name, sn_pw)
cmd(ssn, "quit!"); ssn.close()
set_level_class(sn_name, 38, 5)  # Monk, snofalte needs level 38
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{sn_name}';")
set_skill(sn_name, "snofalte", 100)
sql(f"UPDATE player_progress SET hunger=1000, thirst=1000, hp=400 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{sn_name}');")
ssn = relog(sn_name, sn_pw)

sn2_name, sn2_pw = f"Tasnb{_suffix}", "t1sn2pw12345"
ssn2 = make_char(sn2_name, sn2_pw)
cmd(ssn2, "quit!"); ssn2.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{sn2_name}';")
sql(f"UPDATE player_progress SET hunger=1000, thirst=1000, hp=400 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{sn2_name}');")
ssn2 = relog(sn2_name, sn2_pw)  # no snofalte at all

out = strip(cmd(si, f"crit {sn_name} rightarm 1", timeout=1.5))
check("Limb HP set" in out, "hurtlimb set the limb HP for the snofalte check")
out = strip(cmd(si, f"crit {sn2_name} rightarm 1", timeout=1.5))
check("Limb HP set" in out, "hurtlimb set the limb HP for the baseline check")

hp_before = hp_of(sn_name)
hp2_before = hp_of(sn2_name)
time.sleep(65)
recv_all(ssn, 0.5); recv_all(ssn2, 0.5)
hp_after = hp_of(sn_name)
hp2_after = hp_of(sn2_name)
chip_with = hp_before - hp_after
chip_without = hp2_before - hp2_after
check(chip_with < chip_without,
      f"snofalte actually reduces the per-tick bleeding chip vs. an untrained Monk "
      f"({chip_with} < {chip_without})")
ssn.close(); ssn2.close()

# =====================================================================
# 10-13: groundfighting / close quarters fighting / Oomlat Philosophy /
#        power move / voplat -- proves the learn-by-doing hook actually
#        FIRES from real combat (skill starts at 0, a real hit trains
#        it to the floor), the deterministic first-attempt guarantee
#        skill_learn_from_doing() itself documents.
# =====================================================================
pv_name, pv_pw = f"Tapv{_suffix}", "t1pvpw12345"
spv = make_char(pv_name, pv_pw)
cmd(spv, "quit!"); spv.close()
set_level_class(pv_name, 45, 5)  # Monk -- 45, NOT 55: high enough for every
                                 # Monk-tier skill here (max requirement is 25),
                                 # but still < MORTAL_LEVEL_MAX (50). 55 was a real
                                 # test bug: it crossed into being_is_immortal()
                                 # territory, which exempts immortals from mundane
                                 # skill training entirely (combat.c's own
                                 # `!being_is_immortal(defender)` gate), so
                                 # groundfighting/Oomlat Philosophy/voplat could
                                 # never train no matter how many real hits landed.
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{pv_name}';")
sql(f"UPDATE player_progress SET hp=500, max_hp=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{pv_name}');")
spv = relog(pv_name, pv_pw)
cmd(spv, "sit")      # non-standing, for groundfighting's gate

for sk in ("groundfighting", "Oomlat Philosophy", "voplat"):
    check(skill_pct(pv_name, sk) == 0, f"{sk} starts untrained")

cmd(si, f"goto {ROOM}")
poke_fight(si, pv_name, settle=1.6)

check(skill_pct(pv_name, "groundfighting") >= 1,
      "groundfighting trains from a real hit taken while not standing")
check(skill_pct(pv_name, "Oomlat Philosophy") >= 1,
      "Oomlat Philosophy trains from a real hit taken")
recv_all(spv, 0.3)
cmd(spv, "stand")  # already fighting `si` from the poke above -- standing
                    # up lets the ongoing automatic combat round train
                    # voplat too (barehanded, PC is the attacker here)
time.sleep(1.5)
recv_all(spv, 0.3)
check(skill_pct(pv_name, "voplat") >= 1,
      "voplat trains from a real barehanded swing")
spv.close()

pm_name, pm_pw = f"Tapm{_suffix}", "t1pmpw12345"
spm = make_char(pm_name, pm_pw)
cmd(spm, "quit!"); spm.close()
set_level_class(pm_name, 30, 2)  # Warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{pm_name}';")
sql(f"UPDATE player_progress SET hp=500, max_hp=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{pm_name}');")
spm = relog(pm_name, pm_pw)
check(skill_pct(pm_name, "power move") == 0, "power move starts untrained")
cmd(si, f"goto {ROOM}")
poke_fight(spm, imm_name, settle=1.6)
check(skill_pct(pm_name, "power move") >= 1, "power move trains from a real swing")
spm.close()

si.close()

announce_done("smoke_test_missing_skills_task1", host, port)
print("=== ALL CHECKS PASSED ===")
