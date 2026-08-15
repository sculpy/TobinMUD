#!/usr/bin/env python3
"""Smoke test for the Know-X monster-lore cluster (`know` command +
cmd_know.c / mob_lore.c, spell/skill audit "Generic / cross-class").

An immortal parks in a quiet room and loads three mobs of known race
kingdoms -- a cat (race FELINE -> animal), an ice dragon (DRAGON ->
reptile), and a vrock (DEMON). Then:

  1. `know` auto-selects the lore field from the target's race: the cat
     reads as an animal, the dragon as reptile/dragonkind, the vrock as a
     demon -- and the immortal (full mastery) sees the graded reveal
     ladder (Vitality/Defenses/Disposition).
  2. A mortal of the WRONG class (Mage) is refused, and the refusal names
     the correct auto-selected skill for that creature ("know animal" for
     the cat) -- proving the category dispatch runs even on the deny path.
  3. A Druid (who gets all eight lore skills as class skills) studies the
     same cat successfully and reads its race.

Mortals can't goto/load, so the immortal loads the mobs and `transfer`s
the mortals into the room.

    python3 tests/smoke_test_know_lore.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_know_lore", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "knowpw12"
ROOM = 1200


def make_char(name, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in [name, "y", pw, pw, "new", name, "1", "1", char_class, "done", "done"]:
        send_line(s, step); recv_all(s, 0.7)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


# --- immortal sets the scene ---
imm = f"Knim{_sfx}"
make_char(imm, "1")
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
si = login(imm)
cmd(si, f"goto {ROOM}")
cmd(si, "purge")
cmd(si, "load mob 104")    # a small cat -- FELINE (animal)
cmd(si, "load mob 2107")   # Kylish'Ra, the ice dragon -- DRAGON (reptile)
cmd(si, "load mob 10")     # a vrock demon -- DEMON

# 1. auto-select by race + immortal full reveal ladder
out = cmd(si, "know cat")
check("it is a feline" in out.lower() and "knowledge of animals" in out.lower(),
      "know on a cat auto-selects animal lore and reads its race (feline)")
check("vitality:" in out.lower() and "defenses:" in out.lower()
      and "disposition:" in out.lower(),
      "an immortal (full mastery) sees the whole graded reveal ladder")
out = cmd(si, "know dragon")
check("dragonkind" in out.lower() and "it is a dragon" in out.lower(),
      "know on the ice dragon auto-selects reptile/dragonkind lore")
out = cmd(si, "know vrock")
check("demons and aberrations" in out.lower(),
      "know on the vrock auto-selects demon lore")

# 2. wrong-class mortal is refused, refusal names the right skill
mage = f"Knmg{_sfx}"
make_char(mage, "1")  # Mage -- knows no lore skills
sm = login(mage)
cmd(si, f"transfer {mage} {ROOM}")
out = cmd(sm, "know cat")
check("know animal" in out.lower() and "nothing of animals" in out.lower(),
      "a Mage is refused and the refusal names the auto-selected 'know animal' skill")
sm.close()

# 3. a Druid (all eight lore skills are class skills) succeeds
dru = f"Kndr{_sfx}"
make_char(dru, "5")  # Druid
sql("UPDATE player_progress SET level=10, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sd = login(dru)
cmd(si, f"transfer {dru} {ROOM}")
out = cmd(sd, "know cat")
check("it is a feline" in out.lower() and "nothing of animals" not in out.lower(),
      "a Druid knows animal lore and reads the cat's race")
sd.close()

cmd(si, "purge")
si.close()

announce_done("smoke_test_know_lore", host, port)
print("PASS: smoke_test_know_lore")
