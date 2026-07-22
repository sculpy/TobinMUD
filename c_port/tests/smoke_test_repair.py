#!/usr/bin/env python3
"""Smoke test for the repair-shop economy (Object maintenance tasks 3-4,
Sneezy -> Tobin feature audit, "full system" scope). Covers:

  1. `repair` without the skill fails with the right refusal message.
  2. `repair` without enough gold refuses (materials cost gold up front).
  3. A 100%-proficiency `repair` succeeds: cur_struct is restored to
     max_struct - depreciation, depreciation increments by 1, and the
     item is monogrammed with the repairer's display name.
  4. Persistence: the repaired item's cur_struct/depreciation/monogram
     round-trip through `save` into player_inventory (closing the
     latent Session-55 gap where structure damage was never persisted).
  5. The full shop flow at the real seeded repair shop (shop_nr 134,
     "Blacksmith's Forge", room 7110): `submit` creates a DB-backed
     ticket and destroys the carried item, `tickets` lists it, and
     `retrieve` (after confirming it refuses without enough gold) pays
     up, hands back a freshly-reconstructed item with depreciation
     carried forward, and clears the ticket.

    python3 tests/smoke_test_repair.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

REPAIR_ROOM = 7110
REPAIR_SHOP_NR = 134


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


announce("smoke_test_repair")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
HELM_VNUM = 960000 + (int(time.time()) % 30000)


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


def sql_out(stmt):
    r = subprocess.run(["mariadb", "sneezy", "-e", stmt], capture_output=True, text=True, check=True)
    return r.stdout


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_choice, "done", "2"):
        send_line(s, step); recv_all(s)
    cmd(s, "quit!")
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Rpaimma{_suffix}"
pc_name = f"Rpapca{_suffix}"
pw = "repairtestpw1"

try:
    # quit!-then-SQL ordering: make_char()/quit! must land BEFORE any SQL
    # row edits, or player_save()'s own stale in-memory write clobbers them.
    make_char(imm_name, pw, "1")
    make_char(pc_name, pw, "3")   # Warrior -- "repair" is CLASS_WARRIOR/level 5
    sql(f"UPDATE player_progress SET level=59 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    # basic_disc_pct is left at its creation default (0) here on purpose --
    # being_knows_skill() gates any SKILL_TIER_CLASS skill on the
    # class/level/discipline-pct trio alone, NOT on a player_skill row
    # existing at all (that row only drives skill_roll_success()'s
    # proficiency roll). So "hasn't learned repair" has to mean "hasn't
    # unlocked the Warrior discipline tier yet", granted further down.
    sql(f"UPDATE player_progress SET level=10, gold=0 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{pc_name}');")

    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"can_be_seen,max_struct,cur_struct,decay) VALUES "
        f"({HELM_VNUM},'helm repair test','a dented test helm',"
        f"'A dented test helm is lying here.',11,25,1,10,4,-1);")

    imm = login(imm_name, pw)
    pc = login(pc_name, pw)

    check("Blacksmith" in cmd(imm, f"goto {REPAIR_ROOM}"),
          "goto lands the immortal at the real Blacksmith's Forge repair shop")
    cmd(imm, f"transfer {pc_name} {REPAIR_ROOM}")
    recv_all(pc); recv_all(imm)  # drain arrival notices

    # --- 1: repair without the skill ---
    # `load` now puts the item straight into the loading immortal's own
    # inventory (2026-07-22), not the room floor -- drop it explicitly so
    # `pc` (a different character) can pick it up, same as the old
    # room-drop behavior used to do implicitly.
    cmd(imm, f"load obj {HELM_VNUM}")
    cmd(imm, "drop helm")
    cmd(pc, "get helm")
    out = cmd(pc, "repair helm")
    check("don't know how to repair" in out.lower(),
          "repair refuses a character who hasn't learned the skill")

    # --- 2: repair without enough gold ---
    sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{pc_name}');")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{pc_name}'), 'repair', 100, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct=100;")
    # Reconnect so the freshly-granted skill is loaded into the live session.
    pc.close()
    pc = login(pc_name, pw)
    out = cmd(pc, "repair helm")
    check("gold in makeshift materials" in out,
          "repair refuses when the player can't afford the makeshift materials")

    # --- 3: a successful repair ---
    sql(f"UPDATE player_progress SET gold=1000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{pc_name}');")
    pc.close()
    pc = login(pc_name, pw)
    out = cmd(pc, "repair helm")
    check("mending it back into shape" in out,
          "a 100%-proficiency repair succeeds and reports success")

    pid = sql_out(f"SELECT id FROM player WHERE name='{pc_name}';").strip().splitlines()[-1]

    # --- 4: persistence through `save` ---
    cmd(pc, "save")
    row = sql_out(f"SELECT cur_struct, depreciation, monogram FROM player_inventory "
                  f"WHERE player_id={pid} AND vnum={HELM_VNUM};").strip().splitlines()[-1]
    cur_struct_s, depreciation_s, monogram = row.split("\t")
    check(int(cur_struct_s) == 10,
          "the repaired item's cur_struct (restored to max_struct - depreciation=0) persists via save")
    check(int(depreciation_s) == 1,
          "the repair incremented depreciation and it persists via save")
    check(monogram.strip() == pc_name,
          "the repair monogrammed the item with the repairer's name and it persists via save")

    # Damage it again (simulating combat wear) so there's something to
    # submit to the shop below -- the ceiling is now 10-1=9.
    sql(f"UPDATE player_inventory SET cur_struct=3 WHERE player_id={pid} AND vnum={HELM_VNUM};")
    pc.close()
    pc = login(pc_name, pw)

    # --- 5: shop flow ---
    out = cmd(pc, "submit helm")
    check("claim ticket" in out and "gold" in out,
          "submit hands over a claim ticket quoting a gold price")
    check("helm" not in cmd(pc, "inventory").lower(),
          "the submitted item is removed from inventory (obj_destroy)")

    out = cmd(pc, "tickets")
    check("dented test helm" in out.lower(),
          "tickets lists the pending claim for the submitted item")

    import re
    m = re.search(r"#(\d+)\)", out)
    check(m is not None, "the ticket listing includes a parseable ticket number")
    ticket_id = m.group(1)

    sql(f"UPDATE player_progress SET gold=0 WHERE player_id={pid};")
    pc.close()
    pc = login(pc_name, pw)
    out = cmd(pc, f"retrieve {ticket_id}")
    check("gold to pay for that repair" in out,
          "retrieve refuses when the player can't afford the ticket's price")

    sql(f"UPDATE player_progress SET gold=1000 WHERE player_id={pid};")
    pc.close()
    pc = login(pc_name, pw)
    out = cmd(pc, f"retrieve {ticket_id}")
    check("good as new" in out,
          "retrieve pays the price and hands back the repaired item")
    check("helm" in cmd(pc, "inventory").lower(),
          "the retrieved item is back in the player's inventory")

    out = cmd(pc, "tickets")
    check("no tickets waiting" in out.lower(),
          "the ticket is gone from the listing after being retrieved")

    left = sql_out(f"SELECT COUNT(*) FROM repair_ticket WHERE player_id={pid};").strip().splitlines()[-1]
    check(left == "0", "the retrieved ticket's DB row was actually deleted, not just hidden")

    announce_done("smoke_test_repair")
    print("=== ALL CHECKS PASSED ===")
finally:
    # Close sockets unconditionally, not just on the happy path -- an
    # assertion failure partway through used to leave these connections
    # open while the DB cleanup below deletes their `player` row out
    # from under them, orphaning a still-"connected" being that the
    # server then retries (and fails) autosaving forever after.
    for _sock_name in ("imm", "pc"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"DELETE FROM repair_ticket WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_skill WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{pc_name}');")
    sql(f"DELETE FROM obj WHERE vnum={HELM_VNUM};")
