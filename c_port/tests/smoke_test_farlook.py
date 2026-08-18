#!/usr/bin/env python3
"""Smoke test for `farlook` (Mage, level 25, Advanced tier -- the level-25
audit batch alongside whirlwind/kneestrike/paralyze).

farlook is the ONE spell in the offensive-breadth roster that scries a
REMOTE room: it takes a being connected ANYWHERE in the world (global
descriptor lookup by name-prefix, same reach cmd_tell.c/cmd_transfer.c
use), and shows the caster that being's room name + description WITHOUT
moving anyone. Every other targeted spell is room-scoped
(combat_find_room_target()), so no existing test exercises the cross-room
reveal -- that's the whole point of this one.

Two paths are proven, both fully deterministic (the farlook branch in
cmd_cast.c returns BEFORE the mana-cost gate and the skill-proficiency
success roll, so a caster who reaches it always succeeds):

  1. A real MORTAL mage (level 25, disciplines maxed, holding the real
     bound component -- electrum bar, obj vnum 54) -- the genuine
     level-25 use, exercising component_for_cast()'s real reagent branch.
  2. An IMMORTAL caster -- the component/level/discipline bypass path.

The target sits in a DIFFERENT room from the caster (deterministic
placement via the immortal's goto/transfer, sidestepping the documented
login-room flakiness that relying on load_room hits), so a passing reveal
of the target's room name PROVES the remote scry rather than accidentally
echoing the caster's own room.

    python3 tests/smoke_test_farlook.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_CAST = 947000 + (int(time.time()) % 20000)   # where the caster stands
ROOM_TARGET = ROOM_CAST + 1                        # the remote room being scried

FARLOOK_COMP_VNUM = 247  # the real bound reagent (type-30 "eyes of an eagle", val2=54)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_full_discipline(name):
    sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_farlook", host, port)

# --- Immortal helper (level 58): goto / transfer / load obj only. ---
imm_name, imm_pw = f"Flimm{_suffix}", "flimmpw123"
s = make_char(imm_name, imm_pw, "1")  # Mage
cmd(s, "quit!")
s.close()
set_level(imm_name, 58)
imm = relog(imm_name, imm_pw)

# Two rooms: the caster's, and a DISTINCTIVELY named remote room to scry.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_CAST},0,0,0,'Farlook Casting Chamber','A bare casting chamber.\\n',"
    f"NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_TARGET},0,0,0,'Farlook Distant Vista',"
    f"'A windswept vista visible only through the scrying cloud.\\n',"
    f"NULL,1,0,0,0,0,0,0,0,0,0);")
check("Farlook Casting Chamber" in cmd(imm, f"goto {ROOM_CAST}"), "immortal goto's the casting chamber")

# --- Mortal mage caster (level 25, disciplines maxed). ---
mage_name, mage_pw = f"Flmag{_suffix}", "flmagpw123"
sm = make_char(mage_name, mage_pw, "1")  # Mage
cmd(sm, "quit!")
sm.close()
set_level(mage_name, 25)
set_full_discipline(mage_name)
mage = relog(mage_name, mage_pw)
# Placed deterministically with the caster next to the immortal, dodging
# the documented login-room flakiness.
check(f"to room {ROOM_CAST}" in cmd(imm, f"transfer {mage_name} {ROOM_CAST}"),
      "immortal transfers the mage into the casting chamber")
check("Farlook Casting Chamber" in cmd(mage, "look"), "the mage stands in the casting chamber")

# Hand the mage the REAL bound reagent ("eyes of an eagle", vnum 247).
check("You conjure" in cmd(imm, f"load obj {FARLOOK_COMP_VNUM}"), "farlook reagent loaded")
cmd(imm, "drop eagle")
check("you get" in cmd(mage, "get eagle").lower(), "the mage picks up the farlook reagent")

# --- Target player, placed in the REMOTE room. ---
tgt_name, tgt_pw = f"Fltgt{_suffix}", "fltgtpw123"
st = make_char(tgt_name, tgt_pw, "1")
cmd(st, "quit!")
st.close()
tgt = relog(tgt_name, tgt_pw)
check(f"to room {ROOM_TARGET}" in cmd(imm, f"transfer {tgt_name} {ROOM_TARGET}"),
      "immortal transfers the target into the distant vista")
check("Farlook Distant Vista" in cmd(tgt, "look"), "the target stands in the remote vista")

# ===== Negative paths (mortal mage, component in hand -- neither consumes it) =====
out = cmd(mage, "cast farlook")
check("farlook whom" in out.lower(), "farlook with no target asks whom")

out = cmd(mage, f"cast farlook Nobodyxyz{_suffix}")
check("can't seem to locate" in out.lower(), "farlook on an absent name reports it can't locate them")

# ===== Mortal mage: the real remote scry =====
out = cmd(mage, f"cast farlook {tgt_name}")
check("shimmers slightly before revealing" in out.lower(),
      "farlook conjures the scrying cloud")
check("Farlook Distant Vista" in out,
      "farlook reveals the TARGET's remote room name")
check("windswept vista" in out.lower(),
      "farlook reveals the target's remote room description")
# The reveal must be the target's room, not the caster's own.
check("Farlook Casting Chamber" not in out,
      "farlook shows the remote room, not the caster's own room")
# Nobody moved: the caster is still in the casting chamber.
check("Farlook Casting Chamber" in cmd(mage, "look"),
      "farlook does not move the caster")
check("Farlook Distant Vista" in cmd(tgt, "look"),
      "farlook does not move the target")

# ===== Immortal path: component/level/discipline bypass still scries =====
out = cmd(imm, f"cast farlook {tgt_name}")
check("shimmers slightly before revealing" in out.lower(),
      "an immortal casts farlook with no component")
check("Farlook Distant Vista" in out,
      "the immortal's farlook reveals the target's remote room")

s = imm; s.close()
mage.close()
tgt.close()

sql(f"DELETE FROM room WHERE vnum IN ({ROOM_CAST}, {ROOM_TARGET});")

print("=== ALL CHECKS PASSED ===")
announce_done("smoke_test_farlook", host, port)
