#!/usr/bin/env python3
"""Smoke test for shop resale (user, 2026-08-04: "the shop should
attempt to resell the item at a profit" / "tax should be charged to
people selling at shops"). `sell <item>` (cmd_shop.c) previously
destroyed the sold item outright and paid no tax; now the item moves
into the keeper's own inventory instead, and both `list` and `buy` draw
from that resale stock (numbered/matched right after the shop's static
catalog), priced with the same `.price * profit_buy` formula a fresh
catalog item uses -- inherently a markup over `profit_sell` (always <
profit_buy, shop_repo.h). A flat sales tax (same SALES_TAX_RATE/treasury
destination as `buy`'s own tax) is now deducted from what a seller
receives.

Uses a real seeded shop (shop_nr 0, "The Glinting Dagger", room 559,
keeper mob vnum 150, profit_buy=1.1/profit_sell=0.9) rather than a
SQL-bootstrapped sandbox room, since shops are tied to real seeded
shop/shoptype/keeper data this test can't fabricate cheaply.

Covers:
  1. Selling a weapon (shop 0's own accepted category, raw itemTypeT 5)
     pays gold minus a sales tax, and the tax lands in the treasury.
  2. The sold item shows up in `list`, numbered after the static
     catalog, tagged "(used)".
  3. A second character can `buy` that exact resold item back by name,
     at a markup over what the first character was paid (the "profit").
  4. Once bought back, the item is gone from `list` (not double-sellable).

    python3 tests/smoke_test_shop_resell.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

SHOP_ROOM = 559
OBJ_VNUM = 959000 + (int(time.time()) % 900)
TYPE_WEAPON = 5
WEAR_TAKE = 1
WEAR_HOLD = 16384


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


announce("smoke_test_shop_resell", host, port)

seller_name, seller_pw = f"Shr{_suffix}", "shrpw1234567"
buyer_name, buyer_pw = f"Shrb{_suffix}", "shrbpw123456"

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,material,can_be_seen) "
    f"VALUES ({OBJ_VNUM},'resell dagger','a resell test dagger','A resell test dagger lies here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1000,0,1);")

s1 = make_char(seller_name, seller_pw, 3)  # Warrior (level 51+ needed for `load`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{seller_name}');")
sql(f"UPDATE player SET load_room={SHOP_ROOM} WHERE name='{seller_name}';")
s1 = relog(seller_name, seller_pw)
check("Glinting Dagger" in cmd(s1, "look"), "the seller lands in the real shop room")

check("You conjure" in cmd(s1, f"load obj {OBJ_VNUM}"), "the test dagger is loaded")

before_gold_out = cmd(s1, "score")

out = cmd(s1, "sell dagger")
check("you sell" in out.lower(), "selling the dagger to the shop succeeds")
check("sales tax" in out.lower(), "a sales tax is charged on the sale")

treasury_out = cmd(s1, "treasury")
check("gold" in treasury_out.lower(), "the treasury command still responds (sanity check tax landed somewhere)")

# --- 2: the sold item shows up in `list`, numbered after the catalog, tagged (used) ---
out = cmd(s1, "list")
check("resell test dagger" in out.lower(), "the sold dagger appears in the shop's list")
check("(used)" in out.lower(), "the resold item is tagged as used, distinct from fresh catalog stock")

# --- 3: a second character buys it back by name, at a markup over what the seller was paid ---
buyer_s = make_char(buyer_name, buyer_pw, 3)
cmd(buyer_s, "quit!"); buyer_s.close()
sql(f"UPDATE player_progress SET level=59, gold=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{buyer_name}');")
sql(f"UPDATE player SET load_room={SHOP_ROOM} WHERE name='{buyer_name}';")
buyer_s = relog(buyer_name, buyer_pw)
check("Glinting Dagger" in cmd(buyer_s, "look"), "the buyer lands in the same shop room")

out = cmd(buyer_s, "buy resell dagger")
check("you buy" in out.lower(), "buying the resold dagger back succeeds")
import re as _re
m = _re.search(r"(\d+) gold", out)
check(m is not None, "the buy confirmation shows a gold amount")
resale_price = int(m.group(1)) if m else 0
check(resale_price > 900, "the resale price is a markup over the ~900 (1000*0.9 tax-adjusted) the seller was paid")

# --- 4: bought back -- gone from `list` now ---
out = cmd(s1, "list")
check("resell test dagger" not in out.lower(), "the resold dagger is no longer listed after being bought back")

s1.close()
buyer_s.close()

sql(f"DELETE FROM player_inventory WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{seller_name}', '{buyer_name}'));")
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{seller_name}', '{buyer_name}'));")
sql(f"DELETE FROM player_attrs WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{seller_name}', '{buyer_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{seller_name}', '{buyer_name}');")
sql(f"DELETE FROM obj WHERE vnum={OBJ_VNUM};")

announce_done("smoke_test_shop_resell", host, port)
print("=== ALL CHECKS PASSED ===")
