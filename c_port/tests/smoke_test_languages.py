#!/usr/bin/env python3
"""Smoke test for the Tier-4 speak-a-language subsystem (2026-08-16).

Two mortals share the newbie start room. The SPEAKER is bumped to level 20
with full disciplines -- enough to `know` (and therefore `speak`) the
racial tongues like Trollish -- but keeps a fresh 0% Common proficiency, so
they are NOT a fluent Common speaker and their foreign speech genuinely
garbles for a listener who doesn't know the tongue (that's exactly what
getLanguageChance models: fluent-Common speakers come through clear, so the
test deliberately uses a non-fluent one). The LISTENER stays level 1 and
knows no tongue but Common.

Checks:
  * GATING     -> a level-1 mortal is refused `speak trollish`; the level-20
                  speaker is allowed it, and `speak` lists it as known.
  * SAY GARBLE -> speaking Trollish tags BOTH the speaker's own echo and the
                  listener's copy with "(in Trollish)", and the listener's
                  received text is garbled (not the verbatim phrase).
  * WHISPER    -> a Trollish whisper reaches the target tagged "(in Trollish)".
  * TELL       -> a Trollish tell reaches the target tagged "(in Trollish)".
  * COMMON     -> switching back to Common drops the tag and delivers the
                  listener a clean, verbatim copy.

    python3 tests/smoke_test_languages.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import finish_char_creation

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_languages", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
SPEAKER = f"Langtongue{_suffix}"   # letters only -- character names reject digits
LISTENER = f"Langear{_suffix}"
PW = "langpw12345"

PHRASE = "the fierce cats hiss softly"  # rich in s/h/c/f -> Trollish mangles it


def _connect_and_create(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in (name, "y", pw, pw):
        send_line(s, step); recv_all(s, 0.7)
    send_line(s, "new"); recv_all(s, 0.7)
    finish_char_creation(s, name, send_line, recv_all,
                         race="1", territory="1", char_class=class_choice)
    return s


def make_char(name, pw, class_choice):
    s = _connect_and_create(name, pw, class_choice)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


# --- create both, bump only the speaker to level 20 ---------------------
make_char(SPEAKER, PW, "5")
make_char(LISTENER, PW, "5")
sql(f"UPDATE player_progress SET level=20,"
    f"basic_disc_pct=100,advanced_disc_pct=100,combat_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{SPEAKER}');")

a = login(SPEAKER, PW)     # the speaker (knows Trollish)
b = login(LISTENER, PW)    # the listener (Common only)

# --- GATING -------------------------------------------------------------
out = cmd(b, "speak trollish")
check("don't know how to speak" in out.lower(),
      "a level-1 mortal is refused `speak trollish`")

out = cmd(a, "speak trollish")
check("now speak in trollish" in out.lower(),
      "the level-20 speaker is allowed `speak trollish`")

out = cmd(a, "speak")
check("trollish" in out.lower() and "common" in out.lower(),
      "`speak` lists Trollish among the speaker's known tongues")

# --- SAY GARBLE ---------------------------------------------------------
recv_all(b, 0.5)  # drain
out_a = cmd(a, f"say {PHRASE}")
check("(in trollish)" in out_a.lower(),
      "the speaker's own say echo is tagged (in Trollish)")

heard = recv_all(b, 1.0)
check("(in trollish)" in heard.lower() and "says" in heard.lower(),
      "the listener's copy is tagged (in Trollish)")
check(PHRASE not in heard,
      "the listener's Trollish copy is garbled, not the verbatim phrase")

# --- WHISPER (still Trollish) -------------------------------------------
recv_all(b, 0.5)
cmd(a, f"whisper {LISTENER} secret plans afoot")
heard = recv_all(b, 1.0)
check("(in trollish)" in heard.lower() and "whispers to you" in heard.lower(),
      "a Trollish whisper reaches the target tagged (in Trollish)")

# --- TELL (still Trollish) ----------------------------------------------
recv_all(b, 0.5)
cmd(a, f"tell {LISTENER} meet me at dawn")
heard = recv_all(b, 1.0)
check("(in trollish)" in heard.lower() and "tells you" in heard.lower(),
      "a Trollish tell reaches the target tagged (in Trollish)")

# --- COMMON: tag drops, listener hears it clean -------------------------
out = cmd(a, "speak common")
check("now speak in common" in out.lower(),
      "the speaker can switch back to Common")

recv_all(b, 0.5)
out_a = cmd(a, "say plainly now in common")
check("(in " not in out_a.lower(),
      "Common speech carries no language tag for the speaker")
heard = recv_all(b, 1.0)
check("plainly now in common" in heard,
      "the listener hears Common speech verbatim (no garble)")

# --- cleanup ------------------------------------------------------------
send_line(a, "quit!"); recv_all(a)
send_line(b, "quit!"); recv_all(b)
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{SPEAKER}','{LISTENER}'));")
sql(f"DELETE FROM player WHERE name IN ('{SPEAKER}','{LISTENER}');")

announce_done("smoke_test_languages", host, port)
print("\n=== smoke_test_languages PASSED ===")
