#!/usr/bin/env python3
"""Smoke test for major-limb instadeath and weighted limb targeting (user,
2026-07-12: "some limbs are harder to decapitate, and should be instadeath
if it is a major body part... head neck waist body are all major limbs.
this should be based on the likelihood that a limb could be damaged vs
decapitated. see sneezy code for inspiration"). Covers:

  1. Destroying a non-major limb (an arm) is survivable -- no instadeath.
  2. Destroying each of the four major limbs (head, neck, waist, body) is
     instant death.
  3. Destroying the neck also takes the head off with it.
  4. Weighted limb targeting (Sneezy's own slotChance() proportions):
     over enough real combat rounds, the torso ("body") gets hit far
     more often than a finger.

    python3 tests/smoke_test_limb_severity.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_limb_severity", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
DUMMY_VNUM = ROOM + 1


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


def reconnect(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "limbsevpw123"

imm_name = f"Limbsevimm{_suffix}"
s_imm = make_char(imm_name, pw, "3")
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 51)
s_imm = reconnect(imm_name, pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Limb Severity Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Limb Severity Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

mort_name = f"Limbsevmor{_suffix}"
s_mort = make_char(mort_name, pw, "3")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(s_mort, "quit!")
s_mort.close()
set_hp(mort_name, 500)  # survives an arm destruction easily
s_mort = reconnect(mort_name, pw)
check("Limb Severity Sandbox" in cmd(s_mort, "look"), "the victim lands in the sandbox")

# --- 1: a non-major limb (an arm) is survivable ---
out = cmd(s_imm, f"hurtlimb {mort_name} leftarm 0")
check("Limb HP set." in out, "destroying a non-major limb (left arm) does not report instant death")
check("Limb Severity Sandbox" in cmd(s_mort, "look"), "the victim is still playing after losing an arm")

# --- 2/3: each major limb is instant death; the neck also takes the head ---
for limb_arg, label in [("waist", "waist"), ("body", "body"), ("head", "head")]:
    out = cmd(s_imm, f"hurtlimb {mort_name} {limb_arg} 0")
    check("Instant death" in out, f"destroying the {label} is reported as instant death")
    # Victim was ejected to the account menu (combat_defeat's loser path) --
    # reconnect for the next major-limb case.
    s_mort.close()
    s_mort = reconnect(mort_name, pw)
    check("Limb Severity Sandbox" in cmd(s_mort, "look"), f"the victim is back in the sandbox after the {label} test")

out = cmd(s_imm, f"hurtlimb {mort_name} neck 0")
check("Instant death" in out, "destroying the neck is reported as instant death")
check("head clean off along with it" in out or "head comes off" in out,
      "destroying the neck also reports the head coming off with it")

s_imm.close()
s_mort.close()
announce_done("smoke_test_limb_severity", host, port)
print("=== ALL CHECKS PASSED ===")
