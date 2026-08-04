#!/usr/bin/env python3
"""Smoke test for SPEC_BEGGAR (spec_mobs.cc's `beggar`), the second proc
ported under the spec-proc project (SPEC_PROCS.md). Found via a scoping
correction this session: id 17 lives in the REAL `mob_specials[]`
registration array (spec_mobs.cc), not the sparse named-constant list in
spec_mobs.h -- most of that array's 222 slots have no named constant at
all and are only reachable by array position/id. Real seeded mobs already
carry `spec_proc=17` (vnum 601 "a beggar" among others), confirming this
is genuine importable content, not fabricated for the test.

Ported behavior: `give` an item or coins to a beggar mob and it reacts --
a flat thanks for an item, or one of several coin-amount-tiered reactions
for coins (mob_ai.c's mob_spec_beggar_given_item()/given_coins(), wired
into cmd_object.c's `give` right after a mob recipient successfully
receives something).

Covers:
  1. Giving an item to a beggar produces its thanks line.
  2. A small coin amount (<50) produces the low-tier reaction.
  3. A mid coin amount (500-999) produces the "Wow. Thanks!" tier.
  4. A large coin amount (1000-9999) produces the amazed tier.
  5. A huge coin amount (10000-99999) produces the stunned-silent tier.
  6. Giving coins/items to an ordinary mob with no matching spec_proc
     produces no reaction at all (no false-positive firing).

    python3 tests/smoke_test_specproc_beggar.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_specproc_beggar", host, port)


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


name1, pw1 = f"Sbg{_suffix}", "sbgpw1234567"
ROOM = 951000 + (int(time.time()) % 40000)

s1 = make_char(name1, pw1, 3)  # Warrior (level 51+ needed for `load`)
cmd(s1, "quit!"); s1.close()

sql(f"UPDATE player_progress SET level=59, gold=100000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name1}');")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Beggar Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name1}';")

s1 = relog(name1, pw1)

# --- 1: item given to a real beggar mob (vnum 601, spec_proc=17) ---
# The mob's reaction (a room echo, mobs have no descriptor to notify
# directly) arrives synchronously in the SAME response burst as `give`'s
# own confirmation -- one `cmd()` call, not a separate later read.
cmd(s1, "load mob 601")
cmd(s1, "load obj 403")  # a standard ration
out = cmd(s1, "give ration beggar")
check("you give" in out.lower(), "the ration is handed over")
check("pawn that off" in out.lower(), "the beggar thanks the giver for an item")

# --- 2-5: coin-amount tiers ---
out = cmd(s1, "give 30 gold beggar")
check("strain yourself" in out.lower(), "a small coin amount gets the low-tier reaction")

out = cmd(s1, "give 500 gold beggar")
check("wow. thanks" in out.lower(), "a mid coin amount gets the 'Wow. Thanks!' tier")

out = cmd(s1, "give 5000 gold beggar")
check("utterly amazed" in out.lower(), "a large coin amount gets the amazed tier")

out = cmd(s1, "give 50000 gold beggar")
check("too stunned to speak" in out.lower(), "a huge coin amount gets the stunned-silent tier")

# --- 6: an ordinary mob (no matching spec_proc) gives no reaction ---
cmd(s1, "load mob 10")  # vnum 10, "a vrock demon" -- confirmed spec_proc=0
out = cmd(s1, "give 500 gold demon")
check("you give" in out.lower(), "the gold is handed over to the demon")
check("says" not in out.lower(),
      "a mob with no matching spec_proc reacts with nothing extra")

s1.close()

announce_done("smoke_test_specproc_beggar", host, port)
print("=== ALL CHECKS PASSED ===")
