#!/usr/bin/env python3
"""Smoke test for casting concentration break (user 2026-08-10, Sneezy
spelltask parity -- spellcast.c/spellcast_distract() + being.h's
cast_distracted).

A disruptive maneuver (bash/kick/trip/grapple) landed on a caster while
their multi-round `cast` is still forming adds to a distraction counter;
spellcast_tick_run() rolls it each round and shatters the spell if the
distraction is high enough -- upstream's own `(2*distract) >= 1d20`
check, softened by Wisdom (upstream ties concentration to STAT_WIS, not
the wizardry skill). Plain melee never distracts.

Determinism: bash lags the attacker 2 rounds, so one attacker only lands
~one disruption per round (distraction 2 -> ~20% break). Six Warriors
(re-bashing each round) pile distraction quickly -> a round where enough
shields connect gives effective >=20 vs a 1d20 roll -> a certain break,
with a default-Wisdom (120) caster contributing 0 mitigation. Attackers
are Warriors: bash is Warrior-gated (being_knows_skill), needs a wielded
shield, and needs real bash proficiency to LAND (an unpracticed bash just
whiffs and never distracts), so each is set up with all three. All four
share a room via PK opt-in (PLR_PK_OPTIN) so mortal-vs-mortal combat is
allowed.

Covers:
  1. A Mage mid-cast, bashed by six attackers in one round, has the spell
     BROKEN ('distraction is too much') before it resolves.
  2. The real effect ('shimmering dome of protection', sorcerer's globe
     landing) does NOT appear -- cancelled, not merely delayed.

    python3 tests/smoke_test_spell_concentration_break.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_spell_concentration_break", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26 ** i) % 26) for i in range(4))
ROOM = 905000 + (int(time.time()) % 40000)
COMPONENT = ROOM + 1
SHIELD = 178  # 'shield wooden small simple' -- a real armor-slot shield

caster_name, pw = f"Ccbrk{_suffix}", "concbreakpw1"
imm_name = f"Ccimm{_suffix}"
# NOTE: char names must be LETTERS ONLY -- index with a letter, never a digit.
atk_names = [f"Ccatk{chr(ord('a') + i)}{_suffix}" for i in range(6)]


def create_char(name, cls, quit_after=True):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "y", ""):
        send_line(s, step); recv_all(s)
    send_line(s, ""); recv_all(s)  # skip email
    for step in ("new", name, "1", "1", cls, "done", "done"):
        send_line(s, step); recv_all(s)
    if quit_after:
        send_line(s, "quit!"); recv_all(s); s.close(); return None
    return s


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    return s


pid = lambda n: f"(SELECT id FROM player WHERE name='{n}')"

# pre-clean any stragglers from an aborted run of this test
sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Cc%{_suffix}');")

create_char(caster_name, "1")   # Mage caster
create_char(imm_name, "1")      # immortal helper (loads component + shields)
for a in atk_names:
    create_char(a, "3")         # Warrior attackers (Warriors know bash)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Concentration Break Sandbox','A bare sandbox room.\n',"
    f"NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

# caster: mortal Mage past every cast gate, huge HP so it survives the fight
sql(f"UPDATE player_progress SET level=50, basic_disc_pct=100, combat_disc_pct=100, "
    f"advanced_disc_pct=100, mana=9999, max_mana=9999, hp=999999, max_hp=999999 "
    f"WHERE player_id={pid(caster_name)};")
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"VALUES ({pid(caster_name)}, 'sorcerer''s globe', 100, 0);")
sql(f"UPDATE player SET load_room={ROOM}, pflags=pflags|16 WHERE name='{caster_name}';")  # 16=PLR_PK_OPTIN

sql(f"UPDATE player_progress SET level=51 WHERE player_id={pid(imm_name)};")

for a in atk_names:
    sql(f"UPDATE player_progress SET level=30, basic_disc_pct=100, combat_disc_pct=100, "
        f"hp=5000, max_hp=5000 WHERE player_id={pid(a)};")
    sql(f"UPDATE player SET load_room={ROOM}, pflags=pflags|16 WHERE name='{a}';")  # PK opt-in
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ({pid(a)}, 'bash', 100, 0);")  # so bash actually lands (proficiency roll)

caster = login(caster_name)
imm = login(imm_name)
atks = [login(a) for a in atk_names]

check("Concentration Break Sandbox" in cmd(imm, f"goto {ROOM}"), "immortal helper reaches the sandbox room")

# hand the caster its spell component
cmd(imm, f"load obj {COMPONENT}"); cmd(imm, "drop pouch")
check("you get" in cmd(caster, "get pouch").lower(), "caster picks up the spell component")

# arm each attacker with a shield so bash is legal
for s in atks:
    cmd(imm, f"load obj {SHIELD}"); cmd(imm, "drop shield")
    cmd(s, "get shield"); cmd(s, "hold shield")

for s in [caster, imm] + atks:
    recv_all(s)

# engage first, let open-attack lag clear so the follow-up bash lands at once
for s in atks:
    cmd(s, f"kill {caster_name}")
time.sleep(3.0)
for s in [caster] + atks:
    recv_all(s)

# caster starts the multi-round cast; six shields slam in the same round
out = cmd(caster, "cast sorcerer's globe", timeout=0.6)
# Re-bash every round: bash lags each attacker 2 rounds, so a single
# round won't have all six land -- but re-issuing each round guarantees a
# round where enough shields connect (distraction >= 10 -> a certain
# break), while any recover only EXTENDS the cast, keeping it alive to be
# broken. A round where all six land piles distraction to 12 (effective
# 24 vs 1d20).
broke = out
for _ in range(10):
    for s in atks:
        send_line(s, "bash")
    broke += recv_all(caster, timeout=1.2)
    if "distraction is too much" in broke:
        break

check("distraction is too much" in broke,
      "a Mage bashed by six attackers mid-cast has the spell broken")
check("shimmering dome of protection" not in broke,
      "the broken spell's real effect never lands (cancelled, not delayed)")

for s in [caster, imm] + atks:
    try: s.close()
    except Exception: pass

sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Cc%{_suffix}');")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")
sql(f"DELETE FROM room WHERE vnum={ROOM};")

print("ALL CHECKS PASSED")
announce_done("smoke_test_spell_concentration_break", host, port)
