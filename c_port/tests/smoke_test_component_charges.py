#!/usr/bin/env python3
"""Smoke test for spell component charges / holy symbol decay (user
2026-07-18: "how long does each component last? should be getting 10
casts out of each component and the symbols should decay as in sneezy").

Uses MORTALS throughout: immortals now bypass the component/symbol
requirement entirely (NOHASSLE -- consume_component()/consume_symbol()
skip the immortal sentinel), so only a mortal actually spends charges.

  1. A fresh component (val[0]/val[1] seeded 10/10) survives exactly 10
     `cast` attempts before being destroyed with a "used up" message; an
     11th attempt reports missing components again. Cast by a mortal Mage
     using a cheap unbound spell (sorcerer's globe, a self-ward with no
     bound reagent, so the generic test component satisfies it) whose
     mana cost fits the Mage's natural pool across all 11 attempts.
  2. A fresh holy symbol survives more than one `pray` attempt (real
     decay, 1-2 strength lost per use out of 10) and eventually shatters,
     then a further prayer reports the missing symbol. Prayed by a mortal
     Cleric (no mana gate applies to Cleric prayers).

    python3 tests/smoke_test_component_charges.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_component_charges", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
COMPONENT = 900000 + (int(time.time() * 1000) % 60000)
SYMBOL = COMPONENT + 1
pw = "chgpw123"


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


def clear_and_seed(name, vnum):
    """Wipe the char's starting gear (so no other component/symbol is
    carried) and give them exactly one of `vnum`. A mortal can't `load`,
    so the item is placed straight into player_inventory; it reloads from
    its prototype (with the seeded charges) on next login."""
    pid = f"(SELECT id FROM player WHERE name='{name}')"
    sql(f"DELETE FROM player_inventory WHERE player_id={pid};")
    sql(f"INSERT INTO player_inventory (player_id, vnum, slot) VALUES ({pid}, {vnum}, -1);")


# Prototypes: a generic "component"-keyword reagent and a "symbol"-keyword
# holy symbol, both type 12 (OTHER) with 10/10 charges.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1,10,10) "
    f"ON DUPLICATE KEY UPDATE val0=10,val1=10;")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1,10,10) "
    f"ON DUPLICATE KEY UPDATE val0=10,val1=10;")

# --- 1: a component lasts exactly 10 mortal cast attempts, then is destroyed ---
mage = f"Chgmag{_sfx}"
make_char(mage, "1")  # class 1 = Mage
clear_and_seed(mage, COMPONENT)
sql("UPDATE player_progress SET level=20, basic_disc_pct=100, mana=100, max_mana=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{mage}');")
sm = login(mage)

SPELL = "sorcerer's globe"  # L1 self-ward, unbound (generic component satisfies it), cheap
used_up_on = None
for i in range(1, 12):
    out = cmd(sm, f"cast {SPELL}")
    if "is used up" in out:
        used_up_on = i
        break
check(used_up_on == 10, f"the component survives exactly 10 mortal casts, not {used_up_on}")

out = cmd(sm, f"cast {SPELL}")
check("don't have the spell components" in out, "an 11th cast correctly finds no component left")
sm.close()

# --- 2: a holy symbol decays over several mortal prayers, then shatters ---
cler = f"Chgcle{_sfx}"
make_char(cler, "2")  # class 2 = Cleric
clear_and_seed(cler, SYMBOL)
sql("UPDATE player_progress SET level=20, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cler}');")
sc = login(cler)

shattered_on = None
for i in range(1, 16):
    out = cmd(sc, "pray armor")
    if "shatters from the stress" in out:
        shattered_on = i
        break
check(shattered_on is not None, "the symbol eventually shatters")
check(shattered_on > 1, f"the symbol survives more than one prayer before shattering (#{shattered_on})")
check(shattered_on <= 10, f"the symbol doesn't survive an implausible number of prayers ({shattered_on})")

out = cmd(sc, "pray armor")
check("need a holy symbol" in out, "praying again correctly finds no symbol left")
sc.close()

announce_done("smoke_test_component_charges", host, port)
print("=== ALL CHECKS PASSED ===")
