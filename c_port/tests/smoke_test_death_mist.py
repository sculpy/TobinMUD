#!/usr/bin/env python3
"""Smoke test for the Druid `death mist` spell (Tier-5 port, 2026-08-16).

An immortal (level 60 -> casts reliably, deferred cast auto-succeeds)
Druid casts `death mist` in a private sandbox room with a mortal victim
(transferred in). Death mist is an AoE: it should infect the non-immortal,
un-grouped victim in the room with Syphilis (AFFECT_DISEASE_SYPHILIS).

Also guards the casting-flavor fix from the same session: an immortal
casting a Druid-only spell must see the FOREST (Druid) casting flavor, not
the Mage flavor -- the bug was that an immortal's class is CLASS_IMMORTAL,
not CLASS_DRUID, so flavor was wrongly picked from the caster's class
instead of the spell's.

Checks:
  * FLAVOR   -> the immortal Druid's cast shows forest flavor, not arcane.
  * MIST     -> at resolution the caster breathes the green mist and the
                victim is told it seeps into them.
  * INFECT   -> the victim ends up with the Syphilis affect.

    python3 tests/smoke_test_death_mist.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import finish_char_creation

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_death_mist", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
CASTER = f"Dmcaster{_suffix}"   # letters only -- character names reject digits
VICTIM = f"Dmvictim{_suffix}"
PW = "deathmist123"
ROOM = 977000 + (int(time.time()) % 18000)   # private sandbox vnum


def _connect_and_create(name, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in (name, "y", PW, PW):
        send_line(s, step); recv_all(s, 0.7)
    send_line(s, "new"); recv_all(s, 0.7)
    finish_char_creation(s, name, send_line, recv_all,
                         race="1", territory="1", char_class=cls)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, PW); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


# --- sandbox room -------------------------------------------------------
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Death Mist Sandbox','A sealed testing chamber.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

# --- chars: druid caster (bumped immortal), mortal victim ---------------
_connect_and_create(CASTER, "5")   # class set to Druid via SQL below
_connect_and_create(VICTIM, "5")
sql(f"UPDATE player SET class=4 WHERE name='{CASTER}';")   # CLASS_DRUID
sql(f"UPDATE player_progress SET level=60,basic_disc_pct=100,advanced_disc_pct=100,"
    f"combat_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{CASTER}');")

a = login(CASTER)
v = login(VICTIM)

# Pull both into the private room so no bystanders get misted.
cmd(a, f"goto {ROOM}")
cmd(a, f"transfer {VICTIM}")
recv_all(v, 0.8)

# --- FLAVOR (bug-fix guard) + cast --------------------------------------
# recv_all() reads until a silence gap, so this single read captures the
# whole multi-round cast AND its resolution (the rounds stream in bursts
# less than a gap apart).
out = cmd(a, "cast death mist", timeout=3.0).lower()
flavor_ok = ("wood" in out or "forest" in out
             or "green light" in out or "growing" in out)
check(flavor_ok,
      "an immortal casting a Druid-only spell sees the FOREST (Druid) flavor, not Mage")
check("harsh, guttural" not in out and "arcane" not in out,
      "the immortal Druid cast shows NO Mage-flavor casting lines")

# --- resolution: the caster breathes the mist; the victim is struck -----
check("green mist" in out and "your open mouth" in out,
      "at resolution the caster breathes out the chilling green mist")

victim_tail = recv_all(v, 1.5)
check("green mist seeps into you" in victim_tail.lower(),
      "the green mist reaches the victim")

aff = cmd(v, "affects")
check("syphilis" in aff.lower(),
      "the victim is infected with Syphilis (AFFECT_DISEASE_SYPHILIS)")

# --- cleanup ------------------------------------------------------------
send_line(a, "quit!"); recv_all(a)
send_line(v, "quit!"); recv_all(v)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{CASTER}','{VICTIM}'));")
sql(f"DELETE FROM player WHERE name IN ('{CASTER}','{VICTIM}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")

announce_done("smoke_test_death_mist", host, port)
print("\n=== smoke_test_death_mist PASSED ===")
