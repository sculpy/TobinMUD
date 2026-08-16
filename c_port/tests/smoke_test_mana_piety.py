#!/usr/bin/env python3
"""Smoke test for the mana/piety gain-and-recovery port (real SneezyMUD
manaGain()/pietyGain(), being.c/regen.c) plus the Cleric piety pool.

  1. A Cleric has a Piety pool shown in `score`, starting full at 100.
  2. `pray` spends piety (the Cleric's casting resource); an empty pool
     refuses with "lack the piety".
  3. A Monk now has a real Mana pool (100 + level*3).
  4. Mana recovers on the ~36s mana/piety tick (Mage), proving the regen
     hook fires.

    python3 tests/smoke_test_mana_piety.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_sfx = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step); recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=cls)
    cmd(s, "color off")
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    return s


def pool(sock, label):
    out = cmd(sock, "score")
    m = re.search(label + r":\s*(\d+)\s*/\s*(\d+)", out)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


announce("smoke_test_mana_piety", host, port)
pw = "manapietypw123"

# --- 1 & 2: Cleric piety pool, spend, and empty-pool refusal ---
cle = f"Clpty{_sfx}"
make_char(cle, pw, "2")  # Cleric
sql("UPDATE player_progress SET level=10, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cle}');")
s = relog(cle, pw)
cur, mx = pool(s, "Piety")
check(mx == 100, f"a Cleric's score shows a Piety pool with max 100 (got {mx})")
check(cur == 100, f"piety starts full at 100 (got {cur})")

out = cmd(s, "pray armor")
check("lack the piety" not in out.lower(), "a Cleric with full piety can pray")
cur2, _ = pool(s, "Piety")
check(cur2 < 100, f"praying spent some piety (100 -> {cur2})")
# quit! saves synchronously (avoids a disconnect-save race with the SQL
# below) but drops carried gear, so re-grant a holy symbol before relog.
cmd(s, "quit!"); s.close()

# empty the pool -> refusal
sql(f"UPDATE player_progress SET piety=0 WHERE player_id=(SELECT id FROM player WHERE name='{cle}');")
sql("INSERT INTO player_inventory (player_id, vnum, slot) VALUES "
    f"((SELECT id FROM player WHERE name='{cle}'), 500, -1);")  # wooden holy symbol
s = relog(cle, pw)
out = cmd(s, "pray armor")
check("lack the piety" in out.lower(), f"an empty piety pool refuses the prayer (got: {out.strip()[:60]!r})")
s.close()

# --- 3: Monk now has a real mana pool (100 + level*3) ---
mon = f"Mnman{_sfx}"
make_char(mon, pw, "6")  # Monk
sql("UPDATE player_progress SET level=10 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{mon}');")
# recompute max_mana off the new level: relog reloads and recomputes it.
sm = relog(mon, pw)
mcur, mmax = pool(sm, "Mana")
check(mmax == 100 + 10 * 3, f"a level-10 Monk's Mana pool is 100 + level*3 = 130 (got {mmax})")
sm.close()

# --- 4: mana recovers on the ~36s tick (Mage) ---
mag = f"Mgreg{_sfx}"
make_char(mag, pw, "1")  # Mage
s = relog(mag, pw)
_, magmax = pool(s, "Mana")
sql(f"UPDATE player_progress SET mana=1 WHERE player_id=(SELECT id FROM player WHERE name='{mag}');")
s.close()
s = relog(mag, pw)
low, _ = pool(s, "Mana")
check(low == 1, f"mana set low for the regen check (got {low})")
time.sleep(40)  # one MANA_REGEN_PULSES (~36s) tick
after, _ = pool(s, "Mana")
check(after > low, f"mana recovered on the ~36s tick ({low} -> {after})")
s.close()

# cleanup
for nm in (cle, mon, mag):
    sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player WHERE name='{nm}';")

announce_done("smoke_test_mana_piety", host, port)
print("=== ALL CHECKS PASSED ===")
