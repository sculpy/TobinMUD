#!/usr/bin/env python3
"""Smoke test for `edplayer` (cmd_edplayer.c / descriptor_edplayer_begin()):
  1. Gate: invisible below level 58 (Administrator), like promote's tier.
  2. Editing a nonexistent name is rejected without leaving the menu.
  3. Every field (level, experience, hp/max hp, attributes, gender, title,
     load room, handedness) can be set through the numbered menu and is
     reflected back in the menu/submenu display.
  4. (S)ave persists to the DB -- verified both by reconnecting as the
     edited character AND by an already-connected session of that
     character seeing the change live, with no relog.
  5. (Q)uit with unsaved changes prompts, and (D)iscard truly discards --
     the pre-edit values survive.

    python3 tests/smoke_test_edplayer.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_edplayer", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(nm, pw="edplayertest123"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm, pw="edplayertest123"):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, pw); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


def promote_sql(nm, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")


target = f"Edptgt{_suffix}"
admin = f"Edpadm{_suffix}"

tsock = make_char(target)
# "quit!" leaves to the account menu first (a real disconnect, character
# detached cleanly) -- an abrupt close while still playing would instead
# leave the character linkdead in its CURRENT room, overriding the load
# room this test sets below via edplayer (see world_find_linkdead_pc()).
cmd(tsock, "quit!")
tsock.close()
sa = make_char(admin)
sa.close()

# --- gate: 57 can't reach it, 58 can ---
promote_sql(admin, 57)
sa = relogin(admin)
check("Command not found" in cmd(sa, f"edit player {target}"), "a level-57 immortal can't use edit player (gate is 58)")
sa.close()

promote_sql(admin, 58)
sa = relogin(admin)

# --- nonexistent target ---
out = cmd(sa, f"edit player NoSuchPlayer{_suffix}")
check("No player named" in out, "editing a nonexistent player is rejected")

# --- open the real target, check the menu shape ---
out = strip(cmd(sa, f"edit player {target}"))
check(f"Editing player: {target}" in out, "the menu names the player being edited")
check("1) Level:" in out and "2) Experience:" in out and "3) HP/Max HP:" in out
      and "4) Attributes" in out and "5) Gender:" in out and "6) Title:" in out
      and "7) Load Room:" in out and "8) Handedness:" in out
      and "9) Class:" in out and "0) Race:" in out,
      "the menu shows all ten fields")

# --- edit each field ---
out = strip(cmd(sa, "1"))
check("Enter new level" in out, "option 1 prompts for level")
out = strip(cmd(sa, "30"))
check("Level: 30" in out, "level updates in the menu")

out = strip(cmd(sa, "2"))
check("Enter new experience" in out, "option 2 prompts for experience")
out = strip(cmd(sa, "5000"))
check("Experience: 5000" in out, "experience updates in the menu")

out = strip(cmd(sa, "3"))
check("HP and Max HP" in out, "option 3 prompts for HP/Max HP")
out = strip(cmd(sa, "40 60"))
check("HP/Max HP: 40/60" in out, "HP/Max HP updates in the menu")

out = strip(cmd(sa, "4"))
check("Attributes for" in out, "option 4 opens the attribute sub-editor")
out = strip(cmd(sa, "str 200"))
check("Str 200" in out, "setting an attribute reflects immediately in the sub-editor")
out = strip(cmd(sa, "done"))
check("HP/Max HP: 40/60" in out, "'done' returns to the main menu")

out = strip(cmd(sa, "5"))
check("Enter gender" in out, "option 5 prompts for gender")
out = strip(cmd(sa, "female"))
check("Gender: female" in out, "gender updates in the menu")

out = strip(cmd(sa, "6"))
check("Enter new title" in out, "option 6 prompts for title")
out = strip(cmd(sa, "the Tested"))
check("Title: the Tested" in out, "title updates in the menu")

out = strip(cmd(sa, "7"))
check("load-room vnum" in out, "option 7 prompts for load room")
out = strip(cmd(sa, "1"))
check("Load Room: 1" in out, "load room updates in the menu")

out = strip(cmd(sa, "8"))
check("Enter handedness" in out, "option 8 prompts for handedness")
out = strip(cmd(sa, "left"))
check("Handedness: left" in out, "handedness updates in the menu")

out = strip(cmd(sa, "9"))
check("Enter class" in out, "option 9 prompts for class")
out = strip(cmd(sa, "warrior"))
check("Class: Warrior" in out, "class updates in the menu")

out = strip(cmd(sa, "0"))
check("Enter race" in out, "option 0 prompts for race")
out = strip(cmd(sa, "elf"))
check("Race: Elf" in out, "race updates in the menu")

check("unsaved changes" in out, "the menu flags unsaved changes before Save")

# --- Save, then Quit ---
out = cmd(sa, "S")
check("Player saved" in out, "S saves the working copy")
cmd(sa, "Q")

# --- verify via a fresh reconnect: level/xp/hp/attrs/gender/title/handed/room ---
st = relogin(target)
out = strip(cmd(st, "score"))
check("Level: 30" in out, "the saved level persisted")
check("Experience: 5000" in out, "the saved experience persisted")
check("HP: 4" in out and "60 Max" in out,
      "the saved HP/Max HP persisted (current HP may have ticked up slightly from regen "
      "by the time this reconnects, so just check the max and that it starts with 4x)")
check("Str: 200" in out, "the saved attribute persisted")
check("Sex: female" in out, "the saved gender persisted")
check("Pri. Hand: Left" in out, "the saved handedness persisted")
check("Race: Elf" in out, "the saved race persisted")
check("Class: Warrior" in out, "the saved class persisted")

out = strip(cmd(st, "who"))
check("the Tested" in out, "the saved title shows in who")

out = strip(cmd(st, "look"))
check("Imperia" in out, "the saved load room (vnum 1) took effect on this fresh login")
st.close()

# --- online-sync: edit an ALREADY-connected target live, no relog ---
st = relogin(target)
out = strip(cmd(sa, f"edit player {target}"))
out = strip(cmd(sa, "6"))
out = strip(cmd(sa, "Live Synced"))
check("Title: Live Synced" in out, "title updated in the working copy")
cmd(sa, "S")
cmd(sa, "Q")
out = strip(cmd(st, "who"))
check("Live Synced" in out, "the already-connected target's title updated live, no relog")
st.close()

# --- unsaved-changes Quit: (D)iscard truly discards ---
out = strip(cmd(sa, f"edit player {target}"))
out = strip(cmd(sa, "1"))
out = strip(cmd(sa, "45"))
check("Level: 45" in out, "level changed in the working copy (not yet saved)")
out = cmd(sa, "Q")
check("unsaved changes" in out, "Q with unsaved changes prompts before leaving")
out = cmd(sa, "D")
check("Leaving the player editor" in out, "D discards and leaves")

st = relogin(target)
out = strip(cmd(st, "score"))
check("Level: 30" in out, "the discarded level change did NOT persist -- still 30")
st.close()

sa.close()
announce_done("smoke_test_edplayer", host, port)
print("=== ALL CHECKS PASSED ===")
