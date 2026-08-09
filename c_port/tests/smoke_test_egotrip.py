#!/usr/bin/env python3
"""Smoke test for egotrip's 2026-08-08 expansion (user: "fully implement
egotrip from sneezy") and the new `force` command (user, same day: "need
a force command thats 55+ that force <target> [command] mobs or players
can be forced").

egotrip previously only had `blast` implemented (2026-07-12) -- the other
12 Sneezy subcommands were disclosed as skipped because their underlying
systems (disease, garble, portal objects, mob hate/aggro, a numbered
crit-effect table) didn't exist in Tobin. Since then a real disease
system landed -- this expansion ports every subcommand that now maps
onto something real: disease, cleanse, stupidity, wander, and crit
(reusing Tobin's own "limb hits 0% HP" crit mechanic instead of Sneezy's
missing numbered table). deity/bless/portal/hate/garble stay unported
(still no equivalent system); teleport stays unported because the
separate, pre-existing `transfer` command already covers it.

Covers:
  1. `egotrip disease <target> <cold-ish name>` inflicts the named
     AFFECT_DISEASE_* on the target.
  2. `egotrip cleanse` cures it (and reports a nonzero cured count).
  3. `egotrip stupidity` applies AFFECT_STUPIDITY to a connected mortal.
  4. `egotrip crit <target>` severs a limb without killing them (HP
     unaffected) and without requiring combat.
  5. `egotrip wander` runs without error (0 mobs in an empty sandbox
     room is a valid, non-crashing outcome).
  6. `force <target> <command>` makes the target actually run the
     command -- their own output reaches THEIR screen.
  7. `force` refuses to target another immortal ranked >= the caller.
  8. `help egotrip` and `help force` both exist and read correctly.

    python3 tests/smoke_test_egotrip.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
MOB_VNUM = 960000 + (int(time.time()) % 30000)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    return s


announce("smoke_test_egotrip", host, port)

imm_name, imm_pw = f"Egoimm{_suffix}", "egoimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

vic_name, vic_pw = f"Egovic{_suffix}", "egovicpw12345"
sv = make_char(vic_name, vic_pw)

# --- 8: help topics ---
out = strip(cmd(si, "help egotrip", timeout=1.5))
check("blast <target>" in out and "disease <target>" in out, "`help egotrip` documents the expanded subcommand set")
out = strip(cmd(si, "help force", timeout=1.5))
check("force <target> <command>" in out.lower() or "Usage: force" in out, "`help force` exists and reads correctly")

# --- 1: disease ---
out = strip(cmd(si, f"egotrip disease {vic_name} scurvy", timeout=1.5))
check("breathe a fetid cloud" in out, "`egotrip disease` reports inflicting the disease")
out = strip(cmd(sv, "affects", timeout=1.0))
check("Scurvy" in out, "the target actually carries the named disease affect")

# --- 2: cleanse ---
out = strip(cmd(si, "egotrip cleanse", timeout=1.5))
m = re.search(r"cleanse the world of disease -- (\d+) affliction", out)
check(m is not None and int(m.group(1)) >= 1, "`egotrip cleanse` reports curing at least one affliction")
out = strip(cmd(sv, "affects", timeout=1.0))
check("(none)" in out, "cleanse actually removed the disease from the target's active-affects list")

# --- 3: stupidity ---
out = strip(cmd(si, "egotrip stupidity", timeout=1.5))
check("suspicions" in out, "`egotrip stupidity` reports casting on connected mortals")
out = strip(cmd(sv, "affects", timeout=1.0))
check("Stupidity" in out, "the target actually carries AFFECT_STUPIDITY")

# --- 4: crit doesn't kill, works outside combat ---
# (relative HP comparison, not an absolute value -- score's own HP/max_hp
# are recomputed from class/race/level at every login, so a raw SQL
# override doesn't survive a relog; reading current HP on the SAME live
# connection before/after is what actually proves crit leaves it alone)
out = strip(cmd(sv, "score", timeout=1.0))
m = re.search(r"HP:\s+(\d+)/(\d+)", out)
check(m is not None, "score output includes a parseable HP line")
hp_before = int(m.group(1))
out = strip(cmd(si, f"egotrip crit {vic_name}", timeout=1.5))
check("bad luck has befallen" in out, "`egotrip crit` reports success")
out = strip(cmd(sv, "score", timeout=1.0))
m = re.search(r"HP:\s+(\d+)/(\d+)", out)
check(m is not None and int(m.group(1)) == hp_before, "crit severs a limb without touching the target's HP (no combat needed)")

# --- 5: wander doesn't crash ---
out = strip(cmd(si, "egotrip wander", timeout=1.5))
check("wander pulse" in out, "`egotrip wander` runs without error")

# --- 6: force actually runs the command as the target ---
out = strip(cmd(si, f"force {vic_name} score", timeout=1.5))
check(f"force {vic_name}".lower() in out.lower(), "`force` confirms back to the immortal")
victim_saw = strip(recv_all(sv, 1.0))
check("Name:" in victim_saw and vic_name in victim_saw, "the forced command's own output reached the TARGET's screen")

# --- 7: force can't hit an equal/higher immortal ---
imm2_name, imm2_pw = f"Egoimb{_suffix}", "egoimbpw12345"
si2 = make_char(imm2_name, imm2_pw)
cmd(si2, "quit!"); si2.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm2_name}');")
si2 = relog(imm2_name, imm2_pw)
out = strip(cmd(si, f"force {imm2_name} score", timeout=1.5))
check("shouldn't do that" in out.lower(), "`force` refuses an equal-ranked immortal target")
si2.close()

# --- 9: damn costs the victim zero XP (2026-08-08, user: "bypass xp
# loss on an egotrip hit") -- a real kill (corpse, half-heal, "you have
# been slain" message) but the XP-loss branch inside combat_defeat() is
# conditioned on the winner NOT being a PC, and an immortal always is
# one, so nothing should be deducted. ---
xp_before = int(query(f"SELECT experience FROM player_progress WHERE player_id="
                       f"(SELECT id FROM player WHERE name='{vic_name}');"))
out = strip(cmd(si, f"egotrip damn {vic_name}", timeout=1.5))
check("word of damnation" in out, "`egotrip damn` reports success")
victim_saw = strip(recv_all(sv, 1.0))
check("DAMNED" in victim_saw and "slain" in victim_saw.lower(), "the target sees the damnation + death message")
check("lose" not in victim_saw.lower() or "experience" not in victim_saw.lower(),
      "no 'you lose N experience' line reaches the target")
xp_after = int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{vic_name}');"))
check(xp_after == xp_before, f"damn cost zero XP ({xp_before} -> {xp_after})")

# --- 10: damn refuses an immortal target ---
out = strip(cmd(si, f"egotrip damn {imm_name}", timeout=1.5))
check("damn yourself" in out.lower(), "`egotrip damn` refuses self-targeting")

# --- 11: mob targeting (2026-08-08, user: "make egotrip usable on mobs
# too") -- blast/damn/disease previously only scanned g_descriptors
# (PCs), so a mob name always fell through to 'No one named'. Spawn a
# disposable mob in the immortal's own room and confirm each subcommand
# now reaches it via the combat_find_room_target() room-fallback. ---
mob_tag = f"egomob{_suffix}"
_mob_cols = {
    "vnum": MOB_VNUM, "name": f"'{mob_tag}'", "short_desc": f"'a {mob_tag}'",
    "long_desc": f"'A {mob_tag} stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": -1.7,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 100,
}
_col_names = ",".join(_mob_cols.keys())
_col_values = ",".join(str(v) for v in _mob_cols.values())
sql(f"INSERT INTO mob ({_col_names}) VALUES ({_col_values});")
out = strip(cmd(si, f"load mob {MOB_VNUM}", timeout=1.5))
check("You conjure" in out, "the disposable test mob is loaded into the immortal's room")

out = strip(cmd(si, f"egotrip blast {mob_tag}", timeout=1.5))
check("no one named" not in out.lower(), "`egotrip blast` now reaches a mob in the room")

out = strip(cmd(si, f"egotrip disease {mob_tag} cold", timeout=1.5))
check("no one named" not in out.lower(), "`egotrip disease` now reaches a mob in the room")

out = strip(cmd(si, f"egotrip damn {mob_tag}", timeout=1.5))
check("no one named" not in out.lower(), "`egotrip damn` now reaches a mob in the room")
out = strip(cmd(si, "look", timeout=1.0))
check(f"{mob_tag} stands here" not in out, "damn actually destroyed the mob (no longer listed as a living being)")
check("corpse of a " + mob_tag in out, "damn left a real, lootable corpse behind, same as any other kill")

sv.close()
si.close()

announce_done("smoke_test_egotrip", host, port)
print("=== ALL CHECKS PASSED ===")
