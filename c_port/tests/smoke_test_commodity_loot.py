#!/usr/bin/env python3
"""Smoke test for commodities (TODO.md 2026-08-22: the real gap, scoped
as its own pass). Upstream's commodLoader() converts part of a mob's
wealth into a raw-material item at SPAWN time; Tobin's analogue hooks
combat.c's combat_defeat() at DEATH time instead, skimming a slice of
the corpse_gold a mob already drops and spending it on the priciest
cached commodity prototype it can afford (commodity.c), dropped into
the corpse alongside the coin pile. No new pricing model -- reuses the
182 already-seeded commodity prototypes' (obj.type 42/43/50) real
price column and the shops that already have shoptype rows for those
types.

Killer must be a real (non-immortal) PC -- combat_defeat()'s mob
gold-drop gate (and this feature's skim on top of it) is
`!being_is_immortal(winner)`, same gate smoke_test_animal_no_gold.py's
fight_and_check() works around by having the immortal only `load` the
mob and a separate padded-HP PC do the actual `attack`.

Covers:
  1. A non-animal mob's death sometimes drops a "... commodity" item
     into the corpse, retrievable the same way the coin pile is.
  2. That item is genuinely sellable at a shop already configured to
     buy its raw type (shop 15 for 42/43, shop 104 for 50) -- gold is
     credited on sale.

    python3 tests/smoke_test_commodity_loot.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done, drain

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_commodity_loot", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 950000 + (int(time.time()) % 40000)
MOB_VNUM = ROOM + 1

SHOP_A_ROOM, SHOP_A_DO_NOT_BUY = 558, "Only a fool would buy something so worthless!"
SHOP_B_ROOM = 572


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "3", "done", "done"):
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


def mob_row(vnum, name_tag):
    """Non-animal (race=0/NORACE), level 3 -- same weak level as
    smoke_test_animal_no_gold.py's own mob_row (a level-20 mob was
    tried first and one-shot a 500-HP fresh PC in 2 rounds: mob attrs
    scale off level directly, being_create_mob(), so a big level gap
    is lethal regardless of HP padding). corpse_gold (combat.c) is
    level*(1+rand()%5) = 3-15; the commodities skim (0-50% of that,
    skewed toward 25%) is often enough to afford the cheapest seeded
    commodity (price=1) -- not guaranteed every kill, hence the retry
    loop below."""
    cols = {
        "vnum": vnum, "name": f"'{name_tag}'", "short_desc": f"'a {name_tag}'",
        "long_desc": f"'A {name_tag} is here.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 3, "tohit": 0, "ac": 0, "hpbonus": -1.7,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 3,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    return f"INSERT INTO mob ({col_names}) VALUES ({col_values});"


imm_name, imm_pw = f"Commimm{_suffix}", "commimmpw123"
pc_name, pc_pw = f"Commpc{_suffix}", "commpcpw1234"
mob_tag = f"commtestmob{_suffix}"

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Commodity Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(mob_row(MOB_VNUM, mob_tag))

si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
check("Commodity Sandbox" in cmd(si, f"goto {ROOM}"), "the immortal lands in the sandbox room")

sp = make_char(pc_name, pc_pw)
cmd(sp, "quit!"); sp.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{pc_name}';")
sp = relog(pc_name, pc_pw)
check("Commodity Sandbox" in cmd(sp, "look"), "the fighter lands directly in the sandbox room")
# HP padded via the immortal `set` command AFTER login, not a raw SQL
# UPDATE before it -- player_repo.c's login path recomputes max_hp from
# level/class and clamps current hp down to it on every login, silently
# undoing a pre-login SQL hp set (documented the hard way in STATUS.md's
# Session ~189 smoke_test_group.py writeup).
cmd(si, f"set {pc_name} hp 500 500")

# --- 1: a mob's death sometimes drops a commodity item into the corpse ---
got_item = ""
for _ in range(10):
    # Re-heal before every round -- nothing restores the fighter's HP
    # between fights otherwise, so a run of several level-3 mobs can
    # still add up and kill it partway through the loop, silently
    # cutting off the remaining attempts (found live: a first run without
    # this had the fighter die on its second fight after winning its
    # first, wasting the other 8 attempts on a disconnected character).
    cmd(si, f"set {pc_name} hp 500 500")
    cmd(si, f"load mob {MOB_VNUM}")
    out = cmd(sp, f"attack {mob_tag}")
    for _ in range(10):
        if "slain" in out.lower() or "defeated" in out.lower():
            break
        out += recv_all(sp, 1.5)
    drain(sp)
    out = cmd(sp, "get all.commodity corpse")
    if "you get" in out.lower() and "commodity" in out.lower():
        got_item = out
        break

check(got_item != "", "a mob kill drops a commodity item into the corpse (within 10 kills)")

# --- 2: the dropped commodity is sellable at a shop configured for its type ---
# `transfer` (not quit!+relog) to move the fighter to the shop -- plain
# `quit!` deliberately drops every carried item on the floor where it
# was typed (cmd_quit.c, "quitting drops everything ... rent is the
# safe way to leave with belongings intact"), which would dump the very
# commodity this step needs to sell right back in the sandbox room.
if got_item:
    cmd(si, f"transfer {pc_name} {SHOP_A_ROOM}")
    drain(sp)
    out = cmd(sp, "sell commodity")
    if SHOP_A_DO_NOT_BUY in out:
        cmd(si, f"transfer {pc_name} {SHOP_B_ROOM}")
        drain(sp)
        out = cmd(sp, "sell commodity")
    check("you sell" in out.lower(), "the dropped commodity sells at a shop configured for its raw type")
else:
    check(False, "skipped: no commodity was dropped to test selling")

sql(f"DELETE FROM player WHERE name IN ('{imm_name}','{pc_name}');")
sql(f"DELETE FROM player_progress WHERE player_id NOT IN (SELECT id FROM player);")
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM mob WHERE vnum={MOB_VNUM};")

announce_done("smoke_test_commodity_loot", host, port)
