#!/usr/bin/env python3
"""Smoke test for the Cook profession (Sneezy -> Tobin feature audit,
user 2026-07-26: "professions" -- task_cook.h/.cc, real ingredient-
matching recipes).

  1. `cook` alone lists the known recipes.
  2. A simple two-VNUM-ingredient recipe (mashed potatoes: potato +
     butter) succeeds when both are carried, and both are actually
     consumed (gone from inventory afterward).
  3. Trying to cook the same recipe again with no ingredients left is
     refused, and nothing new gets created.
  4. A COOK_ING_CORPSE recipe (fried chicken: a real BIRD-race corpse +
     a jar of whale grease) works off a genuine killed-mob corpse, not
     a stubbed check -- confirms the new corpse->val[2] source-race
     field (obj.h) actually round-trips from combat.c to cook.c.

    python3 tests/smoke_test_cook.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_cook", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 973000 + (int(time.time()) % 20000)
POTATO_VNUM = 31766
BUTTER_VNUM = 31767
GREASE_VNUM = 263
BIRD_MOB_VNUM = 119  # "crow small ugly", real seeded mob, race=16/BIRD


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def cmd_paged(sock, line, timeout=1.0):
    """Like cmd(), but drains a paginated ("[ENTER for more]") response
    fully before returning -- a long `inventory` listing can trip the
    pager (grew past the threshold since this test was written, per the
    2026-08-02 newbie-gear-expansion sessions adding more starting
    items), and a later command sent while a page is still pending gets
    silently swallowed as a "show more" keystroke instead of dispatched."""
    out = cmd(sock, line, timeout)
    full = out
    while "enter" in out.lower() and "more" in out.lower():
        out = cmd(sock, "", timeout)
        full += out
    return full


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Cook Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name = f"Cookimm{_suffix}"
imm_pw = "cookimmpw1234"
make_char(imm_name, imm_pw)
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")

out = cmd(s, "cook")
check("mashed potatoes" in out.lower(), "`cook` with no argument lists the known recipes")

out = cmd(s, "cook potatoes mashed")
check("don't have everything" in out.lower(), "cooking mashed potatoes with no ingredients is refused")

cmd(s, f"load obj {POTATO_VNUM}")
cmd(s, f"load obj {BUTTER_VNUM}")
out = cmd(s, "cook potatoes mashed")
check("you cook up" in out.lower(), "mashed potatoes succeeds once both ingredients are carried")

out = cmd_paged(s, "inventory")
lines = [l.strip().lower() for l in out.splitlines()]
check(not any(l.startswith("a potato") for l in lines), "the potato was consumed")
check(not any("butter" in l for l in lines), "the butter was consumed")
check(any("mashed potatoes" in l for l in lines), "the finished mashed potatoes are now in inventory")

out = cmd(s, "cook potatoes mashed")
check("don't have everything" in out.lower(), "cooking again with the ingredients gone is refused")

# --- corpse-based recipe: fried chicken ---
cmd(s, f"load mob {BIRD_MOB_VNUM}")
out = cmd(s, "kill crow")
check("slain" in out.lower() or "kill" in out.lower(), "the bird is killed, leaving a corpse")
cmd(s, f"load obj {GREASE_VNUM}")
out = cmd(s, "cook chicken fried")
check("you cook up" in out.lower(), "fried chicken succeeds off a real BIRD-race corpse + whale grease")

out = cmd_paged(s, "inventory")
check("fried chicken" in out.lower(), "the finished fried chicken is now in inventory")

announce_done("smoke_test_cook", host, port)
print("=== ALL CHECKS PASSED ===")
