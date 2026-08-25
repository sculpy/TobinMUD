#!/usr/bin/env python3
"""Smoke test for spell component charges / holy symbol decay (user
2026-07-18: "how long does each component last? should be getting 10
casts out of each component and the symbols should decay as in sneezy").

Uses MORTALS throughout: immortals now bypass the component/symbol
requirement entirely (NOHASSLE -- consume_component()/consume_symbol()
skip the immortal sentinel), so only a mortal actually spends charges.

  1. A fresh component (val[0]/val[1] seeded 10/10) survives exactly 10
     real `cast` attempts before being destroyed with a "used up" message
     (attempts refused with "You are still recovering!" -- the 1-3 round
     post-cast wait state, spellcast.c -- don't count; they're rejected
     before ever reaching the component gate); an 11th real attempt
     reports missing components again. Cast by a mortal Mage using a
     cheap spell (sorcerer's globe, a self-ward) whose real seeded bound
     reagent (vnum 242, session 197 binding fix) is used directly -- a
     fabricated generic component wouldn't be picked up without a
     restart, and this spell is no longer on the generic gate -- whose
     mana cost fits the Mage's natural pool across every attempt.
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
# Real seeded reagent (session 197 binding fix) bound to sorcerer's globe
# (val2=6, indexed at boot by spell_component_init()) -- a freshly-inserted
# fake component would NOT be picked up without a restart, and sorcerer's
# globe is no longer on the generic "any component-keyword item" gate, so
# the test must use a real bound reagent instead of fabricating one.
COMPONENT = 242  # "an air daemon's tail", type=30, val2=6 -> sorcerer's globe
SYMBOL = 900000 + (int(time.time() * 1000) % 60000)
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


# COMPONENT is a real seeded reagent (vnum 242) already bound to sorcerer's
# globe -- just reset its charges. SYMBOL stays a fabricated generic
# "symbol"-keyword item (type 12), since holy symbols are still on the old
# generic gate (find_holy_symbol() in cmd_pray.c), unaffected by the binding fix.
sql(f"UPDATE obj SET val0=10, val1=10 WHERE vnum={COMPONENT};")
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

SPELL = "sorcerer's globe"  # L1 self-ward, bound to vnum 242's reagent, cheap
# Casting now runs over a 1-3 round wait state (spellcast.c) -- a cast
# attempt sent before the previous one's wait clears is refused outright
# ("You are still recovering!") *before* ever reaching the component
# gate, so it must not count as one of the 10 real attempts. Poll past
# those instead of firing blind back-to-back casts.
used_up_on = None
attempts = 0
tries = 0
while attempts < 10 and tries < 40:
    tries += 1
    out = cmd(sm, f"cast {SPELL}")
    if "still recovering" in out:
        time.sleep(1)
        continue
    attempts += 1
    if "is used up" in out:
        used_up_on = attempts
        break
check(used_up_on == 10, f"the component survives exactly 10 mortal casts, not {used_up_on}")

out = cmd(sm, f"cast {SPELL}")
while "still recovering" in out:
    time.sleep(1)
    out = cmd(sm, f"cast {SPELL}")
check("You need an air daemon's tail to cast that" in out,
      "an 11th cast correctly finds no component left (named -- sorcerer's "
      "globe has a bound reagent, not the generic refusal)")
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
