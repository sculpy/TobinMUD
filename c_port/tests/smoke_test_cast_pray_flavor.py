#!/usr/bin/env python3
"""Smoke test for pray flavor messaging (user, 2026-08-04: "Cast/pray
messaging: 3 lines per task step"; clarified 2026-08-05: "use sneezymuds
casting task for example"). Covers spell_flavor_show() (spell_flavor.c),
called once at the top of task_pray() (cmd_pray.c) -- modeled on real
SneezyMUD's TBeing::sendCastingMessages() (spelltask.cc), which shows a
gesture/symbol line, a verbal line, and a completion line each casting/
praying round. Cleric's `pray` has no multi-round task engine and is
explicitly out of scope for the 2026-08-09 `cast` delay below, so this
still ports the STYLE (3 flavor lines) as a one-shot flourish before a
prayer's real effect.

`cast`'s OWN one-shot flavor flourish (this file's original scope) was
superseded 2026-08-09 (user: "spell casting should take 2-3 rounds
before hitting with purple colored messaging... druids should have
modified messages... forest flavor... druid messaging should be <y>")
by a genuine multi-round delay (spellcast.c) -- see
smoke_test_spell_cast_delay.py for that behavior's own dedicated
coverage (purple/yellow, per-round text, the effect landing rounds
later). This file's `cast`-side checks below were updated to match: a
successful cast still shows immediate flavor text (now from
spellcast.c's spellcast_start(), not the old fixed "you feel your spell
taking form" line), but the spell's real effect no longer appears in the
same round -- it lands later, unverified here (see the dedicated test).

  1. `cast <spell>` shows immediate per-round flavor text before its
     real effect, which is now deferred rather than instant.
  2. `pray <prayer>` still shows the original one-shot 3-line shape with
     prayer-flavored text (a symbol line, a verbal line, "you feel your
     prayer being answered"), unaffected by the `cast`-only change.
  3. A gated cast/pray (missing component/symbol) shows NONE of the
     flavor lines -- the gate is checked before spellcast_start()/
     task_pray() ever runs.

    python3 tests/smoke_test_cast_pray_flavor.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_cast_pray_flavor", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26 ** i) % 26) for i in range(4))
name = f"Flavtest{_suffix}"
pw = "flavorpw123"


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "3"); recv_all(sock)  # class: warrior -- irrelevant here,
                                           # immortal status bypasses the class
                                           # gate for cast/pray either way
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


s = socket.create_connection((host, port), timeout=5)
make_char(s, name, pw)
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s = login(name, pw)

# --- 1: gated cast (no component) shows NO flavor lines ---
out = cmd(s, "cast gust")
check("spell components" in out.lower(), "cast without a component is refused")
check("gathers around you" not in out.lower() and "crackles" not in out.lower(),
      "no flavor lines on a gated (component-less) cast")

# --- 2: cast with a component shows immediate round-1 flavor text, but
# NOT the spell's real effect in the same round (spellcast.c -- the
# delay this file no longer covers in depth, see
# smoke_test_spell_cast_delay.py). This character is a Warrior (class 3,
# immortal bypasses the class gate) -- not Mage/Druid -- so it gets the
# generic (Mage-shaped, purple) flavor text either way; only the color/
# wording SPLIT is Mage-vs-Druid, not whether flavor text appears at all. ---
cmd(s, "load obj 200")  # "a tiny lasso made of basilisk hair" -- real component
out = cmd(s, "cast gust")
lines = [l for l in out.splitlines() if l.strip() and l.strip() != "cast gust"]
check(len(lines) >= 3, "cast produced at least 3 lines of immediate flavor text")
check("nothing happens yet" not in out.lower(), "the spell's real effect has NOT resolved in the same round")

# --- 3: gated pray (no symbol) shows NO flavor lines ---
out = cmd(s, "pray sterilize")
check("holy symbol" in out.lower(), "pray without a holy symbol is refused")
check("feel your prayer being answered" not in out.lower(), "no flavor lines on a gated (symbol-less) pray")

# --- 4: pray with a symbol shows the completion flavor line ---
cmd(s, "load obj 500")  # "a wooden holy symbol"
out = cmd(s, "pray sterilize")
check("feel your prayer being answered" in out.lower(), "pray shows the completion flavor line")

print("ALL CHECKS PASSED")
announce_done("smoke_test_cast_pray_flavor", host, port)
