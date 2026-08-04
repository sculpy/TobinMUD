#!/usr/bin/env python3
"""Smoke test for socials (socials.c, cmd_socials.c + the dispatch hook) --
the full DB-backed port from the upstream lib/actions file (db/import-
socials.py, ~155 verbs), not just the old hand-rolled 16.

  1. An untargeted social shows a self message and a room message (real
     upstream wording, e.g. smile's "You smile happily.").
  2. A targeted social shows self / target / room messages.
  3. Targeting YOURSELF now gets a proper dedicated message (self_auto/
     others_auto), not just a repeat of the no-target self message -- a
     real behavioral improvement the port brought along for free.
  4. Pronoun ($s/$e/$m/...) substitution matches the actor's gender.
  5. A social's own not_found wording is used (per-social now, not one
     generic string for everything).
  6. min_position gates a social (dance needs at least CRAWLING; refused
     while asleep, allowed while standing).
  7. `socials` lists the verbs, paginated (155 overflows one screen).
  8. Abbreviation still resolves to the closest verb.
  9. `point`'s Tobin-original held-item form still works, on top of the
     ported base wording.

    python3 tests/smoke_test_socials.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_socials", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(nm, gender_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "y", "socpw", "socpw", "new", nm, "1", "1", "1", "done", "done"):
        send_line(s, step); recv_all(s)
    # gender is set post-creation via SQL below (character creation doesn't
    # prompt for it directly in every build) -- see set_gender().
    return s


def set_gender(name, gender):
    # being.h: 0=neutral/it, 1=male, 2=female (matches gender_t) -- confirmed
    # against gender_subject()/gender_possess() callers elsewhere in tests.
    sql(f"UPDATE player SET gender={gender} WHERE name='{name}';")


# Two fresh mortals both land in Center Square (100) -- same room.
nameA, nameB = f"Soca{_suffix}", f"Socb{_suffix}"
sA = make_char(nameA, 1)
sB = make_char(nameB, 1)
set_gender(nameA, 1)  # male, for a deterministic $s/$m/$e pronoun check
recv_all(sA); recv_all(sB)   # drain arrival notices
sA.close()
sA = socket.create_connection((host, port), timeout=5)
recv_all(sA)
send_line(sA, nameA); recv_all(sA)
send_line(sA, "socpw"); recv_all(sA)
send_line(sA, "1"); recv_all(sA)
cmd(sA, "color off")
recv_all(sB)

# --- 1: untargeted social, real upstream wording ---
out = cmd(sA, "smile")
check("You smile happily." in out, "an untargeted social shows the real ported self message")
check("smiles happily" in recv_all(sB), "the room sees the untargeted social")

# --- 2: targeted social ---
out = cmd(sA, f"smile {nameB}")
check(f"You smile at" in out, "a targeted social shows the self-with-target message")
check("smiles at you" in recv_all(sB), "the target sees the aimed social")

# --- 3: targeting yourself gets its OWN dedicated message ---
out = cmd(sA, f"smile {nameA}")
check("You smile at yourself." in out,
      "targeting yourself now uses the dedicated self_auto message, not a repeat of the no-target one")

# --- 4: pronoun substitution matches the actor's gender (male -> his).
# shake's others_no_arg template is literally "$n shakes $s head." ($s =
# ACTOR's own possessive) -- the room-echo is the precise test of this;
# shake's OWN self message is hardcoded "your" (no token), so it doesn't
# exercise the engine at all. ---
out = cmd(sA, "shake")
check("You shake your head." in out, "shake's self message (literal wording, not token-derived)")
check(f"{nameA} shakes his head." in recv_all(sB),
      "the room sees the CORRECT gendered pronoun ($s -> his, nameA is male)")

# --- 5: per-social not_found wording (not one generic string) ---
check("There's no one by that name around." in cmd(sA, f"smile Nobody{_suffix}"),
      "smile's own not_found wording is used")
check("Eh, whom?" in cmd(sA, f"dance Nobody{_suffix}"),
      "dance's DIFFERENT not_found wording is used -- confirms these are per-social, not generic")

# --- 6: min_position gates a social (dance needs >= CRAWLING) ---
# Uses "sit" rather than "sleep" -- sitting is ALSO below CRAWLING (so
# still exercises the min_position gate this check targets) but isn't
# literally asleep, so it doesn't collide with the separate 2026-08-03
# "sleeping blocks everything" dispatch gate (cmd_table.c), which would
# otherwise intercept `dance` earlier with its own unrelated message.
cmd(sA, "sit")
check("can't do that right now" in cmd(sA, "dance"),
      "dance is refused while sitting (below its min_position)")
cmd(sA, "stand")
check("Feels silly" in cmd(sA, "dance"),
      "dance is allowed (and shows its real ported wording) once standing")

# --- 7: socials list, paginated, full port scale. The list is alphabetical
# and multiple screens long, so a verb near the end ("wave") only shows up
# after paging through -- drain the pager fully (same pattern as
# vnum_read() in smoke_test_vnum.py) rather than checking one page. ---
first = cmd(sA, "socials")
check("ENTER for more, Q to stop" in first,
      "the (much larger, ~155-verb) list is long enough to need paging")
full = first
resp = first
guard = 0
while "ENTER for more" in resp and guard < 30:
    resp = cmd(sA, "")
    full += resp
    guard += 1
check("smile" in full and "growl" in full and "wave" in full,
      "socials lists both old-familiar and newly-ported verbs, across the full paged listing")

# --- 8: abbreviation still resolves to the closest verb. "smi" is
# ambiguous between smile/smirk in the real ported set -- confirms
# alphabetical-first-match still wins (smile < smirk). ---
check("point around randomly" in cmd(sA, "poi"), "'poi' abbreviates to the point social")
check("You smile happily." in cmd(sA, "smi"), "'smi' abbreviates to smile (alphabetically before smirk)")

# --- 9: point's Tobin-original held-item form still works. `load obj` is
# builder-only, so nameA is briefly promoted to spawn+wield a sandbox item
# -- a level change only takes effect on the NEXT login (cached in memory
# otherwise, same as gender above), so reconnect after setting it. ---
check("point around randomly" in cmd(sA, "point"), "point with empty hands uses the plain wording")

STICK = 900000 + (int(time.time()) % 90000)
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameA}');")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({STICK},'stick','a wooden stick','A wooden stick is lying here.',5,16385,1);")
sA.close()
sA = socket.create_connection((host, port), timeout=5)
recv_all(sA)
send_line(sA, nameA); recv_all(sA)
send_line(sA, "socpw"); recv_all(sA)
send_line(sA, "1"); recv_all(sA)
cmd(sA, "color off")
recv_all(sB)

check("You conjure" in cmd(sA, f"load obj {STICK}"), "spawned a sandbox stick to wield")
cmd(sA, "get stick")
cmd(sA, "wield stick")
out = cmd(sA, "point")
check("point around with your wooden stick" in out,
      "point with a held item uses the item-referencing form (short_descr's article "
      "stripped, but the rest -- 'wooden stick' -- kept verbatim)")
sql(f"UPDATE player_progress SET level=1 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameA}');")

announce_done("smoke_test_socials", host, port)
print("=== ALL CHECKS PASSED ===")
