#!/usr/bin/env python3
"""Smoke test for the Tier-3 survival/utility skills (2026-08-16):
fishing/fishlore (`fish`), seekwater, encamp, divine.

An immortal (level 60 -> skill gates bypassed, every proficiency roll
auto-succeeds) exercises each skill in a sandbox: an outdoor PLAINS room
(sector 17, plantable land) with a north exit into a TEMPERATE OCEAN room
(sector 25, water). One land room adjacent to water satisfies all four:
`fish`/`seekwater` see the water next door, `encamp`/`divine` need the
plantable land underfoot. Checks:

  * FISH      -> lands a fish from the adjacent water; a second immediate
                 cast is refused by the AFFECT_FISH_COOLDOWN throttle.
  * SEEKWATER -> senses the water to the north (the directional branch).
  * ENCAMP    -> pitches a camp (AFFECT_ENCAMP shows in `affects`); a
                 second encamp is refused (already camped).
  * DIVINE    -> dowses water into a carried, empty drink container.

    python3 tests/smoke_test_tier3_survival.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import finish_char_creation

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_tier3_survival", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
LAND = 976000 + (int(time.time()) % 18000)   # outdoor PLAINS (plantable)
WATER = LAND + 1                              # TEMPERATE OCEAN (water)
DRINK = LAND + 2                              # empty drink-container obj vnum


def _connect_and_create(name, pw, class_choice):
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


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


who = f"Tsurv{_suffix}"   # letters only -- character names reject digits
pw = "t3surpw1234"

# --- sandbox rooms + adjacency ------------------------------------------
sql(f"DELETE FROM roomexit WHERE vnum IN ({LAND},{WATER});")
sql(f"DELETE FROM room WHERE vnum IN ({LAND},{WATER});")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({LAND},0,0,0,'Tier3 Survival Land','A grassy plain.\\n',NULL,1,17,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({WATER},0,0,0,'Tier3 Survival Water','Open ocean.\\n',NULL,1,25,0,0,0,0,0,0,0,0);")
# North (direction 0) from the land room into the water room.
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,"
    f"condition_flag,lock_difficulty,weight,key_num,destination) "
    f"VALUES ({LAND},0,'','',0,0,-1,-1,-1,{WATER});")

# --- an empty drink container (ITEM_DRINKCON=17), 10 units max ----------
sql(f"DELETE FROM obj WHERE vnum={DRINK};")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,val0,val1,val2,"
    f"wear_flag,can_be_seen) "
    f"VALUES ({DRINK},'waterskin flask','a waterskin','A waterskin lies here.',"
    f"17,10,0,0,1,1);")

make_char(who, pw, "5")
sql(f"UPDATE player_progress SET level=60,"
    f"basic_disc_pct=100,advanced_disc_pct=100,combat_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{who}');")

s = login(who, pw)
check("Tier3 Survival Land" in cmd(s, f"goto {LAND}"),
      "the tester reaches the outdoor land sandbox room")

# --- FISH: land a catch from the adjacent water -------------------------
out = cmd(s, "fish", timeout=1.5)
check(("fish" in out.lower() or "prize" in out.lower()) and "line" in out.lower(),
      "fish lands a catch from the water next door")
check("no water" not in out.lower(),
      "fish is NOT refused for lack of water (adjacency detected)")

# --- FISH again: the spot is fished out (cooldown) ----------------------
out2 = cmd(s, "fish", timeout=1.5)
check("fished" in out2.lower() and "out" in out2.lower(),
      "a second immediate fish is refused by the cooldown throttle")

# --- SEEKWATER: sense the water to the north ----------------------------
out = cmd(s, "seekwater", timeout=1.5)
check("water" in out.lower() and "north" in out.lower(),
      "seekwater senses the water to the north")

# --- ENCAMP: pitch camp, gain the AFFECT_ENCAMP buff --------------------
out = cmd(s, "encamp", timeout=1.5)
check("camp" in out.lower(),
      "encamp sets up a camp on plantable land")
aff = cmd(s, "affects")
check("encamped" in aff.lower(),
      "encamp grants the Encamped regen affect")
out = cmd(s, "encamp", timeout=1.5)
check("already" in out.lower(),
      "a second encamp is refused (already have a camp)")

# --- DIVINE: dowse water into a carried empty container -----------------
cmd(s, f"load obj {DRINK}")
cmd(s, "get waterskin")
out = cmd(s, "divine waterskin", timeout=1.5)
check("divine" in out.lower() and "ounce" in out.lower(),
      "divine dowses water into the carried drink container")
check("nature" not in out.lower(),
      "divine is NOT refused for terrain (land is plantable)")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name='{who}');")
sql(f"DELETE FROM player WHERE name='{who}';")
sql(f"DELETE FROM roomexit WHERE vnum IN ({LAND},{WATER});")
sql(f"DELETE FROM room WHERE vnum IN ({LAND},{WATER});")
sql(f"DELETE FROM obj WHERE vnum={DRINK};")

announce_done("smoke_test_tier3_survival", host, port)
print("=== ALL CHECKS PASSED ===")
