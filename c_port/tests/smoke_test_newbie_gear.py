#!/usr/bin/env python3
"""Smoke test for newbie equipment suits (user 2026-07-26: "6 sets of
newbie equipment to load on the character when connecting for the first
time... a shield and a weapon based upon class choice", plus the
Grimhaven/Tobin City Welfare Department social worker's reissue and the
`loadsuit` immortal command). New `suit`/`suit_item` tables (db/tobin/
suit.sql), suit_repo.h/.c + suit.h/.c, wired into player_create()
(player_repo.c), cmd_loadsuit.c, and cmd_say.c's SPEC_PROC_NEWBIE_EQUIPPER
dispatch (mob vnum 90, room 570). Covers:

  1. A fresh Warrior character's starting inventory has exactly the
     warrior_newbie suit's 4 items (weapon, shield, torch, backpack),
     loose (not auto-equipped -- user: "they can hold the items
     themselves, just load into inventory").
  2. `loadsuit <name>` (immortal) loads a suit onto the caller.
  3. `loadsuit <name> <target>` loads it onto someone else in the room,
     who gets their own notice.
  4. `loadsuit <bogus name>` reports no match, doesn't crash.
  5. The Welfare Department social worker (room 570) reissues a lost
     suit when asked ("say gear"), matching the speaker's own class --
     and stays silent for unrelated speech.

    python3 tests/smoke_test_newbie_gear.py [host] [port]
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


announce("smoke_test_newbie_gear")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0, idle_gap=0.3):
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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Nwgimm{_suffix}", "nwgimmpw123"
war_name, war_pw = f"Nwgwar{_suffix}", "nwgwarpw123"
tgt_name, tgt_pw = f"Nwgtgt{_suffix}", "nwgtgtpw123"
mnk_name, mnk_pw = f"Nwgmnk{_suffix}", "nwgmnkpw123"

sockets = []

try:
    make_char(imm_name, imm_pw, "3").close()
    sql(f"UPDATE player_progress SET level=56 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")

    # --- 1: a fresh Warrior gets exactly the warrior_newbie suit ---
    war = make_char(war_name, war_pw, "3")
    sockets.append(war)
    cmd(war, "color off")
    out = cmd(war, "inventory")
    check("training shield" in out.lower(), "fresh Warrior's inventory has the shield")
    check("training sword" in out.lower(), "fresh Warrior's inventory has the class weapon")
    check("torch" in out.lower(), "fresh Warrior's inventory has the torch")
    check("backpack" in out.lower(), "fresh Warrior's inventory has the backpack")
    out = cmd(war, "equipment")
    check("nothing" in out.lower() and "training" not in out.lower(),
          "the suit items are loose, not auto-equipped")

    make_char(tgt_name, tgt_pw, "1").close()  # Mage target for loadsuit

    imm = login(imm_name, imm_pw)
    sockets.append(imm)
    tgt = login(tgt_name, tgt_pw)
    sockets.append(tgt)

    check("Center Square" in cmd(imm, "goto 100"), "immortal reaches Center Square")
    cmd(imm, f"transfer {war_name}")
    cmd(imm, f"transfer {tgt_name}")
    for s in (war, tgt):
        recv_all(s)

    # --- 2/3: loadsuit onto self and onto a named target ---
    out = cmd(imm, "loadsuit cleric_newbie")
    check("cleric_newbie" in out and "yourself" in out, "loadsuit onto self reports correctly")
    out = cmd(imm, f"loadsuit druid_newbie {tgt_name}")
    check("druid_newbie" in out and tgt_name in out, "loadsuit onto a named target reports correctly")
    tgt_out = recv_all(tgt, 1.0)
    check("outfits you" in tgt_out.lower(), "the target gets their own notice")
    tgt_inv = cmd(tgt, "inventory")
    check("sickle" in tgt_inv.lower(), "the target actually received the druid suit's weapon")

    # --- 4: a bogus suit name doesn't crash, just reports no match ---
    out = cmd(imm, "loadsuit not_a_real_suit_xyz")
    check("no suit matches" in out.lower(), "a bogus suit name is refused cleanly")

    # --- 5: the Welfare Department social worker reissues on request ---
    mnk = make_char(mnk_name, mnk_pw, "6")
    sockets.append(mnk)
    cmd(mnk, "color off")
    cmd(imm, "goto 570")
    cmd(imm, f"transfer {mnk_name}")
    recv_all(mnk)

    cmd(mnk, "drop all")
    check("nothing" in cmd(mnk, "inventory").lower(), "the Monk's suit is dropped, inventory empty")

    out = cmd(mnk, "say gear")
    check("hands you a fresh set of gear" in out.lower(), "asking for gear reissues the suit")
    out = cmd(mnk, "inventory")
    check("nunchaku" in out.lower(), "the reissued suit is the Monk's own class suit again")

    cmd(mnk, "drop all")
    out = cmd(mnk, "say hello there")
    check("hands you" not in out.lower(), "unrelated speech doesn't trigger a reissue")
    check("nothing" in cmd(mnk, "inventory").lower(), "still empty after unrelated speech")

    for s in sockets:
        s.close()
    sockets = []

    announce_done("smoke_test_newbie_gear")
    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            s.close()
        except OSError:
            pass
    name_list = f"'{imm_name}','{war_name}','{tgt_name}','{mnk_name}'"
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ({name_list}));")
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ({name_list}));")
    sql(f"DELETE FROM player WHERE name IN ({name_list});")
