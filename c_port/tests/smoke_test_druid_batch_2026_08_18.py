#!/usr/bin/env python3
"""Smoke test for the Session-158-backlog Druid spell batch (2026-08-18):
sunscald (16), withering touch (32), wave crash (32), feral wrath (28),
leeching vine (48), tree walk (41). See cmd_cast.c.

An immortal Druid (casts resolve synchronously; component gate bypassed)
exercises each spell against a mortal, huge-HP Warrior victim in a
sandbox room. Checks each resolves with its OWN real effect, never the
generic "nothing happens yet" stub:

  * SUNSCALD       -> damages the victim (radiant lance);
  * WITHERING TOUCH-> damages the victim AND leaves them poisoned (decay);
  * WAVE CRASH     -> a room-wide area burst that catches the victim;
  * FERAL WRATH    -> a self buff, granting the caster the Blessed affect;
  * LEECHING VINE  -> damages the victim (life-drain);
  * TREE WALK      -> self random teleport out of the room (done last).

    python3 tests/smoke_test_druid_batch_2026_08_18.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import finish_char_creation

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_druid_batch_2026_08_18", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 978000 + (int(time.time()) % 17000)
COMPONENT = ROOM + 2


def _connect_and_create(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in (name, "y", pw, pw):
        send_line(s, step); recv_all(s, 0.7)
    send_line(s, "new"); recv_all(s, 0.7)
    finish_char_creation(s, name, send_line, recv_all,
                         race="1", territory="1", char_class=class_choice)
    return s


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


druid = f"Dedru{_suffix}"
vic = f"Devic{_suffix}"
pw = "dedrupw1234"

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Druid 08-18 Sandbox','A bare field.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

make_char_and_quit(druid, pw, "5")   # Druid
make_char_and_quit(vic, pw, "3")     # Warrior victim

sql(f"UPDATE player_progress SET level=60,basic_disc_pct=100,advanced_disc_pct=100,"
    f"combat_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{druid}');")
sql(f"UPDATE player_progress SET level=50,hp=999999,max_hp=999999 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic}';")

sv = login(vic, pw)
s = login(druid, pw)
check("Druid 08-18 Sandbox" in cmd(s, f"goto {ROOM}"), "the Druid reaches the sandbox room")
recv_all(sv)
check(vic.lower() in cmd(s, "look").lower(), "the victim shares the room")

cmd(s, "toggle pk")
cmd(sv, "toggle pk")


def cast(spell):
    cmd(s, f"load obj {COMPONENT}")
    return cmd(s, f"cast {spell}", timeout=1.5)


# --- SUNSCALD: single-target radiant damage ---
recv_all(sv)
before = hp_of(sv)
out = cast(f"sunscald {vic}")
after = hp_of(sv)
check("nothing happens yet" not in out.lower(), "sunscald is not the generic stub")
check("sunlight" in out.lower() or "scald" in out.lower(), f"sunscald resolves with its own message: {out[:80]!r}")
check(before is not None and after is not None and after < before, f"sunscald damages the victim ({before} -> {after})")

# --- WITHERING TOUCH: damage + poison DoT ---
recv_all(sv)
before = hp_of(sv)
out = cast(f"withering touch {vic}")
after = hp_of(sv)
check("nothing happens yet" not in out.lower(), "withering touch is not the generic stub")
check("wither" in out.lower(), f"withering touch resolves with its own message: {out[:80]!r}")
check(after is not None and before is not None and after < before, "withering touch damages the victim")
check("poison" in cmd(sv, "affects").lower(), "withering touch leaves the victim poisoned")

# --- WAVE CRASH: area burst ---
recv_all(sv)
before = hp_of(sv)
out = cast("wave crash")
after = hp_of(sv)
check("nothing happens yet" not in out.lower(), "wave crash is not the generic stub")
check("unleash" in out.lower(), f"wave crash resolves as an area burst: {out[:80]!r}")
check(after is not None and before is not None and after < before, "wave crash catches the victim in the room")

# --- FERAL WRATH: self buff (Blessed affect) ---
out = cast("feral wrath")
check("nothing happens yet" not in out.lower(), "feral wrath is not the generic stub")
check("feral wrath" in out.lower(), f"feral wrath resolves with its own message: {out[:80]!r}")
check("blessed" in cmd(s, "affects").lower(), "feral wrath grants the caster an offensive buff (Blessed)")

# --- LEECHING VINE: life-drain damage ---
recv_all(sv)
before = hp_of(sv)
out = cast(f"leeching vine {vic}")
after = hp_of(sv)
check("nothing happens yet" not in out.lower(), "leeching vine is not the generic stub")
check("leeching vine" in out.lower(), f"leeching vine resolves with its own message: {out[:80]!r}")
check(after is not None and before is not None and after < before, "leeching vine drains the victim")

# --- TREE WALK: self random teleport (done last -- it moves the caster) ---
out = cast("tree walk")
check("nothing happens yet" not in out.lower(), "tree walk is not the generic stub")
check("tree" in out.lower() and "bark" in out.lower(), f"tree walk resolves with its own message: {out[:80]!r}")
check("Druid 08-18 Sandbox" not in cmd(s, "look"), "tree walk moved the Druid out of the sandbox room")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
send_line(sv, "quit!"); recv_all(sv)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{druid}','{vic}'));")
sql(f"DELETE FROM player WHERE name IN ('{druid}','{vic}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")

announce_done("smoke_test_druid_batch_2026_08_18", host, port)
print("=== ALL CHECKS PASSED ===")
