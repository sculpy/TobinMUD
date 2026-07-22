#!/usr/bin/env python3
"""Smoke test for the material property system (Sneezy -> Tobin feature
audit, "Material property system"). Per user 2026-07-21's AskUserQuestion,
Tobin's version deliberately goes FURTHER than the real upstream's own
verified mechanics (durability + value only) -- a genuine damage/AC
multiplier per tier too, a disclosed invention rather than a faithful
port. Reuses Tobin's EXISTING real seeded `obj.material` column (already
populated from the original import, never mechanically used before this)
rather than inventing a new per-item field, bucketed into 5 tiers via
material_tier_for_id() (material.c). Covers:

  1. `identify` shows the right tier name for a real seeded item
     (vnum 3108, material=159 MAT_STEEL -> Superior).
  2. Structure bonus: a Superior-tier item's live max_struct is the
     prototype's own max_struct (38) + the tier's +5 bonus (43) --
     confirmed indirectly via `repair`'s own cost quote, since there's
     no direct "show me a live instance's max_struct" command.
  3. AC multiplier: two custom armor items, identical except material
     (Common vs Legendary), show `score`'s Armor Class scaling by
     exactly the tier's 2.0x multiplier (a flat weight-based formula,
     no randomness -- an exact assertion, not statistical).
  4. Damage multiplier: two custom weapons, identical except material,
     land repeated hits against a huge-HP mortal target -- the
     Legendary-material weapon's total damage over N hits is
     meaningfully higher than the Common one's (statistical, same
     "eventually"/repeated-sample shape smoke_test_skillcombat.py
     already uses for stochastic combat outcomes).
  5. Value multiplier: buying a real shop-producing item (shop_nr 1,
     "Lumor's Illuminations") after temporarily bumping its seeded
     material to Rare (6x) costs 6x what the same item costs at its
     real original material -- the item's real material is restored
     afterward, not left mutated.

    python3 tests/smoke_test_material.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

STEEL_HELMET_VNUM = 3108   # real seeded item, material=159 (MAT_STEEL -> Superior)
SHOP_ROOM = 550
SHOP_ITEM_INDEX = 6        # "fuel large brick", vnum 110, price 30 -- see smoke_test_bank.py's own note

TYPE_WEAPON = 5             # matches OBJ_CAT_WEAPON's raw seeded itemTypeT bucket
TYPE_WORN = 11              # matches OBJ_CAT_ARMOR (same raw type other tests' worn items use)
WEAR_TAKE = 1
WEAR_HOLD = 16384
WEAR_BODY = 8

MAT_COMMON = 0       # MAT_UNDEFINED
MAT_LEGENDARY = 160  # MAT_MITHRIL
MAT_RARE = 162       # MAT_SILVER


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


announce("smoke_test_material")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 950000 + (int(time.time()) % 20000)
COMMON_ARMOR_VNUM = ROOM + 1
LEGENDARY_ARMOR_VNUM = ROOM + 2
COMMON_WEAPON_VNUM = ROOM + 3
LEGENDARY_WEAPON_VNUM = ROOM + 4


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


imm_name = f"Mtrimma{_suffix}"
tgt_name = f"Mtrtgta{_suffix}"
pw = "materialtestpw1"

orig_shop_material = sql_out(
    "SELECT material FROM obj WHERE vnum=("
    "SELECT producing FROM shopproducing WHERE shop_nr=1 ORDER BY producing LIMIT 1 OFFSET "
    f"{SHOP_ITEM_INDEX - 1});").strip().splitlines()[-1]
shop_item_vnum = sql_out(
    "SELECT producing FROM shopproducing WHERE shop_nr=1 ORDER BY producing LIMIT 1 OFFSET "
    f"{SHOP_ITEM_INDEX - 1};").strip().splitlines()[-1]

try:
    make_char(imm_name, pw, "1")
    make_char(tgt_name, pw, "3")
    sql(f"UPDATE player_progress SET level=59, gold=100000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"UPDATE player_progress SET hp=999999, max_hp=999999, gold=100000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{tgt_name}');")
    sql(f"UPDATE player_attrs SET dexterity=900 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"UPDATE player_attrs SET dexterity=1 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{tgt_name}');")

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Material Sandbox','A bare sandbox room.\\n',"
        f"NULL,1,0,0,0,0,0,0,0,0,0);")

    # Two armor items, identical (weight 5, no real objaffect AC row) except
    # material -- obj_armor_ac()'s weight formula gives a base AC of 10
    # (5 * ARMOR_AC_PER_WEIGHT=2), well under ARMOR_AC_MAX=30, so the
    # Legendary tier's 2.0x multiplier is fully visible, not clamped away.
    # `name` is a single word each, deliberately -- get/wear/wield/remove
    # all parse only the FIRST whitespace-separated token of their args
    # (cmd_get's own `sscanf(args, "%63s %63s", tok, conttok)`, the
    # second slot being a container name, not more of the item token), so
    # a two-word `name` like 'vest common test' can't be told apart from
    # 'vest legendary test' by a single-token search -- only the first
    # word ("vest") would ever be read.
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen,material) "
        f"VALUES ({COMMON_ARMOR_VNUM},'commonvest','a common test vest',"
        f"'A common test vest is lying here.',{TYPE_WORN},{WEAR_TAKE | WEAR_BODY},5,1,{MAT_COMMON});")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen,material) "
        f"VALUES ({LEGENDARY_ARMOR_VNUM},'legendaryvest','a legendary test vest',"
        f"'A legendary test vest is lying here.',{TYPE_WORN},{WEAR_TAKE | WEAR_BODY},5,1,{MAT_LEGENDARY});")

    # Two weapons, identical except material -- same "sword" shape
    # smoke_test_skillcombat.py already uses successfully for real damage.
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen,material) "
        f"VALUES ({COMMON_WEAPON_VNUM},'commonsword','a common test sword',"
        f"'A common test sword is lying here.',{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},5,1,{MAT_COMMON});")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen,material) "
        f"VALUES ({LEGENDARY_WEAPON_VNUM},'legendarysword','a legendary test sword',"
        f"'A legendary test sword is lying here.',{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},5,1,{MAT_LEGENDARY});")

    imm = login(imm_name, pw)
    tgt = login(tgt_name, pw)

    check("Material Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the sandbox room")
    cmd(imm, f"transfer {tgt_name} {ROOM}")
    recv_all(tgt); recv_all(imm)

    # --- 1 & 2: identify + structure bonus (real seeded steel helmet) ---
    # `load` puts the item straight into the loading immortal's own
    # inventory (2026-07-22) -- no `get` needed when the loader is also
    # the one using it.
    cmd(imm, f"load obj {STEEL_HELMET_VNUM}")
    out = cmd(imm, "identify helmet")
    check("Material:  Superior" in out,
          "identify shows the right tier name for a real seeded material (159 MAT_STEEL -> Superior)")

    pid = sql_out(f"SELECT id FROM player WHERE name='{imm_name}';").strip().splitlines()[-1]
    cmd(imm, "save")
    sql(f"UPDATE player_inventory SET cur_struct=3 WHERE player_id={pid} AND vnum={STEEL_HELMET_VNUM};")
    sql(f"UPDATE player_progress SET gold=0 WHERE player_id={pid};")
    imm.close()
    imm = login(imm_name, pw)
    out = cmd(imm, "repair helmet")
    m = re.search(r"need (\d+) gold", out)
    check(m is not None, "repair's cost quote is parseable after forcing cur_struct down")
    needed_gold = int(m.group(1))
    ceiling = needed_gold // 2 + 3
    check(ceiling == 43,
          "the live instance's max_struct is the prototype's 38 + Superior tier's +5 bonus = 43")
    sql(f"UPDATE player_progress SET gold=100000 WHERE player_id={pid};")
    imm.close()
    imm = login(imm_name, pw)
    cmd(imm, f"goto {ROOM}")

    # --- 3: AC multiplier (exact, no randomness involved) ---
    # `tgt`, not the loading immortal, needs to wear these -- `load`
    # puts a fresh item in the LOADER's own inventory now, so it has to
    # be dropped explicitly before `tgt` can `get` it.
    cmd(imm, f"load obj {COMMON_ARMOR_VNUM}")
    cmd(imm, "drop commonvest")
    cmd(tgt, "get commonvest")
    cmd(tgt, "wear commonvest")
    out = cmd(tgt, "score")
    m = re.search(r"Armor Class:\s+(-?\d+)", out)
    check(m is not None, "score's Armor Class line is parseable")
    common_ac = int(m.group(1))
    check(common_ac == 10, "a Common-material armor item shows the plain weight-based AC (5 * 2 = 10)")

    cmd(tgt, "remove commonvest")
    cmd(imm, f"load obj {LEGENDARY_ARMOR_VNUM}")
    cmd(imm, "drop legendaryvest")
    cmd(tgt, "get legendaryvest")
    cmd(tgt, "wear legendaryvest")
    out = cmd(tgt, "score")
    m = re.search(r"Armor Class:\s+(-?\d+)", out)
    legendary_ac = int(m.group(1))
    check(legendary_ac == 20,
          "a Legendary-material armor item's AC is exactly 2.0x the Common one's (10 -> 20)")
    cmd(tgt, "remove legendaryvest")

    # --- 4: damage multiplier (statistical -- real combat rolls involved) ---
    # The weapon has to be wielded by the ATTACKER (imm) -- combat_strike()
    # reads combat_wielded_weapon(attacker), not the defender's gear.
    # Measured via `tgt`'s own `score` HP line rather than parsing combat
    # chat text -- combat messages no longer carry a raw damage number at
    # all (user 2026-07-22: "take out the damage number and use it to
    # describe how hard the hit was"), so total HP lost over a fixed
    # real-time window is now the only way to compare average damage.
    def tgt_hp():
        out = cmd(tgt, "score")
        m = re.search(r"HP:\s+(\d+)/", out)
        check(m is not None, "score's HP line is parseable")
        return int(m.group(1))

    cmd(imm, f"load obj {COMMON_WEAPON_VNUM}")
    cmd(imm, "wield commonsword")
    hp_before = tgt_hp()
    cmd(imm, f"hit {tgt_name}")
    time.sleep(15)
    common_loss = hp_before - tgt_hp()
    check(common_loss > 0, f"the Common weapon dealt measurable damage over the sample window ({common_loss} HP)")

    # Explicitly unwield the common sword first -- `wield` with an
    # occupied primary hand fills the OFF-hand instead of replacing it,
    # which would leave both weapons active (and combat_wielded_weapon()
    # alternating between them) rather than a clean before/after swap.
    cmd(imm, "remove commonsword")
    cmd(imm, f"load obj {LEGENDARY_WEAPON_VNUM}")
    cmd(imm, "wield legendarysword")
    hp_before = tgt_hp()
    time.sleep(15)
    legendary_loss = hp_before - tgt_hp()
    check(legendary_loss > 0,
          f"the Legendary weapon dealt measurable damage over the sample window ({legendary_loss} HP)")
    check(legendary_loss > common_loss * 1.3,
          f"a Legendary-material weapon's total damage over the same window ({legendary_loss} HP) is "
          f"meaningfully higher than a Common one's ({common_loss} HP) -- the material system's 2.0x multiplier")

    # --- 5: value multiplier (real shop item, material temporarily bumped) ---
    sql(f"UPDATE obj SET material={MAT_RARE} WHERE vnum={shop_item_vnum};")
    cmd(imm, f"goto {SHOP_ROOM}")
    cmd(imm, f"transfer {tgt_name} {SHOP_ROOM}")
    recv_all(tgt); recv_all(imm)
    sql(f"UPDATE player_progress SET gold=100000, bank_gold=0 WHERE player_id={pid};")
    out = cmd(tgt, f"buy {SHOP_ITEM_INDEX}")
    # shop_nr 1's own message_buy template is "That'll be %d talens.
    # Thank you!" -- not a generic "for N gold" phrasing every shop
    # shares (each shop has its own hand-authored flavor text).
    m = re.search(r"That'll be (\d+)", out)
    check(m is not None, "the shop's own paid-message price is parseable")
    rare_price = int(m.group(1))
    # 30 gold base * shop_nr 1's 1.1 profit_buy * Rare's 6.0x value multiplier.
    check(rare_price == int(30 * 1.1 * 6.0),
          "bumping a real shop item's material to Rare (6x) scales its buy price by exactly 6x")

    announce_done("smoke_test_material")
    print("=== ALL CHECKS PASSED ===")
finally:
    # Close sockets unconditionally, not just on the happy path -- an
    # assertion failure partway through used to leave these connections
    # open while the DB cleanup below deletes their `player` row out
    # from under them, orphaning a still-"connected" being that the
    # server then retries (and fails) autosaving forever after.
    for _sock_name in ("imm", "tgt"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"UPDATE obj SET material={orig_shop_material} WHERE vnum={shop_item_vnum};")
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{tgt_name}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{tgt_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{tgt_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{tgt_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({COMMON_ARMOR_VNUM}, {LEGENDARY_ARMOR_VNUM}, "
        f"{COMMON_WEAPON_VNUM}, {LEGENDARY_WEAPON_VNUM});")
