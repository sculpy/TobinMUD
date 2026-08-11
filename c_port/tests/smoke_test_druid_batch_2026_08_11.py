#!/usr/bin/env python3
"""Smoke test for the 2026-08-11 Druid spell batch -- six Shaman spells
folded onto Druid (Tobin has no Shaman class), ported from real upstream
(disc_shaman_skunk.cc / disc_shaman_spider.cc / disc_shaman_healing.cc):

  healing grasp  -- restorative touch (heals its target)
  life leech     -- damage + 3/4 life-drain heal-back to the caster
  vampiric touch -- damage + full life-drain heal-back to the caster
  squish         -- crushing single-target damage
  boiling blood  -- heavy single-target damage
  coronary       -- heaviest single strike on the roster

An immortal Druid (whose casts resolve synchronously) casts each at a
mortal, huge-HP victim sharing its room: the five offensive spells each
drop the victim's HP and print their own message (never the generic
"nothing happens yet" stub); the two drains additionally report a
"+N HP" heal-back to the caster; and healing grasp raises the wounded
victim's HP back up. Same immortal-caster + mortal-target + load_room
scaffolding as smoke_test_offensive_spells.py.

    python3 tests/smoke_test_druid_batch_2026_08_11.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_druid_batch", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 974000 + (int(time.time()) % 20000)
COMPONENT = ROOM + 1


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    s.close()


def make_char_and_quit(name, pw, class_choice):
    """Create the character AND send a real `quit!` before closing -- a
    raw close() leaves a linkdead body whose room outranks load_room on
    the next connect, stranding a load_room-routed mortal target away
    from the sandbox (see smoke_test_offensive_spells.py's note)."""
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


druid = f"Drbat{_suffix}"
vic = f"Drvic{_suffix}"
pw = "drbatpw1234"

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Druid Batch Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

make_char(druid, pw, "5")            # class 5 = Druid (immortal, self-gotos)
make_char_and_quit(vic, pw, "3")     # class 3 = Warrior (load_room-routed target)

# Immortal Druid caster (level >= IMMORTAL_LEVEL_MIN=51 -> casts resolve
# synchronously; component gate bypassed), full discipline.
sql(f"UPDATE player_progress SET level=60,"
    f"basic_disc_pct=100,advanced_disc_pct=100,combat_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{druid}');")
# Victim: MORTAL, level 50 for a large HP pool, hp=999999 -> login clamps
# it down to the real level-based max (i.e. starts at full HP), enough to
# survive every strike in the batch.
sql(f"UPDATE player_progress SET level=50,hp=999999,max_hp=999999 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{vic}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic}';")

sv = login(vic, pw)
s = login(druid, pw)
check("Druid Batch Sandbox" in cmd(s, f"goto {ROOM}"), "the Druid reaches the sandbox room")
recv_all(sv)  # drain the Druid's arrival notice
check(vic.lower() in cmd(s, "look").lower(),
      "the victim shares the room (name targeting will resolve)")

cmd(s, "toggle pk")
cmd(sv, "toggle pk")


def cast(spell, target=None):
    cmd(s, f"load obj {COMPONENT}")
    line = f"cast {spell}" + (f" {target}" if target else "")
    return cmd(s, line, timeout=1.5)


# --- five offensive / drain spells drop the victim's HP synchronously ---
sigs = {
    "squish": "squish",
    "boiling blood": "blood boiling",
    "coronary": "heart",
    "life leech": "leech",
    "vampiric touch": "vampiric",
}
for spell in ("squish", "boiling blood", "coronary", "life leech", "vampiric touch"):
    before = hp_of(sv)
    out = cast(spell, vic)
    after = hp_of(sv)
    check(sigs[spell] in out.lower(),
          f"{spell} resolves with its own message, not the generic stub")
    check(after is not None and before is not None and after < before,
          f"{spell} actually damages the victim ({before} -> {after})")

# --- the two drains report a heal-back to the caster ---
for spell in ("life leech", "vampiric touch"):
    out = cast(spell, vic)
    check(re.search(r"\+\d+ HP", out) is not None,
          f"{spell} reports a life-drain heal-back to the caster")

# --- healing grasp restores the wounded victim ---
before = hp_of(sv)
out = cast("healing grasp", vic)
after = hp_of(sv)
check("healing grasp" in out.lower(),
      "healing grasp resolves with its own message, not the generic stub")
check(after is not None and before is not None and after > before,
      f"healing grasp actually restores the target's HP ({before} -> {after})")

# Cleanup.
send_line(s, "quit!"); recv_all(s)
send_line(sv, "quit!"); recv_all(sv)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{druid}','{vic}'));")
sql(f"DELETE FROM player WHERE name IN ('{druid}','{vic}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")

announce_done("smoke_test_druid_batch", host, port)
print("=== ALL CHECKS PASSED ===")
