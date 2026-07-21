#!/usr/bin/env python3
"""Smoke test for Money system v2 (Sneezy -> Tobin feature audit,
"Money system v2 (banking/taxes)"). Confirmed with the user via
AskUserQuestion 2026-07-21: a single global bank (not the original's
per-shop accounts) and tax revenue collects into a visible treasury (not
a sink, not full double-entry). Covers:

  1. `bank` away from a bank shop refuses.
  2. `bank`/`bank balance` at the real seeded Grimhaven First Kingdom
     Bank (shop_nr 4, room 31750) shows wallet + bank balances.
  3. deposit more than the wallet holds refuses; a valid deposit moves
     gold from wallet to bank_gold.
  4. withdraw more than the bank holds refuses; a valid withdraw moves
     it back.
  5. Persistence: gold/bank_gold round-trip through `save`.
  6. Sales tax: buying from a real ordinary shop (shop_nr 1, "Lumor's
     Illuminations") charges a 5% surcharge on top of the sticker price,
     and the treasury's tracked total grows by exactly that surcharge.

Daily bank interest (bank_interest_tick(), bank.c) isn't covered here --
it only fires on a real in-game day rollover (~96 real minutes at
Tobin's default clock speed), the same "not practical to keep automated"
call already made for smoke_test_heartbeat.py's own real-time boundary.

    python3 tests/smoke_test_bank.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

BANK_ROOM = 31750
SHOP_ROOM = 550


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


announce("smoke_test_bank")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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


imm_name = f"Bnkimma{_suffix}"
pc_name = f"Bnkpca{_suffix}"
pw = "banktestpw123"

try:
    make_char(imm_name, pw, "1")
    make_char(pc_name, pw, "3")
    sql(f"UPDATE player_progress SET level=59 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"UPDATE player_progress SET gold=1000, bank_gold=0 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{pc_name}');")

    imm = login(imm_name, pw)
    pc = login(pc_name, pw)

    # --- 1: no bank here ---
    out = cmd(pc, "bank")
    check("don't see a bank" in out.lower(),
          "bank refuses when there's no bank in the room")

    # --- 2: balance at the real bank ---
    check("Grimhaven" in cmd(imm, f"goto {BANK_ROOM}"),
          "goto lands the immortal at the real Grimhaven First Kingdom Bank")
    cmd(imm, f"transfer {pc_name} {BANK_ROOM}")
    recv_all(pc); recv_all(imm)

    out = cmd(pc, "bank")
    check("1000 gold in your wallet" in out and "0 gold in the bank" in out,
          "bank with no args shows both wallet and bank balances")
    out = cmd(pc, "bank balance")
    check("1000 gold in your wallet" in out,
          "bank balance is an explicit alias for the same view")

    # --- 3: deposit ---
    out = cmd(pc, "bank deposit 5000")
    check("don't have that much gold to deposit" in out,
          "deposit refuses more than the wallet holds")

    out = cmd(pc, "bank deposit 400")
    check("counts 400 gold into your account" in out,
          "a valid deposit succeeds and reports the amount")
    out = cmd(pc, "bank")
    check("600 gold in your wallet" in out and "400 gold in the bank" in out,
          "the deposit actually moved gold from wallet to bank_gold")

    # --- 4: withdraw ---
    out = cmd(pc, "bank withdraw 9000")
    check("don't have that much in the bank" in out,
          "withdraw refuses more than the bank holds")

    out = cmd(pc, "bank withdraw 100")
    check("counts 100 gold out for you" in out,
          "a valid withdraw succeeds and reports the amount")
    out = cmd(pc, "bank")
    check("700 gold in your wallet" in out and "300 gold in the bank" in out,
          "the withdrawal actually moved gold back from bank_gold to wallet")

    # --- 5: persistence ---
    pid = sql_out(f"SELECT id FROM player WHERE name='{pc_name}';").strip().splitlines()[-1]
    cmd(pc, "save")
    row = sql_out(f"SELECT gold, bank_gold FROM player_progress WHERE player_id={pid};").strip().splitlines()[-1]
    gold_s, bank_gold_s = row.split("\t")
    check(int(gold_s) == 700 and int(bank_gold_s) == 300,
          "wallet/bank balances persist correctly via save")

    # --- 6: sales tax ---
    treasury_before = int(sql_out("SELECT gold FROM world_treasury WHERE id=1;").strip().splitlines()[-1])
    cmd(imm, f"goto {SHOP_ROOM}")
    cmd(imm, f"transfer {pc_name} {SHOP_ROOM}")
    recv_all(pc); recv_all(imm)

    # Item #6 ("fuel large brick", price 30 * shop's own 1.1 profit_buy =
    # 33) rather than #1 (a 3-gold torch) -- a 5% tax on anything under
    # ~20 gold rounds down to 0 and cmd_buy() skips the tax message
    # entirely in that case (see cmd_shop.c's `if (tax > 0)`), which
    # would make this check pass for the wrong reason.
    gold_before_buy = 700
    out = cmd(pc, "buy 6")
    check("You buy" in out, "buy 6 successfully purchases a pricier producing item")
    check("sales tax of" in out and "crown's coffers" in out,
          "buying from an ordinary shop charges a visible sales tax")

    import re
    m = re.search(r"sales tax of (\d+) gold", out)
    check(m is not None, "the tax message includes a parseable gold amount")
    tax_charged = int(m.group(1))

    row = sql_out(f"SELECT gold FROM player_progress WHERE player_id={pid};").strip().splitlines()[-1]
    gold_after_buy = int(row)
    total_deducted = gold_before_buy - gold_after_buy
    check(total_deducted > tax_charged,
          "the tax is charged on top of the item's own sticker price, not folded into it")

    treasury_after = int(sql_out("SELECT gold FROM world_treasury WHERE id=1;").strip().splitlines()[-1])
    check(treasury_after == treasury_before + tax_charged,
          "the treasury's tracked total grew by exactly the tax charged")

    out = cmd(imm, "treasury")
    check(str(treasury_after) in out,
          "the immortal-only treasury command reports the same running total")

    imm.close()
    pc.close()

    announce_done("smoke_test_bank")
    print("=== ALL CHECKS PASSED ===")
finally:
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{pc_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{pc_name}');")
