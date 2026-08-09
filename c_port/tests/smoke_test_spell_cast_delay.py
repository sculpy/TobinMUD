#!/usr/bin/env python3
"""Smoke test for the multi-round `cast` delay (user, 2026-08-09: "spell
casting should take 2-3 rounds before hitting with purple colored
messaging about 2-3 lines per casting tick. druids should have modified
messages that mages have except those messages should have a forest
flavor to them. druid casting should take about the same amount of
time"; follow-up correction: "druid messageing should be <y>").

Before this change, `cast <spell>` resolved its real effect the instant
the command was typed (cmd_cast.c's old inline `task_cast()`). Now,
once every existing gate (class/level/discipline/mana/component/
proficiency roll -- all UNCHANGED) has passed, cmd_cast.c hands off to
spellcast.c's spellcast_start()/spellcast_tick_run(): the caster is
locked out (being_set_wait()) for 2-3 COMBAT_ROUND_PULSES rounds, shown
that round's flavor text each tick (Mage: purple <p>, arcane-incantation
flavor; Druid: yellow <y>, a structurally-identical forest-flavor
reskin), and only once the countdown reaches 0 does the spell's real
effect actually land -- via cmd_cast_resolve_effect(), the exact same
per-spell dispatch `cast` always used, just moved out from under the
instant path.

Uses two spells whose effect branch keys off the spell's own NAME (not
its `desc`, which is a generic "See help `X` for help." placeholder
across the whole roster) so the effect message is unambiguous:
`sorcerer's globe` (Mage, min level 1, applies AFFECT_SANCTUARY
room-wide) and `barkskin` (Druid, min level 3, applies AFFECT_SANCTUARY
to self) -- both self-targeted, so no live opponent is needed.

Covers:
  1. Casting as a Mage shows purple (<p>) flavor text immediately, and
     the spell's real effect message has NOT appeared yet at that point.
  2. That same effect message DOES appear ~1-3 rounds later, once the
     delay completes -- i.e. the effect is genuinely deferred, not just
     re-labeled.
  3. Casting as a Druid shows yellow (<y>) flavor text (not purple),
     with forest-flavored wording, same delayed-then-lands shape.

    python3 tests/smoke_test_spell_cast_delay.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

PURPLE = ("\x1b[0;35m", "\x1b[1;35m")
YELLOW = ("\x1b[0;33m", "\x1b[1;33m")

announce("smoke_test_spell_cast_delay", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26 ** i) % 26) for i in range(4))


def create_char(name, pw, char_class):
    """New account -> new character, same account-menu sequence
    smoke_test_email.py's create_through_email_prompt() established
    (name, confirm-new, pw x2, enable-color, skip-timezone, skip-email),
    then the standard finish_char_creation() steps (race/territory/
    class/attrs-done/options-done)."""
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "y", ""):
        send_line(s, step)
        recv_all(s)
    send_line(s, "")  # skip email opt-in
    recv_all(s)
    for step in ("new", name, "1", "1", char_class, "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    return s


mage_name, mage_pw = f"Cdlym{_suffix}", "castdelaypw1"
druid_name, druid_pw = f"Cdlyd{_suffix}", "castdelaypw2"

create_char(mage_name, mage_pw, "1")   # 1 = CLASS_MAGE
create_char(druid_name, druid_pw, "5")  # 5 = CLASS_DRUID

# level 51 (> MORTAL_LEVEL_MAX) -> immortal: bypasses level/discipline/
# mana gates so this test only exercises the timing/flavor/effect being
# checked, same "level=51 to reach the real cast machinery cleanly"
# precedent smoke_test_cast_pray_flavor.py already uses. char_class
# itself (set at creation, unaffected by immortal status) is what
# spellcast.c actually reads to pick purple vs. yellow.
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mage_name}');")
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{druid_name}');")

# --- Mage: purple flavor immediately, effect lands only after the delay ---
s = login(mage_name, mage_pw)
cmd(s, "load obj 200")  # "a tiny lasso made of basilisk hair" -- real spell component
out = cmd(s, "cast sorcerer's globe", timeout=0.6)
check(any(tag in out for tag in PURPLE), "Mage cast shows purple (<p>) flavor color immediately")
check("shimmering dome of protection" not in out,
      "Mage spell effect has NOT landed yet right after the cast command")
out2 = cmd(s, "", timeout=4.5)  # drain the remaining round(s) + the effect once it resolves
check("shimmering dome of protection" in out2,
      "Mage spell effect lands after the multi-round delay completes")
s.close()

# --- Druid: yellow forest-flavor, same delayed-then-lands shape ---
s = login(druid_name, druid_pw)
cmd(s, "load obj 200")
out = cmd(s, "cast barkskin", timeout=0.6)
check(any(tag in out for tag in YELLOW), "Druid cast shows yellow (<y>) flavor color immediately")
check(not any(tag in out for tag in PURPLE), "Druid cast does NOT show Mage's purple")
check("protective ward settles over you" not in out,
      "Druid spell effect has NOT landed yet right after the cast command")
out2 = cmd(s, "", timeout=4.5)
check("protective ward settles over you" in out2,
      "Druid spell effect lands after the multi-round delay completes")
s.close()

print("ALL CHECKS PASSED")
announce_done("smoke_test_spell_cast_delay", host, port)
