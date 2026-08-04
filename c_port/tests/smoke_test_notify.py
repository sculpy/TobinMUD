#!/usr/bin/env python3
"""Smoke test for Session 21's notification batch:
  1. Movement announcement: the room sees "<name> exits to the <dir>."
     when someone walks out (phrasing per user spec).
  2. Prompt-after-message: unsolicited output (someone else's movement,
     a broadcast) leaves the receiving player at a fresh "> " prompt.
  3. Link-drop: the room is told, and every non-editing immortal receives
     the typed "[PIO] ... has lost its link." log line (gender-specific
     pronoun; the test victim is neuter, the creation default); mortals do
     NOT get [PIO]; an immortal who is mid-editor gets nothing.

    python3 tests/smoke_test_notify.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_notify", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def set_level(name, level):
    subprocess.run(
        ["mariadb", "tobin", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def make_player(tag):
    name = f"Ntf{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, "notifypw")
    recv_all(s)
    send_line(s, "notifypw")  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s, name


def relog(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "notifypw")
    recv_all(s)
    send_line(s, "1")
    recv_all(s)
    return s


sWatcher, nameWatcher = make_player("Watch")
sWalker, nameWalker = make_player("Walk")
send_line(sWalker, "color off")  # so the [Exits:] regex below isn't broken up by ANSI codes
recv_all(sWalker)

# --- Part 1 + 2: exit announcement, with a prompt after it ---
# Find a direction that actually leads somewhere from the start room.
send_line(sWalker, "look")
out = recv_all(sWalker)
m = re.search(r"\[Exits:\]\s*([A-Za-z ]+)", out)
check(m and m.group(1).split(), "the start room lists at least one obvious exit")
direction = m.group(1).split()[0].lower()  # the [Exits:] list is capitalized; lowercase to match room-echo text

send_line(sWalker, direction)
recv_all(sWalker)
outW = recv_all(sWatcher, timeout=1.0)
check(f"{nameWalker.capitalize()} exits to the {direction}." in outW
      or f"{nameWalker.capitalize()} exits {direction}ward." in outW,
      f"the room sees '<name> exits to the {direction}.' when someone walks out")
check(outW.rstrip().endswith(">"),
      "the unsolicited movement message leaves the watcher at a fresh prompt")

# --- Part 3: link-drop -> room told; [LOG] to non-editing immortals only ---
sImm, nameImm = make_player("Imm")
set_level(nameImm, 51)
sImm.close()
sImm = relog(nameImm)

sEditor, nameEditor = make_player("Edt")
set_level(nameEditor, 56)
sEditor.close()
sEditor = relog(nameEditor)
send_line(sEditor, f"edit help scratch{_suffix}")
recv_all(sEditor)  # now mid-editor: must NOT receive the [LOG] line

sVictim, nameVictim = make_player("Vic")
recv_all(sWatcher)  # drain arrivals etc.
recv_all(sImm)
sVictim.close()  # abrupt close while playing = link drop
time.sleep(0.5)

outImm = recv_all(sImm, timeout=1.0)
# Link-drops are now a typed [PIO] log, cyan-colored (<c>[PIO]<z>), so strip
# ANSI for the text assertion and check the color separately.
outImm_plain = re.sub(r"\x1b\[[0-9;]*m", "", outImm)
check(f"[PIO] {nameVictim.capitalize()} has lost its link." in outImm_plain,
      "a non-editing immortal receives the [PIO] link-drop line (gender-specific pronoun)")
check("\x1b[0;36m[PIO]\x1b[0m" in outImm, "the [PIO] log tag is cyan-colored")

outWatcher = recv_all(sWatcher, timeout=1.0)
check("[PIO]" not in outWatcher, "a mortal does not receive the [PIO] line")

outEditor = recv_all(sEditor, timeout=1.0)
check("[LOG]" not in outEditor,
      "an immortal mid-editor is spared the [LOG] line (screen not corrupted)")
send_line(sEditor, "/a")  # leave the editor cleanly
recv_all(sEditor)

# hygiene
set_level(nameImm, 1)
set_level(nameEditor, 1)
for s in (sWatcher, sWalker, sImm, sEditor):
    s.close()
announce_done("smoke_test_notify", host, port)
print("=== ALL CHECKS PASSED ===")
