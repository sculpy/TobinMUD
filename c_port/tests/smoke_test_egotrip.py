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
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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

sv.close()
si.close()

announce_done("smoke_test_egotrip", host, port)
print("=== ALL CHECKS PASSED ===")
