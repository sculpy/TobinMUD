#!/usr/bin/env python3
"""Smoke test for Planting (Sneezy -> Tobin feature audit). Scoped via
AskUserQuestion, 2026-07-26: BOTH mechanics the real `plant` command covers
(user: "Both, same pass"), and the full 15-plant-type seed-farming depth
(user: "Full 15-type system", disclosed as compressed real-time-to-growth-
tick-count -- see obj_plant.h's own doc comment for why years-long real
lifespans were rescaled to tens/hundreds of ~1-real-minute growth ticks).

  1. Seed farming: `plant <seed sack>` outdoors starts a 3-step dig/sow/
     cover task (planting.c's planting_tick_run(), forced via `aitick` --
     the real ~3s-per-step cadence is too slow for an automated test), the
     seed sack is consumed, and a fresh dirt-mound plant object appears.
  2. Growth: forcing many more ticks ages the plant through its stages
     (obj_plant_growth_tick()) into a mature, fruit-yielding state.
  3. Indoors refusal (`room_can_plant()`).
  4. Abort-on-room-change mid-task (the seed sack survives, uneaten).
  5. Thief `plant <item> <victim>`: class-gated (a non-Thief is refused),
     transfers the item into the victim's inventory regardless of the
     stealth roll's outcome (both branches call the transfer -- only the
     flavor message differs), refuses an immortal/self target, and
     refuses a non-consenting PC target (reusing combat.c's own mutual
     `toggle pk` gate).

NOT covered (disclosed, same "not practical to keep automated" precedent
as pet-charm's natural-expiry case): the 8-plants-per-room cap (would need
7 real completed plant tasks just to set up), and a plant actually
withering out of existence (its compressed lifespan is still tens of
real minutes even at the fastest end).

    python3 tests/smoke_test_plant.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_plant")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 910000 + (int(time.time()) % 70000)
ROOM_INDOOR = ROOM + 1
ITEM_VNUM = ROOM + 2
SEED_TOMATO = 13880

WEAR_TAKE = 1


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_caster(name, level):
    # Same "grant discipline percentages directly, keep a real mortal
    # level" precedent as smoke_test_pet.py/smoke_test_transformation.py --
    # avoids the immortal-instant-slay/instant-success shortcuts that would
    # make the Thief plant roll meaningless to test.
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, "
        f"combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def grant_proficiency(name, skill_name, pct=100):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, 0) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


# Defensive cleanup for a prior run that errored out mid-test.
sql(f"DELETE FROM room WHERE vnum IN ({ROOM},{ROOM_INDOOR});")
sql(f"DELETE FROM obj WHERE vnum={ITEM_VNUM};")

imm_name, imm_pw = f"Plntimmb{_suffix}", "plntimmpw1234"
s_imm = make_char(imm_name, imm_pw, "1")
set_level(imm_name, 51)
s_imm.close()
s_imm = relog(imm_name, imm_pw)

# sector=0 (SUBARCTIC) + room_flag=1 (ALWAYS_LIT only, no INDOORS bit) --
# passes room_can_plant() (not indoors, not a water/lava/atmosphere
# keyword), same sandbox-room shape smoke_test_pet.py already uses.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Plant Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# room_flag=9 = ALWAYS_LIT(1) | INDOORS(8).
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_INDOOR},0,0,0,'Plant Sandbox Indoors','A bare indoor room.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM}, 0, '', '', 0, 0, 0, 0, 0, {ROOM_INDOOR});")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM_VNUM},'incriminating letter','an incriminating letter',"
    f"'An incriminating letter lies here.',12,{WEAR_TAKE},1);")


def give_from_imm(sock, vnum, keyword):
    check("You conjure" in cmd(s_imm, f"load obj {vnum}"), f"immortal loads {keyword}")
    cmd(s_imm, f"drop {keyword}")
    out = cmd(sock, f"get {keyword}")
    check("you get" in out.lower(), f"caster picks up {keyword}")


# ============================================================
# Phase 1-2: Seed farming -- plant, grow, yield fruit
# ============================================================
gard_name, gard_pw = f"Plntgab{_suffix}", "plntgapw1234"
sg = make_char(gard_name, gard_pw, "3")  # Warrior -- no class gate on seed farming
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{gard_name}';")
cmd(sg, "quit!")
sg.close()
sg = relog(gard_name, gard_pw)
cmd(s_imm, f"goto {ROOM}")

give_from_imm(sg, SEED_TOMATO, "seeds")
check("tomato" in cmd(sg, "inventory").lower(), "gardener is carrying the tomato seed sack")

out = cmd(sg, "plant seeds")
check("begin to plant" in out.lower(), "starting `plant <seeds>` prints the begin message")

# Force the 3-step dig/sow/cover task to completion.
cmd(s_imm, "aitick 3")
out = cmd(sg, "look")
check("mound of" in out.lower() and "dirt" in out.lower(), "a fresh dirt-mound plant appears after the task completes")
check("tomato" not in cmd(sg, "inventory").lower(), "the seed sack was consumed by the task")

# Force enough growth ticks to reach maturity (mean age ~2/tick, mature at
# age>=30 -- 40 ticks gives a wide safety margin) and very likely yield at
# least one fruit (25%/tick while mature, well over a dozen mature ticks).
cmd(s_imm, "aitick 40")
out = cmd(sg, "look")
check("tomato" in out.lower() and ("plant" in out.lower() or "vine" in out.lower()), "the plant has grown into a recognizable tomato plant")
check("tomato" in cmd(sg, "look").lower(), "look still shows the tomato plant/fruit on the ground")


# ============================================================
# Phase 3: Indoors refusal
# ============================================================
# `transfer` (not quit!/relog) -- quit! deliberately drops all carried
# items on the floor (documented existing behavior, cmd_quit.c), which
# would strip the gardener of the very seed sack this phase needs intact.
give_from_imm(sg, SEED_TOMATO, "seeds")
cmd(s_imm, f"transfer {gard_name} {ROOM_INDOOR}")
out = cmd(sg, "plant seeds")
check("can't plant anything here" in out.lower(), "planting indoors is refused")


# ============================================================
# Phase 4: Abort on room change mid-task
# ============================================================
cmd(s_imm, f"transfer {gard_name} {ROOM}")
out = cmd(sg, "plant seeds")
check("begin to plant" in out.lower(), "gardener starts a second planting task")
cmd(sg, "north")
cmd(s_imm, "aitick 1")
out = cmd(sg, "")
check("moved on" in out.lower() or "stop planting" in out.lower(), "moving away aborts the in-progress task")
check("tomato" in cmd(sg, "inventory").lower(), "the seed sack survives an aborted task, uneaten")


# ============================================================
# Phase 5: Thief `plant <item> <victim>`
# ============================================================
thief_name, thief_pw = f"Plnttfb{_suffix}", "plnttfpw1234"
vict_name, vict_pw = f"Plntvcb{_suffix}", "plntvcpw1234"
st = make_char(thief_name, thief_pw, "4")  # Thief
sv = make_char(vict_name, vict_pw, "1")    # Mage
for name in (thief_name, vict_name):
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
cmd(st, "quit!"); st.close()
cmd(sv, "quit!"); sv.close()
set_caster(thief_name, 35)
grant_proficiency(thief_name, "plant")
st = relog(thief_name, thief_pw)
sv = relog(vict_name, vict_pw)
cmd(s_imm, f"goto {ROOM}")

# Non-Thief class gate.
give_from_imm(sv, ITEM_VNUM, "letter")
out = cmd(sv, f"plant letter {thief_name}")
check("know nothing about planting" in out.lower(), "a non-Thief is refused plant-on-victim")
cmd(sv, "drop letter")
cmd(sv, "get letter")

# Self-target refusal.
give_from_imm(st, ITEM_VNUM, "letter")
out = cmd(st, f"plant letter {thief_name}")
check("rather stupid" in out.lower(), "planting on yourself is refused")

# No-consent refusal (victim hasn't toggled pk).
out = cmd(st, f"plant letter {vict_name}")
check("toggle pk" in out.lower(), "planting on a non-consenting player is refused")

# Both consent -- item transfers regardless of the stealth roll's outcome.
cmd(st, "toggle pk")
cmd(sv, "toggle pk")
out = cmd(st, f"plant letter {vict_name}")
check("letter" in out.lower(), "planting on a consenting victim prints a result either way")
check("letter" in cmd(sv, "inventory").lower(), "the planted item is now in the victim's own inventory")

announce_done("smoke_test_plant")
print("=== ALL CHECKS PASSED ===")
