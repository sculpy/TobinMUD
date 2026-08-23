#!/usr/bin/env python3
"""Smoke test for four reference-parity gaps closed against SneezyMUD's
own group/follow/goto/transfer source (misc/utility.cc inGroup(),
misc/other.cc doGroup(), misc/immortal.cc doGoto()/doTrans()), found by a
prior read-only audit. This file covers the three group-system fixes;
smoke_test_goto_transfer_reference_fixes.py covers the fourth (goto
dragging immortal followers / transfer dragging mount+rider).

  1. being_in_group() mount/rider special-case (being.c): your own mount
     and your rider are ALWAYS "in group" with you regardless of the
     `grouped` flag -- proven via cmd_cast.c's area-effect friendly-fire
     exclusion: a caster's mount survives a room-wide spell that kills an
     otherwise-identical, equally fragile bystander mob.
  2. cmd_group.c eligibility guards (`group all`): skip an already-
     grouped candidate, skip your own mount, and refuse (with a message)
     an immortal-level NPC follower.
  3. `group <name>` toggle: ungroups an already-grouped target instead of
     being a no-op/error; blocked while that target is fighting; the
     leader self-ungrouping cascades to disband the whole group.

    python3 tests/smoke_test_group_reference_fixes.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_group_reference_fixes", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 60000)
HORSE_VNUM = ROOM + 1
VICTIM_VNUM = ROOM + 2
STEED2_VNUM = ROOM + 3
SPRITE_VNUM = ROOM + 4
FOE_VNUM = ROOM + 5
COMPONENT = ROOM + 6


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
    # `quit!` (not a raw close) -- a raw close leaves a linkdead body at
    # its CURRENT room, and reconnecting resumes there, ignoring
    # load_room entirely (see smoke_test_group.py's own note on this).
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


def insert_mob(vnum, name, short_desc, long_desc, race, level=1, hpbonus=-2):
    cols = {
        "vnum": vnum, "name": f"'{name}'", "short_desc": f"'{short_desc}'",
        "long_desc": f"'{long_desc}'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": level, "tohit": 0, "ac": 0, "hpbonus": hpbonus,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": race,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 1,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")


imm_name, imm_pw = f"Grfxima{_suffix}", "grfximapw123"
ldr_name, ldr_pw = f"Grfxldr{_suffix}", "grfxldrpw123"
mem_name, mem_pw = f"Grfxmem{_suffix}", "grfxmempw123"
pw = "grfxpw12345"

make_char(imm_name, imm_pw, "1")
set_level(imm_name, 51)
imm = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Group Fixes Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# max_hp = 20 + level*5 + hpbonus*10 -> 20+5-20 = 5, so ANY area-spell
# hit kills these two mobs -- makes "did it get hit" directly observable
# via whether it's still standing there afterward.
insert_mob(HORSE_VNUM, "horse steed", "a lone steed", "A lone steed stands here, saddled and ready.", 47)
insert_mob(VICTIM_VNUM, "victim dummy", "a fragile bystander", "A fragile bystander stands here.", 1)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")


def restock_component():
    cmd(imm, f"load obj {COMPONENT}")
    cmd(imm, "get pouch")


check("Group Fixes Sandbox" in cmd(imm, f"goto {ROOM}"), "immortal lands in the sandbox room")
cmd(imm, f"load mob {HORSE_VNUM}")
cmd(imm, f"load mob {VICTIM_VNUM}")
recv_all(imm, 0.3)

out = cmd(imm, "ride steed")
check("mount" in out.lower(), "immortal mounts the horse")

restock_component()
# `cast fireball` (like most spells) spans several game pulses of
# incantation text before landing -- cmd()'s default 1s idle-gap
# heuristic can return BEFORE the actual resolution message arrives, so
# wait explicitly for the completion marker instead.
out = cmd_until(imm, "cast fireball", "catching everyone nearby", deadline=15.0)
check("catching everyone nearby" in out.lower(), "area-effect fireball goes off")

out = strip(cmd(imm, "look"))
check("victim dummy" not in out.lower() and "corpse of a fragile bystander" in out.lower(),
      "the un-mounted, equally fragile bystander mob was killed by the area spell")
check("a lone steed is here" in out.lower() and "corpse of a lone steed" not in out.lower(),
      "the caster's OWN MOUNT survived the same area spell -- being_in_group()'s "
      "mount special-case excluded it from friendly fire even without AFF_GROUP")

# =================== Fix 2: eligibility guards (own mount + immortal NPC follower) ===================
insert_mob(STEED2_VNUM, "steed2 mount", "a second lone steed", "A second lone steed stands here.", 47)
insert_mob(SPRITE_VNUM, "pixie sprite", "an immortal-tier sprite", "An immortal-tier sprite hovers here.", 1, level=51)
cmd(imm, f"load mob {STEED2_VNUM}")
cmd(imm, f"load mob {SPRITE_VNUM}")
recv_all(imm, 0.3)

# `ensorcer` (charm) attaches an existing mob as a follower directly --
# the only way to get an NPC into someone's followers[] from outside
# (charm never resists a MOB target: being_race_resists() short-circuits
# false for THING_MOB, see being.c).
# `ensorcer` only allows ONE charmed pet at a time (being_find_charmed_pet()
# guard), so the own-mount exclusion and the immortal-NPC refusal are
# each proven with their own charmed follower, one after the other --
# `dismiss` releases/destroys the first before charming the second.
restock_component()
out = cmd_until(imm, "cast ensorcer steed2", "bends", deadline=15.0)
check("bends" in out.lower(), "immortal charms steed2, making it a follower")

out = cmd(imm, "ride steed2")
check("mount" in out.lower(), "immortal mounts their own (now-follower) steed2")

out = strip(cmd(imm, "group all"))
check("steed2" not in out.lower(), "`group all` never groups in the leader's own mount, even though it's a follower")
cmd(imm, "dismount")
cmd(imm, "dismiss")

restock_component()
out = cmd_until(imm, "cast ensorcer sprite", "bends", deadline=15.0)
check("bends" in out.lower(), "immortal charms the level-51 sprite, making it a follower")

out = strip(cmd(imm, "group all"))
check("is immortal and has no need of you" in out.lower(),
      "`group all` refuses to group in an immortal-level NPC follower, with the reference's own message")

# =================== Fix 2b + Fix 3: already-grouped skip, and the toggle ===================
make_char(ldr_name, pw, "1")
make_char(mem_name, pw, "1")
sql(f"UPDATE player SET load_room={ROOM} WHERE name IN ('{ldr_name}','{mem_name}');")

ldr = login(ldr_name, pw)
mem = login(mem_name, pw)
recv_all(ldr, 0.3); recv_all(mem, 0.3)

out = strip(cmd(mem, f"follow {ldr_name}"))
check("now follow" in out.lower(), "member attaches via follow")

out = strip(cmd(ldr, "group all"))
check("group in all 1" in out.lower(), "first `group all` groups in the one real follower")
out2 = strip(cmd(ldr, "group all"))
check("no followers to group in" in out2.lower(),
      "a second `group all` finds nothing left to add -- the already-grouped member was skipped")

# --- Fix 3: `group <name>` toggle ---
out = strip(cmd(ldr, f"group {mem_name}"))
check("ungroup" in out.lower(), "`group <name>` on an already-grouped member TOGGLES them out")
out = strip(cmd(ldr, "group"))
check("not grouped in yet" in out.lower(), "the member no longer shows as grouped")

out = strip(cmd(ldr, f"group {mem_name}"))
check("you group in" in out.lower(), "re-grouping the member works")

insert_mob(FOE_VNUM, "combat foe", "a combat foe", "A combat foe stands here, snarling.", 1, hpbonus=50)
cmd(imm, f"load mob {FOE_VNUM}")
recv_all(imm, 0.3)
send_line(mem, "attack foe")
recv_all(mem, 0.5)
out = strip(cmd(ldr, f"group {mem_name}"))
check("fighting" in out.lower(), "the leader can't ungroup a member who is currently fighting")
cmd(mem, "flee"); recv_all(mem, 0.5)

out = strip(cmd(ldr, f"group {ldr_name}"))
check("rest of the group" in out.lower(), "leader self-ungroup announces the whole group being ungrouped")
out_mem = strip(recv_all(mem, timeout=1.0))
check("disbanded" in out_mem.lower(), "the follower is notified the group was disbanded")
out = strip(cmd(ldr, "group"))
check("not grouped in yet" in out.lower() or "you aren't in a group" in out.lower(),
      "the leader itself is no longer grouped after the self-ungroup cascade")

imm.close(); ldr.close(); mem.close()
announce_done("smoke_test_group_reference_fixes", host, port)
print("=== ALL CHECKS PASSED ===")
