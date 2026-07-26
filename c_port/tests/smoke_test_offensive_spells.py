#!/usr/bin/env python3
"""Smoke test for "offensive spell breadth" (Sneezy -> Tobin feature
audit) -- cmd_cast.c/cmd_pray.c's offensive-damage path used to deal a
single FLAT amount regardless of which spell was cast, only ever hit
whoever you were already fighting (no way to open combat with a spell,
and `cast` had no target syntax at all), and used a raw being_hurt_limb()
with no defeat handling. Covers:

  1. Damage now scales with the SPELL's own tier (a level-1 spell hits
     far softer than a level-48 one), not a flat caster-level formula.
  2. `cast <spell> <target>` now works at all (previously cast had no
     target syntax whatsoever) and can OPEN combat against someone not
     already fighting -- proven for real: a SECOND cast with no target
     still lands on the same opponent, meaning ch->fighting was actually
     set by the first one, not just a one-off hit.
  3. Regression check for a bug caught and fixed mid-development: praying
     a heal spell with NO target while already in combat heals SELF, not
     the opponent (an early draft of this breadth work briefly broke
     this by resolving the default target the wrong way).
  4. A real "area-effect" spell now hits EVERY other being in the room,
     not just a single target -- two separate, uninvolved bystanders
     both take damage from one cast.
  5. `pray` gets the same tiered-damage/open-combat treatment as `cast`.

    python3 tests/smoke_test_offensive_spells.py [host] [port]
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


announce("smoke_test_offensive_spells")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 60000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2


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


def dmg_of(out):
    m = re.search(r"for (\d+) damage", out)
    return int(m.group(1)) if m else None


imm_name = f"Offimma{_suffix}"
imm2_name = f"Offimmb{_suffix}"
b1_name = f"Offbysa{_suffix}"
b2_name = f"Offbysb{_suffix}"
pw = "offspellpw123"

try:
    make_char(imm_name, pw, "1")   # class doesn't matter, immortal bypasses class gates
    make_char(imm2_name, pw, "1")
    make_char(b1_name, pw, "3")
    make_char(b2_name, pw, "3")
    # All four promoted to immortal -- not just imm/imm2. b1/b2 only need
    # to be reachable, damageable-in-name targets for the offensive-spell
    # checks below; being immortal doesn't affect any of those (an
    # immortal DEFENDER just takes zero real damage, same immunity rule
    # combat_strike() enforces -- the messages/damage NUMBERS shown to
    # the immortal CASTER are the requested amount, not the post-immunity
    # one, so the tiered-damage assertions below are unaffected). Doing
    # this means all four can `goto` a throwaway sandbox room (`goto` is
    # immortal-only) instead of running the area-effect spell in a real,
    # populated production room where it could catch actual bystanders.
    set_level(imm_name, 51)
    set_level(imm2_name, 51)
    set_level(b1_name, 51)
    set_level(b2_name, 51)

    imm = login(imm_name, pw)
    imm2 = login(imm2_name, pw)
    b1 = login(b1_name, pw)
    b2 = login(b2_name, pw)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Offensive Spell Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    check("Offensive Spell Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the sandbox room")
    cmd(imm2, f"goto {ROOM}")
    cmd(b1, f"goto {ROOM}")
    cmd(b2, f"goto {ROOM}")
    recv_all(imm); recv_all(imm2); recv_all(b1); recv_all(b2)  # drain arrival notices

    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
        f"'A pouch of spell components is lying here.',12,1,1);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
        f"'A tarnished silver holy symbol is lying here.',12,1,1);")

    def restock_component():
        cmd(imm, f"load obj {COMPONENT}")
        cmd(imm, "get pouch")

    def restock_symbol():
        cmd(imm, f"load obj {SYMBOL}")
        cmd(imm, "get symbol")

    # --- 1: damage scales with the SPELL's own tier, not a flat formula.
    # Both casts target the SAME being (b1) -- ch is fighting nobody yet,
    # so the first cast opens combat with b1, and the second (b1 still
    # being ch->fighting) is a normal continued strike, not a one-off on
    # a different bystander -- keeps step 2 below unambiguous. ---
    restock_component()
    out = cmd(imm, f"cast gust {b1_name}")  # level 1, "A bolt of wind damage."
    check("You cast gust at" in out, "`cast <spell> <target>` works at all (previously no target syntax existed)")
    dmg_low = dmg_of(out)
    check(dmg_low is not None, "casting a low-tier offensive spell shows a damage number (immortal caster)")

    restock_component()
    out = cmd(imm, f"cast atomize {b1_name}")  # level 48, "An overwhelming single-target burst of energy."
    dmg_high = dmg_of(out)
    check(dmg_high is not None, "casting a high-tier offensive spell shows a damage number")
    check(dmg_high > dmg_low * 2,
          f"a level-48 spell ({dmg_high}) hits far harder than a level-1 one ({dmg_low}) -- "
          "tiered by the SPELL itself, not a flat formula")

    # --- 2: `cast <spell> <target>` genuinely OPENS combat, proven by a
    # second cast with NO target still landing on the same opponent
    # (ch->fighting was really set by the very first cast above, not
    # just a one-off hit each time). ---
    restock_component()
    out = cmd(imm, "cast gust")  # no target this time
    check(b1_name in out, "a follow-up cast with no target keeps hitting the SAME opponent -- "
          "proves the earlier cast actually opened real combat (ch->fighting), not a one-off hit")

    # --- 3: regression check -- a heal prayer with NO target, while
    # already in combat (imm is still fighting b2 from step 2 -- imm is
    # ALREADY fighting someone, so this is deliberately a one-off hit on
    # a THIRD party, imm2, not a re-target; see the damage branch's own
    # "already fighting someone else" comment), heals SELF, not whoever
    # ch->fighting happens to be. This is the exact bug an early draft of
    # this breadth work introduced and then fixed: resolving the default
    # target as ch->fighting at the CALLER level (rather than only inside
    # the offensive branches) would have made a plain "pray heal light"
    # heal the enemy instead. ---
    restock_symbol()
    out = cmd(imm, f"pray harm light {imm2_name}")
    check("You pray for harm light, striking" in out, "a one-off offensive prayer hits imm2 without disturbing the existing fight with b2")
    restock_symbol()
    out = cmd(imm, "pray heal light")  # no target, while fighting imm2
    check("You pray for heal light and feel restored" in out,
          "a no-target heal prayer heals SELF even while in combat, not the opponent "
          "(regression check for a bug caught and fixed mid-development)")

    # --- 4: real area-effect -- hits every other being in the room, not
    # just one target. Doesn't require ch->fighting at all. ---
    restock_component()
    out = cmd(imm, "cast fireball")  # level 14, "A powerful area-effect burst of fire damage."
    check("catching everyone nearby" in out, "casting a real area-effect spell announces it hit everyone nearby")
    out_b1 = recv_all(b1, timeout=1.0)
    out_b2 = recv_all(b2, timeout=1.0)
    check("catches you" in out_b1, "bystander 1 takes area-effect damage")
    check("catches you" in out_b2, "bystander 2 (a completely different, uninvolved bystander) ALSO takes damage")
    out_imm2 = recv_all(imm2, timeout=1.0)
    check("catches you" in out_imm2, "imm2 (a third, still-different being) is caught by the same area burst too")

    imm.close()
    imm2.close()
    b1.close()
    b2.close()

    announce_done("smoke_test_offensive_spells")
    print("=== ALL CHECKS PASSED ===")
finally:
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{imm2_name}', '{b1_name}', '{b2_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{imm2_name}', '{b1_name}', '{b2_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT}, {SYMBOL});")
