#!/usr/bin/env python3
"""Smoke test for the Monk sash quest chain (monk_quest.h/.c) -- white
belt, yellow sash, and purple sash turn-ins end to end, plus a spot-check
that eligibility gating actually blocks a below-level/out-of-order
attempt. Full 7-stage (blue/green/red/black) coverage is out of scope for
this test (time-boxed per the session's own scope); white/yellow/purple
below exercise all three of monk_quest.c's own mechanisms once each: the
real level-up eligibility hook (monk_quest_on_levelup, via a genuine
combat kill/level-gain), an item turn-in (`give`, monk_quest_on_give),
and the leper kill-counter mechanic (monk_quest_on_mob_kill).

    python3 tests/smoke_test_monk_quest.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 964000 + (int(time.time()) % 30000)
WEAK_MOB = ROOM + 1  # freshly-created level-10 mob, used only for the real level-1->2 kill


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
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


def kill_until_dead(sock, target, max_rounds=15):
    out = cmd(sock, f"kill {target}", 1.0)
    for _ in range(max_rounds):
        if "have slain" in out.lower() or "have defeated" in out.lower():
            return out
        out += cmd(sock, "", 1.2)
    return out


announce("smoke_test_monk_quest", host, port)

imm_name, imm_pw = f"Mqti{_suffix}", "mqtipw123456"
monk_name, monk_pw = f"Mqtm{_suffix}", "mqtmpw123456"

# --- Immortal setup (Warrior, level 59 -- goto/load) ---
s1 = make_char(imm_name, imm_pw, 3)  # Warrior
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Monk Quest Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Monk Quest Sandbox" in cmd(s1, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# A weak, easily-killed level-10 mob -- used ONLY to trigger a real
# combat level-up (1 -> 2) so monk_quest_on_levelup() actually fires
# through its real call site (combat.c's combat_award_hit_xp()), not
# just via a SQL-set level.
_mob_row = {
    "vnum": str(WEAK_MOB),
    "name": f"'strawmonk{_suffix}'",
    "short_desc": "'a straw training dummy'",
    "long_desc": "'A straw training dummy stands here.'",
    "description": "'A stuffed practice dummy.'",
    "actions": "0", "affects": "0", "faction": "0", "fact_perc": "0",
    "letter": "'A'", "attacks": "1.0", "class": "0", "level": "10",
    "tohit": "-999", "ac": "0.0", "hpbonus": "0.02",
    "damage_level": "0", "damage_precision": "0", "gold": "0",
    "race": "0", "weight": "0", "height": "0",
    "str": "0", "bra": "0", "con": "0", "dex": "0", "agi": "0",
    "intel": "0", "wis": "0", "foc": "0", "per": "0", "cha": "0", "kar": "0", "spe": "0",
    "pos": "10", "def_position": "10", "sex": "1",
    "spec_proc": "0", "skin": "0", "vision": "0", "can_be_seen": "1", "max_exist": "1",
}
sql(f"INSERT INTO mob ({', '.join(_mob_row.keys())}) VALUES ({', '.join(_mob_row.values())});")

# --- Monk PC ---
sm = make_char(monk_name, monk_pw, 6)  # Monk
cmd(sm, "quit!"); sm.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{monk_name}';")
# Boosted attrs/HP from the start -- this test cares about the monk_quest.c
# dispatch logic firing correctly, not about a fair fight; a level-1 Monk's
# default stats make even a weak training dummy an unreliable, slow kill.
sql(f"UPDATE player_attrs SET strength=250, dexterity=250 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{monk_name}');")
sql(f"UPDATE player_progress SET hp=500, max_hp=500, basic_disc_pct=100, "
    f"combat_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{monk_name}');")
sm = relog(monk_name, monk_pw)
check("Monk Quest Sandbox" in cmd(sm, "look"), "the level-1 Monk lands in the sandbox room")

# --- Guildmaster 207 + Huang'lo 385, loaded into the sandbox ---
check("You conjure" in cmd(s1, "load mob 207"), "the monk guildmaster (207) is loaded")
check("You conjure" in cmd(s1, "load mob 385"), "Huang'lo (385) is loaded")

# --- Gating spot-check: a level-1 Monk is NOT eligible for white belt yet ---
out = cmd(sm, "say white belt", 0.7)
check("seek out huang'lo" not in out.lower(),
      "eligibility gating blocks the white belt quest text before level 2")

# --- Real level-up (1 -> 2) via genuine combat, exercising monk_quest_on_levelup() ---
check("You conjure" in cmd(s1, f"load mob {WEAK_MOB}"), "the training dummy is loaded")
out = kill_until_dead(sm, "strawmonk")
check("have slain" in out.lower() or "have defeated" in out.lower(), "the Monk kills the training dummy")
check("more experienced" in out.lower(), "the kill grants enough XP to level up")

out = cmd(sm, "say white belt", 0.7)
check("seek out huang'lo" in out.lower(),
      "now level-2, the guildmaster's white belt eligibility text fires")

# --- White belt turn-in (give bandage -> 207) ---
check("You conjure" in cmd(s1, "load obj 9"), "a bandage (vnum 9) is loaded into the immortal's inventory")
cmd(s1, f"give bandage {monk_name}", 0.5)
out = cmd(sm, "give bandage guildmaster", 0.7)
check("white belt" in out.lower(), "the guildmaster sews the bandage into a white belt")
check("white belt" in cmd(sm, "inventory").lower(), "the white belt is now in the Monk's inventory")

# --- Yellow sash: fast-track eligibility via SQL (the level-5 gate itself
# is exercised once already above; re-testing via a full combat grind to
# level 5 isn't worth the runtime) ---
sql(f"UPDATE player_progress SET level=5 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{monk_name}');")
sql(f"UPDATE player SET monk_quest_flags = monk_quest_flags | 8 WHERE name='{monk_name}';")  # MQ_YELLOW_ELIGIBLE
sm2 = relog(monk_name, monk_pw)
out = cmd(sm2, "say yellow sash", 0.7)
check("metal ashtray" in out.lower(), "the guildmaster asks for a metal ashtray")
check("You conjure" in cmd(s1, "load obj 3319"), "a metal ashtray (vnum 3319) is loaded")
cmd(s1, f"give ashtray {monk_name}", 0.5)
out = cmd(sm2, "give ashtray guildmaster", 0.7)
check("yellow sash" in out.lower(), "the guildmaster hands over a yellow sash")
check("yellow sash" in cmd(sm2, "inventory").lower(), "the yellow sash is now in the Monk's inventory")

# --- Purple sash: fast-track eligibility, then the real leper-kill-counter mechanic ---
sql(f"UPDATE player_progress SET level=15, hp=5000, max_hp=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{monk_name}');")
sql(f"UPDATE player_attrs SET strength=250, dexterity=250 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{monk_name}');")
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{monk_name}');")
sql(f"UPDATE player SET monk_quest_flags = monk_quest_flags | 32 WHERE name='{monk_name}';")  # MQ_PURPLE_ELIGIBLE
sm3 = relog(monk_name, monk_pw)
out = cmd(sm3, "say purple sash", 0.7)
check("lepers infest" in out.lower(), "the guildmaster's leper-colony rant fires")
out = cmd(sm3, "say i am ready to slaughter lepers", 0.7)
check("go" in out.lower(), "the guildmaster sends the Monk off to slaughter lepers")

# Gating spot-check: turning in "leper" before 5 kills must NOT award the sash.
out = cmd(sm3, "say leper", 0.7)
check("purple sash" not in out.lower(),
      "eligibility gating blocks the purple sash award before 5 lepers are slain")
check("come back to me" in out.lower(), "the guildmaster reminds the Monk to finish the kill count")

for i in range(5):
    check("You conjure" in cmd(s1, "load mob 6602"), f"leper #{i + 1} is loaded")
    out = kill_until_dead(sm3, "leper")
    check("have slain" in out.lower() or "have defeated" in out.lower(), f"the Monk kills leper #{i + 1}")

out = cmd(sm3, "say leper", 0.7)
check("purple sash" in out.lower(), "after 5 leper kills, the guildmaster awards the purple sash")
check("purple sash" in cmd(sm3, "inventory").lower(), "the purple sash is now in the Monk's inventory")

sm3.close()
s1.close()

announce_done("smoke_test_monk_quest", host, port)
print("=== ALL CHECKS PASSED ===")
