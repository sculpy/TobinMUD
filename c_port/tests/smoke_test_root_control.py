#!/usr/bin/env python3
"""Smoke test for the Druid `root control` spell (Tier 2 port, 2026-08-16).

Real upstream SPELL_ROOT_CONTROL (SneezyMUD disc_shaman_spider.cc), folded
onto Druid (no Shaman class in Tobin). Unlike sibling `entangling roots`
(which only damages), root control ALSO knocks the victim down and costs
them a round -- the real crowd-control mechanic that made it a genuine
port gap. It is outdoor-gated ("in nature or on land"), same as entangling
roots / living vines.

An immortal Druid (whose casts resolve synchronously) casts it at a
mortal, huge-HP victim sharing its room. Checks:
  * outdoors + non-lethal hit -> own message (not the generic stub),
    the victim takes damage, and the victim is knocked to the ground
    (POSITION_SITTING, "crash to the ground") losing a round;
  * indoors -> refused with the outdoor-gate message, no effect.

Same immortal-caster + mortal-target + load_room scaffolding as
smoke_test_druid_batch_2026_08_11.py.

    python3 tests/smoke_test_root_control.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_root_control", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OUT = 976000 + (int(time.time()) % 18000)   # outdoor sandbox (room_flag=1, no INDOORS bit)
ROOM_IN = ROOM_OUT + 1                             # indoor sandbox (room_flag=1|8)
COMPONENT = ROOM_OUT + 2


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    s.close()


def make_char_and_quit(name, pw, class_choice):
    """Create the character AND send a real `quit!` before closing -- a
    raw close() leaves a linkdead body whose room outranks load_room on
    the next connect, stranding a load_room-routed mortal target."""
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


def hp_of(sock):
    m = re.search(r"HP:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1)) if m else None


druid = f"Rcdru{_suffix}"
vic = f"Rcvic{_suffix}"
pw = "rcdrupw1234"

sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
# room_flag=1 (bit 0) = NOT indoors (INDOORS is 1<<3=8) -> outdoor room.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'Root Control Outdoor Sandbox','A bare field.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# room_flag=9 (1|8) -> INDOORS bit set.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IN},0,0,0,'Root Control Indoor Sandbox','A bare hall.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

make_char(druid, pw, "5")            # class 5 = Druid (immortal, self-gotos)
make_char_and_quit(vic, pw, "3")     # class 3 = Warrior (load_room-routed target)

# Immortal Druid caster: level >= IMMORTAL_LEVEL_MIN=51 -> casts resolve
# synchronously; component gate bypassed; full discipline.
sql(f"UPDATE player_progress SET level=60,"
    f"basic_disc_pct=100,advanced_disc_pct=100,combat_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{druid}');")
# Victim: MORTAL, level 50, huge HP so the non-lethal knockdown branch runs.
sql(f"UPDATE player_progress SET level=50,hp=999999,max_hp=999999 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name='{vic}';")

sv = login(vic, pw)
s = login(druid, pw)
check("Root Control Outdoor Sandbox" in cmd(s, f"goto {ROOM_OUT}"),
      "the Druid reaches the outdoor sandbox room")
recv_all(sv)  # drain the Druid's arrival notice
check(vic.lower() in cmd(s, "look").lower(),
      "the victim shares the room (name targeting will resolve)")

cmd(s, "toggle pk")
cmd(sv, "toggle pk")


def cast(spell, target=None):
    cmd(s, f"load obj {COMPONENT}")
    line = f"cast {spell}" + (f" {target}" if target else "")
    return cmd(s, line, timeout=1.5)


# --- outdoors: real message + damage + knockdown -------------------------
recv_all(sv)
before = hp_of(sv)
out = cast("root control", vic)
# Grab the victim's async notify FIRST -- reading `score` (hp_of) on the
# victim socket would otherwise consume the interleaved knockdown line.
vic_saw = recv_all(sv)
after = hp_of(sv)

check("tree roots" in out.lower() and "ground" in out.lower(),
      "root control resolves with its own message, not the generic stub")
check("nothing happens yet" not in out.lower(),
      "root control does NOT hit the generic 'nothing happens yet' stub")
check(after is not None and before is not None and after < before,
      f"root control actually damages the victim ({before} -> {after})")
# The victim-side "crash to the ground" notify is only sent inside the
# knockdown branch (the `if (!defeated)` that also sets POSITION_SITTING
# and being_set_wait) -- so seeing it proves the real knockdown fired,
# not just damage. (Position isn't shown in the room listing, and the
# victim's own `score` would be queued behind their new combat-round
# wait, so this async notify is the robust knockdown signal.)
check("crash to the ground" in vic_saw.lower(),
      "the victim is knocked down (crashes to the ground)")

# --- indoors: outdoor gate refuses ---------------------------------------
# The gate lives in the outer cast dispatcher and fires BEFORE any target
# resolution, so no victim is needed in the room for it to trigger.
check("Root Control Indoor Sandbox" in cmd(s, f"goto {ROOM_IN}"),
      "the Druid reaches the indoor sandbox room")
cmd(s, f"load obj {COMPONENT}")
out_in = cmd(s, "cast root control", timeout=1.5)
check("only works outdoors" in out_in.lower(),
      "indoors, root control is refused by the outdoor gate")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
send_line(sv, "quit!"); recv_all(sv)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{druid}','{vic}'));")
sql(f"DELETE FROM player WHERE name IN ('{druid}','{vic}');")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")

announce_done("smoke_test_root_control", host, port)
print("=== ALL CHECKS PASSED ===")
