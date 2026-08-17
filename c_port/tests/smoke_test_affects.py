#!/usr/bin/env python3
"""Smoke test for the Affects system (buffs/debuffs/status) -- user
2026-07-11's backlog item, self-sequenced after trap mechanics. New
generic active_affect_t infrastructure (affect.h/affect.c), proven with
one real flagship effect: the Cleric spell "sanctuary" ("a strong aura
that reduces incoming damage") now actually halves damage taken via
combat_strike(), instead of falling into the placeholder "isn't
implemented yet" branch every other still-unimplemented spell hits.
Covers:

  1. A fresh character's `affects` shows "(none)".
  2. `pray sanctuary` applies it -- `affects` then shows "Sanctuary"
     with a round count.
  3. HP loss under sustained attack over a fixed round window is
     meaningfully lower with Sanctuary active than without (immortal
     attacker so the mortal 1.2s post-swing cooldown doesn't slow
     this down).
  4. Sanctuary wears off on its own after enough rounds, reporting
     "Your Sanctuary wears off." and leaving `affects` empty again.

    python3 tests/smoke_test_affects.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

# COMBAT_ROUND_PULSES(12) * OPT_USEC(100ms) -- include/pulse.h, src/game_loop.c.
COMBAT_ROUND_SECS = 1.2


announce("smoke_test_affects", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)
SYMBOL = ROOM + 1
# Two separate round-count constants, not one shared value, because they
# have very different constraints: the BASELINE phase runs before
# Sanctuary is even cast, so it can use a generous window for a stable
# HP-loss-per-round estimate; the WITH-SANCTUARY phase is capped by the
# buff's own 12-round duration (~14.4s) -- sampling longer than that
# window would silently mix in post-expiry (unbuffed) rounds and corrupt
# the comparison. Kept well under 12 to leave slack for round-boundary
# timing slop and to still have a few rounds left afterward for the
# natural-expiry check (item 4) to observe.
BASELINE_ROUNDS = 10
SANCTUARY_ROUNDS = 8


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    # `timeout` is a hard deadline (never wait longer than this in total);
    # `idle_gap` is how long a genuine quiet moment has to last before
    # treating the response as complete. Live-diagnosed 2026-07-18: the
    # old version used `timeout` as a PER-RECV idle-gap, which meant every
    # call burned the FULL timeout even for an instant reply (the first
    # recv() gets the data right away, but the loop always tries a SECOND
    # recv() that then has nothing left to read and just sits out the
    # whole window) -- confirmed by direct instrumentation showing every
    # step taking ~1.5s flat regardless of actual response size. Shrinking
    # the per-recv wait to a short idle_gap (comfortably larger than
    # realistic same-response multi-packet TCP jitter on localhost, ~tens
    # of ms) while keeping `timeout` as an outer safety cap fixes both the
    # constant-tax problem AND, as a side effect, keeps this responsive
    # during ONGOING combat: round messages arrive every ~1.2s
    # (COMBAT_ROUND_PULSES), which is now LARGER than idle_gap, so a
    # single call naturally returns after one round's output instead of
    # (in the worst case, if packet timing ever lined up just right)
    # never seeing a gap wide enough to stop.
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp):
    # Mutual combat (combat_process_run() strikes both ways every round)
    # means BOTH sides need enough overall HP -- and, since a fresh
    # player_load() now recomputes limb HP as a share of max_hp (fixed
    # 2026-07-12, player_repo.c), a big set_hp here also gives each limb
    # real toughness once the character reconnects (quit!-then-relogin,
    # not a raw socket close -- see smoke_test_weapon_depth.py's set_hp).
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_dex(name, dex):
    # A huge DEX gap clamps the OTHER side's to-hit modifier to its -44
    # floor (~6% hit chance) -- used here so the Cleric's automatic
    # retaliation essentially never lands on the immortal attacker,
    # keeping the attacker alive for the whole sampling window.
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def hp_of(sock):
    # Combat messages carry NO raw damage number anymore (user 2026-07-12,
    # follow-up: "take out the damage number and use it to describe how
    # hard the hit was" -- combat.c's describe_dam() now emits a
    # qualitative word like "pathetically"/"very lightly" instead, for
    # BOTH mortals and immortals -- damages_from()'s old "for N damage"
    # regex can no longer match anything at all). `score` still reports
    # the real live HP directly from the being_t in memory, so HP loss
    # over a fixed round window is used as the measurable signal instead.
    out = cmd(sock, "score")
    m = re.search(r"HP:\s*(-?\d+)\s*/", out)
    return int(m.group(1)) if m else None


def hp_loss_over(attacker_sock, target_sock, target_name, rounds):
    # Returns (hp_lost, seen) -- `seen` is whatever text arrived on the
    # TARGET's own connection during the window, so a caller can check it
    # for a "wears off" line without a separate poll (Sanctuary's 12-round
    # duration can expire mid-window, same reasoning the old
    # average_incoming() had for returning its own `seen` text).
    cmd(attacker_sock, f"hit {target_name}")
    before = hp_of(target_sock)
    check(before is not None, "read the target's starting HP via score")
    time.sleep(rounds * COMBAT_ROUND_SECS)
    recv_all(attacker_sock, 0.3)  # drain buffered combat spam so it doesn't
    seen = recv_all(target_sock, 0.3)  # bleed into the next command's response
    after = hp_of(target_sock)
    check(after is not None, "read the target's ending HP via score")
    return before - after, seen


pw = "affectspw123"

# --- Attacker immortal, set up first (immortals can `goto`/`load obj`;
#     also gets the immortal damage-immunity treatment, 2026-07-12: "an
#     immortal character shouldnt be damaged by hits in a fight" --
#     combat_strike() now zeroes damage against an immortal DEFENDER, so
#     the Cleric below (whose incoming damage this test measures) has to
#     be an ordinary MORTAL instead of immortal like earlier revisions of
#     this test used; a huge set_dex() keeps their retaliation from ever
#     landing on the attacker anyway, and now any retaliation that DID
#     land would deal zero damage regardless, since the attacker here is
#     immortal). ---
imm_name = f"Affimm{_suffix}"
imm_pw = "affimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)  # territory: urban
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 51)
set_hp(imm_name, 2000)
set_dex(imm_name, 900)  # the Cleric's automatic retaliation should essentially never land
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Affects Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Affects Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")
# `load obj` lands the item in the LOADING immortal's own inventory, not
# the room floor (documented gap, STATUS.md 2026-07-22) -- drop it back
# onto the floor explicitly so the Cleric below can actually `get` it,
# same workaround tests/smoke_test_drugs.py already uses.
cmd(s_imm, "drop symbol")

# --- Cleric MORTAL (a real defender now that immortals take zero damage)
#     -- basic_disc_pct/advanced_disc_pct set directly via SQL to satisfy
#     sanctuary's Advanced-tier discipline gate without immortal status;
#     level bumped to 25 too (2026-07-18 level-curve rescale: Advanced-
#     tier min_level now spans 25-50 per class, not universally 1). ---
cleric_name = f"Affcle{_suffix}"
cleric_pw = "affclepw123"
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, "y"); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "new"); recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)  # territory: urban
send_line(s_cle, "2"); recv_all(s_cle)  # class: cleric
send_line(s_cle, "done"); recv_all(s_cle)
send_line(s_cle, "done"); recv_all(s_cle)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(s_cle, "quit!")
s_cle.close()
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=50 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cleric_name}');")
set_level(cleric_name, 25)
# The discipline-percentage gate above is separate from "sanctuary"'s OWN
# per-skill proficiency (learn-by-doing, added 2026-07-17 -- postdates
# this test): a fresh character starts at the 1% floor, so `pray
# sanctuary` would fumble ~99% of the time without this. Seeded straight
# to 100%, same "bypass via direct SQL" spirit as the discipline_pct
# lines above -- this test is about the AFFECT mechanic, not proficiency
# gain. Live-diagnosed 2026-07-18 after a real fumble consumed the
# caster's only holy symbol without ever attempting the prayer's effect.
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"SELECT id, 'sanctuary', 100, 0 FROM player WHERE name='{cleric_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
set_hp(cleric_name, 8000)  # survive a long mutual-combat sampling window even
                            # if a few unlucky retaliation hits concentrate on
                            # one limb (Tobin's death check is per-limb, not
                            # the aggregate pool -- 2000 occasionally wasn't
                            # enough headroom, live-observed 2026-07-18)
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
cmd(s_cle, "color off")
check("Affects Sandbox" in cmd(s_cle, "look"), "the Cleric lands in the sandbox room")

out = cmd(s_cle, "get symbol")
check("you get" in out.lower(), "the Cleric picks up the holy symbol")

# --- 1: a fresh character's affects list is empty ---
out = cmd(s_cle, "affects")
check("(none)" in out, "affects shows (none) before casting anything")

# --- 3a: baseline HP-loss rate, no Sanctuary yet ---
baseline_loss, _ = hp_loss_over(s_imm, s_cle, cleric_name, BASELINE_ROUNDS)
check(baseline_loss > 0, f"the immortal's sustained attack does real damage before Sanctuary ({baseline_loss} HP)")
baseline_rate = baseline_loss / BASELINE_ROUNDS

# --- 2: pray sanctuary applies the affect ---
out = cmd(s_cle, "pray sanctuary")
check("shimmering aura surrounds you" in out, "pray sanctuary applies the affect")
out = cmd(s_cle, "affects")
check("Sanctuary" in out, "affects now lists Sanctuary")
m = re.search(r"Sanctuary\s+(\d+) round", out)
check(m is not None and int(m.group(1)) > 0, "affects shows a positive round count for Sanctuary")

# --- 3b: HP-loss rate drops noticeably with Sanctuary active ---
sanctuary_loss, seen_during_sampling = hp_loss_over(s_imm, s_cle, cleric_name, SANCTUARY_ROUNDS)
sanctuary_rate = sanctuary_loss / SANCTUARY_ROUNDS
check(sanctuary_rate < baseline_rate * 0.8,
      f"Sanctuary's damage reduction is clearly visible "
      f"({baseline_rate:.2f} HP/round -> {sanctuary_rate:.2f} HP/round)")

# --- 4: Sanctuary wears off on its own -- its 12-round duration can
#     expire mid-sampling above, so check the accumulated sampling text
#     too, not just a fresh poll after the fact. ---
out = seen_during_sampling
waited = 0
while "wears off" not in out and waited < 20:
    out += recv_all(s_cle, 1.5)
    waited += 1
check("Your Sanctuary wears off" in out, "Sanctuary reports wearing off on its own")
out = cmd(s_cle, "affects")
check("(none)" in out, "affects is empty again after Sanctuary expires")

s_cle.close()
s_imm.close()
announce_done("smoke_test_affects", host, port)
print("=== ALL CHECKS PASSED ===")
