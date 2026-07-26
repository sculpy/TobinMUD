#!/usr/bin/env python3
"""Smoke test for "Magic items" (Sneezy -> Tobin feature audit, full
system: equipment stat/AC affects + wands/scrolls/staves). Covers:

  1. A real equipped item (ring, vnum 179) with a real `objaffect` AC row
     raises total Armor Class on wear and reverts exactly on remove --
     regression check for a bug caught during manual verification: AC
     only applied to OBJ_CAT_ARMOR-category items, silently dropping the
     bonus for rings/jewelry/other worn slots that also carry real AC
     data (obj_armor_ac() now applies real aff_ac data regardless of
     category).
  2. A real equipped item (token of the deities, vnum 2) with a real HIT
     (max HP) affect raises max HP on wear and reverts on remove.
  3. `use <wand>` on a target consumes one charge, hits the target, and
     exhausting all charges gives a "no charges left" message.
  4. `use <staff>` is a room-wide area effect (mirrors cast/pray's
     area-effect handling) and also consumes a charge.
  5. `use <scroll>` applies its effect once and destroys the scroll
     (crumbles to dust, vanishes from inventory) -- single-use, unlike
     wands/staves.

    python3 tests/smoke_test_magic_items.py [host] [port]
"""
import re
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


announce("smoke_test_magic_items")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 60000)

# Real, already-seeded upstream vnums with real objaffect rows (verified by
# querying the live DB before writing this test, same as the sign-flip
# discovery during development):
#   179 = "ring wood small simple", type 11 (ARMOR) mod1=-5 -> +5 AC (Tobin
#         convention, sign-flipped on import)
#   2   = "token deities", type 12 (HIT) mod1=25 -> +25 max HP
RING_VNUM = 179
TOKEN_VNUM = 2
RING_AC_BONUS = 5
TOKEN_HP_BONUS = 25

# New Tobin-owned magic items (db/sneezy/obj_magic.sql):
WAND_VNUM = 90000     # wand of gusts, "gust", 5 charges, targeted
STAFF_VNUM = 90001    # staff of fireball, "fireball", 3 charges, room-wide
SCROLL_VNUM = 90002   # scroll of minor healing, "heal light", single-use


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


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def ac_of(score_out):
    m = re.search(r"Armor Class:\s+(-?\d+)", score_out)
    return int(m.group(1)) if m else None


def max_hp_of(score_out):
    m = re.search(r"HP:\s+\d+ \((\d+) Max", score_out)
    return int(m.group(1)) if m else None


imm_name = f"Magimma{_suffix}"
b1_name = f"Magbysa{_suffix}"
pw = "magicitempw123"

try:
    make_char(imm_name, pw, "1")   # class doesn't matter, immortal bypasses class gates
    make_char(b1_name, pw, "3")
    set_level(imm_name, 51)   # needs goto/load, immortal-only
    set_level(b1_name, 51)    # just needs to be a reachable, damageable target

    imm = login(imm_name, pw)
    b1 = login(b1_name, pw)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Magic Item Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    check("Magic Item Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the sandbox room")
    cmd(b1, f"goto {ROOM}")
    recv_all(imm); recv_all(b1)  # drain arrival notices

    # --- 1: equipment AC affect (ring, real objaffect data) ---
    before_ac = ac_of(cmd(imm, "score"))
    cmd(imm, f"load obj {RING_VNUM}")
    cmd(imm, "get ring")
    cmd(imm, "wear ring")
    worn_ac = ac_of(cmd(imm, "score"))
    check(worn_ac == before_ac + RING_AC_BONUS,
          f"wearing a real-data ring raises AC by {RING_AC_BONUS} ({before_ac} -> {worn_ac}) -- "
          "regression check: AC affects used to be silently dropped for non-armor-category "
          "worn items like rings")
    cmd(imm, "remove ring")
    after_ac = ac_of(cmd(imm, "score"))
    check(after_ac == before_ac, f"removing the ring reverts AC back to {before_ac} (got {after_ac})")

    # --- 2: equipment HP affect (token of the deities, real objaffect data) ---
    before_hp = max_hp_of(cmd(imm, "score"))
    cmd(imm, f"load obj {TOKEN_VNUM}")
    cmd(imm, "get token")
    cmd(imm, "wear token")
    worn_hp = max_hp_of(cmd(imm, "score"))
    check(worn_hp == before_hp + TOKEN_HP_BONUS,
          f"wearing a real-data token raises max HP by {TOKEN_HP_BONUS} ({before_hp} -> {worn_hp})")
    cmd(imm, "remove token")
    after_hp = max_hp_of(cmd(imm, "score"))
    check(after_hp == before_hp, f"removing the token reverts max HP back to {before_hp} (got {after_hp})")

    # --- 3: wand -- targeted, charge-limited ---
    cmd(imm, f"load obj {WAND_VNUM}")
    cmd(imm, "get wand")
    out = cmd(imm, f"use wand {b1_name}")
    check("You use a wand of gusts" in out, "using a wand invokes its stored spell")
    out_b1 = recv_all(b1, timeout=1.0)
    check(len(out_b1) > 0, "the wand's target actually sees something happen")
    # Wand seeded with 5 charges; one already spent above -- burn the rest.
    for _ in range(4):
        cmd(imm, f"use wand {b1_name}")
    out = cmd(imm, f"use wand {b1_name}")
    check("no charges left" in out, "a wand with all charges spent refuses further use")

    # --- 4: staff -- room-wide area effect, also charge-limited ---
    cmd(imm, f"load obj {STAFF_VNUM}")
    cmd(imm, "get staff")
    out = cmd(imm, "use staff")
    check("catches everyone nearby" in out, "using a staff announces a room-wide area effect")
    out_b1 = recv_all(b1, timeout=1.0)
    check("catches you" in out_b1, "the staff's area effect actually reaches a bystander in the room")

    # --- 5: scroll -- single use, destroyed after ---
    cmd(imm, f"load obj {SCROLL_VNUM}")
    cmd(imm, "get scroll")
    out = cmd(imm, "use scroll")
    check("heals you" in out, "using a scroll applies its stored effect (heal)")
    check("crumbles to dust" in out, "the scroll announces its own destruction")
    inv = cmd(imm, "inventory")
    check("scroll" not in inv.lower(), "the used scroll is actually gone from inventory, not just cosmetically destroyed")

    imm.close()
    b1.close()

    announce_done("smoke_test_magic_items")
    print("=== ALL CHECKS PASSED ===")
finally:
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{b1_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{b1_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
