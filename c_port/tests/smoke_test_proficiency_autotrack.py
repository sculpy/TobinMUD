#!/usr/bin/env python3
"""Smoke test for the Combat-discipline proficiency skills auto-tracking
discipline (user, 2026-08-03, across several clarifying messages: "the
proficiency skills in combat disciplines should be gained at 1% of the
disc" / "for all classes and all proficiencies" / "they should get all
proficiency skills for spending 1% of the combat practices" / "they
should gain in proficiency automatically, when combat hits 100% they
should be able to increase proficiency to 100%, but they should start
from level 1").

Root problem: the 5 weapon/barehand proficiency skills (slash/blunt/
pierce/barehand/ranged proficiency) had no gameplay hook anywhere that
ever called skill_learn_from_doing() on them (Tobin's obj.h has no
weapon-type distinction to gate a per-swing roll on) -- under the old
learn-by-doing system they'd sit permanently stuck at their 1% floor
forever. Also, only Cleric/Mage/Druid had these 5 skills in their
roster at all; Warrior/Thief/Monk had none.

skill_proficiency() (skill.c) now special-cases SKILL_TIER_COMBAT:
returns 0 until any Combat discipline is trained, then automatically
tracks combat_disc_pct 1:1 (floored at 1%, capped at combat_disc_pct's
own value, up to 100 at full discipline) -- no separate per-skill
grinding needed. All 6 classes now carry all 5 roster rows.

Covers:
  1. Warrior (previously had ZERO proficiency skills) now knows all 5.
  2. At 0% Combat discipline, proficiency reads 0 (not yet trained).
  3. At 1% Combat discipline, proficiency reads exactly 1% (the floor)
     for every one of the 5 skills, with NO combat use at all.
  4. At 100% Combat discipline, proficiency reads exactly 100% for
     every one of the 5, still with no combat use.
  5. A mid-value (55%) Combat discipline tracks exactly, confirming
     it's not just floor/ceiling special-cased.

    python3 tests/smoke_test_proficiency_autotrack.py [host] [port]
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

PROF_SKILLS = ["slash proficiency", "blunt proficiency", "pierce proficiency", "barehand proficiency"]


announce("smoke_test_proficiency_autotrack", host, port)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def profs_from_skills_output(out):
    """Extracts {skill_name: pct} for every 'X proficiency [N%]' line."""
    result = {}
    for m in re.finditer(r"(\w+ proficiency)\s+.*?\[(\d+)%\]", out):
        result[m.group(1)] = int(m.group(2))
    return result


name, pw = f"Proftrk{_suffix}", "proftrkpw1234"
s = make_char(name, pw, 3)  # Warrior
cmd(s, "quit!"); s.close()

# --- 1/2: at 0% Combat discipline, all 5 exist but read as locked/0 ---
set_combat_disc(name, 0)
s = relog(name, pw)
out = cmd(s, "practice combat")
for sk in PROF_SKILLS:
    check(sk in out.lower(), f"Warrior's Combat listing includes '{sk}' (previously had ZERO proficiency skills)")
check("unlock" in out.lower(), "at 0% Combat discipline, the skills show as locked, not a live percentage")

# --- 3: at 1% discipline, all 5 read exactly 1% with no combat use ---
set_combat_disc(name, 1)
s.close()
s = relog(name, pw)
out = cmd(s, "practice combat")
profs = profs_from_skills_output(out)
for sk in PROF_SKILLS:
    check(profs.get(sk) == 1, f"'{sk}' reads exactly 1% at 1% Combat discipline, no combat use needed (got {profs.get(sk)})")

# --- 5: a mid-value (55%) tracks exactly, not just floor/ceiling ---
set_combat_disc(name, 55)
s.close()
s = relog(name, pw)
out = cmd(s, "practice combat")
profs = profs_from_skills_output(out)
for sk in PROF_SKILLS:
    check(profs.get(sk) == 55, f"'{sk}' reads exactly 55% at 55% Combat discipline (got {profs.get(sk)})")

# --- 4: at 100% discipline, all 5 read exactly 100% ---
set_combat_disc(name, 100)
s.close()
s = relog(name, pw)
out = cmd(s, "practice combat")
profs = profs_from_skills_output(out)
for sk in PROF_SKILLS:
    check(profs.get(sk) == 100, f"'{sk}' reads exactly 100% at 100% Combat discipline (got {profs.get(sk)})")

s.close()

announce_done("smoke_test_proficiency_autotrack", host, port)
print("=== ALL CHECKS PASSED ===")
