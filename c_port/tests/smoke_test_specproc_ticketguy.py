#!/usr/bin/env python3
"""Smoke test for SPEC_TICKET_GUY (spec_mobs.cc's `TicketGuy`), the
fifth proc ported under the spec-proc project (SPEC_PROCS.md). Id 51
lives in the REAL `mob_specials[]` registration array, not the sparse
named-constant list in spec_mobs.h. Real seeded mobs already carry
`spec_proc=51` (vnum 69 "student exiled mage" among others).

Ported behavior: `buy ticket` from a matching mob deducts 1000 gold and
teleports the buyer to a fixed destination room (upstream's
Room::TICKET_DESTINATION=6969, confirmed to map onto a real imported
Tobin room, "The Arrivals Circle") -- cmd_shop.c's cmd_buy_ticket(),
checked before the normal shop-catalog gate since a ticket-guy mob has
no real shop of its own.

Covers:
  1. Standing + enough gold: `buy ticket` succeeds, gold is deducted,
     and the buyer lands in the destination room.
  2. Not standing (sitting): refused with the "stand on your own feet"
     message, no gold deducted.
  3. Not enough gold: refused with the price message, no gold deducted.
  4. An ordinary mob (no matching spec_proc) in a real shop: `buy
     ticket` falls through to the normal shop flow (item not found),
     not the ticket path.

    python3 tests/smoke_test_specproc_ticketguy.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_specproc_ticketguy", host, port)


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


imm_name, imm_pw = f"Stg{_suffix}", "stgpw1234567"
p_name, p_pw = f"Stgp{_suffix}", "stgppw123456"
ROOM = 957000 + (int(time.time()) % 40000)

s1 = make_char(imm_name, imm_pw, 3)  # Warrior (level 51+ needed for `load`/`goto`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s1 = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Ticket Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Ticket Sandbox" in cmd(s1, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sp = make_char(p_name, p_pw, 3)
cmd(sp, "quit!"); sp.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{p_name}';")
# Start poor -- 500 gold, less than the 1000 price -- so scenarios 2/3 below
# need no mid-session gold mutation (a live character's in-memory progress_t
# doesn't pick up a DB-side UPDATE without a fresh relog, same gotcha other
# tests in this suite have hit).
sql(f"UPDATE player_progress SET gold=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{p_name}');")
sp = relog(p_name, p_pw)
check("Ticket Sandbox" in cmd(sp, "look"), "the buyer lands in the sandbox room")

check("You conjure" in cmd(s1, "load mob 69"), "a real ticket-guy mob is loaded")

# --- 2: sitting is refused ---
cmd(sp, "sit")
out = cmd(sp, "buy ticket")
check("stand on your own feet" in out.lower(), "buying a ticket while sitting is refused")
out = cmd(sp, "score")
check("500" in out, "gold is untouched after the sitting refusal")
cmd(sp, "stand")

# --- 3: not enough gold ---
out = cmd(sp, "buy ticket")
check("1000 talens" in out.lower(), "buying a ticket without enough gold is refused with the price")
out = cmd(sp, "score")
check("500" in out, "gold is untouched after the too-poor refusal")

# --- 1: a real purchase succeeds and teleports the buyer -- top up gold via
# a relog (not a live mid-session SQL mutation, see the note above) ---
cmd(sp, "quit!"); sp.close()
sql(f"UPDATE player_progress SET gold=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{p_name}');")
sp = relog(p_name, p_pw)
check("Ticket Sandbox" in cmd(sp, "look"), "the buyer is back in the sandbox room after the gold top-up relog")
out = cmd(sp, "buy ticket")
check("you buy a ticket" in out.lower(), "buy ticket succeeds with enough gold")
check("another plane of existence" in out.lower(), "the flavor text plays")
check("Arrivals Circle" in out, "the buyer's own look shows the destination room")
out = cmd(sp, "score")
check("4000" in out, "1000 gold was deducted")

# --- 4: no ticket-guy in the (real, non-sandbox) destination room -- falls
# through to the normal shop flow instead of re-triggering the ticket path ---
out = cmd(sp, "buy ticket")
check("don't see a shop" in out.lower(), "buying a ticket with no ticket-guy present falls through to the normal shop refusal")

sp.close()
s1.close()

announce_done("smoke_test_specproc_ticketguy", host, port)
print("=== ALL CHECKS PASSED ===")
