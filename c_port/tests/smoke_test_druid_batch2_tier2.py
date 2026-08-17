#!/usr/bin/env python3
"""Smoke test for the Tier-2 Druid spell batch (2026-08-16): transfix,
transform limb, shapeshift, creeping doom, stormy skies.

An immortal Druid (casts resolve synchronously; component gate bypassed)
exercises each spell against a mortal, huge-HP Warrior victim in an
outdoor sandbox room. Checks:

  * TRANSFIX      -> own "transfixed" message; the held victim then can't
                    attack (AFFECT_TRANSFIX gate in cmd_attack.c);
  * TRANSFORM LIMB-> "gills" grants AFFECT_WATERBREATH (shows in `affects`
                    as "Water Breathing"); "claws" grants the STR buff
                    (shows as "Transformed Limb");
  * SHAPESHIFT    -> own "you are now <beast>" message (descriptor-swap;
                    tested on a DEDICATED druid so the swap doesn't
                    disrupt the rest of the run);
  * CREEPING DOOM -> own "creeping doom" message, victim takes damage AND
                    is left poisoned (AFFECT_POISON);
  * STORMY SKIES  -> indoors it is refused ("open sky"), regardless of the
                    current world weather (the outdoor gate runs first).

    python3 tests/smoke_test_druid_batch2_tier2.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import finish_char_creation

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_druid_batch2_tier2", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OUT = 977000 + (int(time.time()) % 17000)   # outdoor sandbox (room_flag=1)
ROOM_IN = ROOM_OUT + 1                             # indoor sandbox (room_flag=1|8)
COMPONENT = ROOM_OUT + 2


def _connect_and_create(name, pw, class_choice):
    """Account preamble (name/confirm/password/password) then the shared,
    maintained character-creation walk from mud_creation."""
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in (name, "y", pw, pw):
        send_line(s, step); recv_all(s, 0.7)
    send_line(s, "new"); recv_all(s, 0.7)
    finish_char_creation(s, name, send_line, recv_all,
                         race="1", territory="1", char_class=class_choice)
    return s


def make_char(name, pw, class_choice):
    s = _connect_and_create(name, pw, class_choice)
    s.close()


def make_char_and_quit(name, pw, class_choice):
    s = _connect_and_create(name, pw, class_choice)
    send_line(s, "quit!"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


def hp_of(sock):
    m = re.search(r"HP:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1)) if m else None


druid = f"Dbdru{_suffix}"
druid2 = f"Dbshp{_suffix}"
vic = f"Dbvic{_suffix}"
pw = "dbdrupw1234"

sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'Druid Batch2 Outdoor Sandbox','A bare field.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IN},0,0,0,'Druid Batch2 Indoor Sandbox','A bare hall.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

make_char(druid, pw, "5")
make_char(druid2, pw, "5")
make_char_and_quit(vic, pw, "3")

for who in (druid, druid2):
    sql(f"UPDATE player_progress SET level=60,"
        f"basic_disc_pct=100,advanced_disc_pct=100,combat_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{who}');")
sql(f"UPDATE player_progress SET level=50,hp=999999,max_hp=999999 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name='{vic}';")

sv = login(vic, pw)
s = login(druid, pw)
check("Druid Batch2 Outdoor Sandbox" in cmd(s, f"goto {ROOM_OUT}"),
      "the Druid reaches the outdoor sandbox room")
recv_all(sv)
check(vic.lower() in cmd(s, "look").lower(),
      "the victim shares the room (name targeting will resolve)")

cmd(s, "toggle pk")
cmd(sv, "toggle pk")


def cast(spell):
    cmd(s, f"load obj {COMPONENT}")
    return cmd(s, f"cast {spell}", timeout=1.5)


# --- TRANSFIX: hold the victim, then it can't attack ---------------------
recv_all(sv)
out = cast(f"transfix {vic}")
check("transfix" in out.lower() and "frozen" in out.lower(),
      "transfix resolves with its own hold message, not the generic stub")
check("nothing happens yet" not in out.lower(),
      "transfix does NOT hit the generic 'nothing happens yet' stub")
# The held victim tries to fight back -> refused by the AFFECT_TRANSFIX gate.
vic_attack = cmd(sv, f"kill {druid}")
check("transfix" in vic_attack.lower(),
      "a transfixed victim is refused when it tries to attack")

# --- TRANSFORM LIMB: gills (waterbreath) + claws (STR buff) ---------------
out = cast("transform limb gills")
check("gills" in out.lower() and "breathe water" in out.lower(),
      "transform limb gills grows gills to breathe water")
aff = cmd(s, "affects")
check("water breathing" in aff.lower(),
      "transform limb gills grants the Water Breathing affect")

out = cast("transform limb claws")
check("claws" in out.lower(),
      "transform limb claws hardens the hands into claws")
aff = cmd(s, "affects")
check("transformed limb" in aff.lower(),
      "transform limb claws grants the Transformed Limb (strength) affect")

out = cast("transform limb elbow")
check("try: gills" in out.lower() or "can't transform that" in out.lower(),
      "transform limb rejects an unknown limb with the usage hint")

# --- CREEPING DOOM: damage + lingering poison ----------------------------
recv_all(sv)
before = hp_of(sv)
out = cast(f"creeping doom {vic}")
vic_saw = recv_all(sv)
after = hp_of(sv)
check("creeping doom" in out.lower() or "insects" in out.lower(),
      "creeping doom resolves with its own swarm message")
check(after is not None and before is not None and after < before,
      f"creeping doom damages the victim ({before} -> {after})")
vic_aff = cmd(sv, "affects")
check("poison" in vic_aff.lower() or "fester" in vic_saw.lower(),
      "creeping doom leaves the victim poisoned")

# --- STORMY SKIES: indoors it's refused (outdoor gate runs first) --------
check("Druid Batch2 Indoor Sandbox" in cmd(s, f"goto {ROOM_IN}"),
      "the Druid reaches the indoor sandbox room")
cmd(s, f"load obj {COMPONENT}")
out_in = cmd(s, "cast stormy skies", timeout=1.5)
check("open sky" in out_in.lower(),
      "indoors, stormy skies is refused (needs open sky)")

# --- SHAPESHIFT: dedicated druid (descriptor-swap) -----------------------
s2 = login(druid2, pw)
cmd(s2, f"goto {ROOM_OUT}")
cmd(s2, f"load obj {COMPONENT}")
out_sh = cmd(s2, "cast shapeshift", timeout=1.5)
check("you are now" in out_sh.lower() and "flesh" in out_sh.lower(),
      "shapeshift melts the druid into a beast form")
check("nothing happens yet" not in out_sh.lower(),
      "shapeshift does NOT hit the generic 'nothing happens yet' stub")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
send_line(s2, "quit!"); recv_all(s2)
send_line(sv, "quit!"); recv_all(sv)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{druid}','{druid2}','{vic}'));")
sql(f"DELETE FROM player WHERE name IN ('{druid}','{druid2}','{vic}');")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")

announce_done("smoke_test_druid_batch2_tier2", host, port)
print("=== ALL CHECKS PASSED ===")
