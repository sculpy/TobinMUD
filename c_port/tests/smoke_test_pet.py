#!/usr/bin/env python3
"""Smoke test for pet/charm (Sneezy -> Tobin feature audit, "Pet / charm
(followers)"). Scoped via AskUserQuestion, 2026-07-25: full pet behavior
(follows its master room-to-room, assists in combat) across all three
fitting classes in one pass -- Mage's four pre-existing "conjure elemental
air/earth/fire/water" placeholders (real Sneezy names/flavor text, never
wired to anything before this), and two new spells, Cleric's "summon
swarm" and Druid's "animal companion". All three reuse real seeded world
mobs (fire elemental vnum 16, wolf vnum 570, locust swarm vnum 7852) via
being_create_mob(), not new DB rows. Covers:
  1. Mage: `cast conjure elemental fire` summons a charmed pet.
  2. A second summon attempt while one is already active is refused
     (one-pet-at-a-time cap, being_find_charmed_pet()).
  3. The pet follows its master through a room move (cmd_move.c).
  4. The pet joins its master's fight (combat.c's pet-assist pass) --
     visible via the "<pet> strikes/misses <target>!" message.
  5. `dismiss` releases the pet immediately, before its natural
     AFFECT_CHARMED duration would run out; a new summon works right
     after (the cap is actually clear again, not just message-copy).
  6. Cleric `pray summon swarm` and Druid `cast animal companion` each
     summon their own real pet too (spot-checked, not a full re-run of
     1-5 -- same class-breadth-without-re-testing-every-branch precedent
     as the offensive-spell-breadth work).

Natural charm expiry (AFFECT_CHARMED timing out on its own,
PET_CHARM_DURATION_ROUNDS ~5 real minutes) is NOT covered here, same
"not practical to keep automated" call already made for
smoke_test_heartbeat.py's own real-time boundary -- sanity-checked
manually instead.

    python3 tests/smoke_test_pet.py [host] [port]
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


announce("smoke_test_pet")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
ROOM2 = ROOM + 1
MOB_VNUM = ROOM + 2
COMPONENT = ROOM + 3
SYMBOL = ROOM + 4

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


def grant_proficiency(name, skill_name, pct=100):
    # cast/pray roll skill_roll_success(skill_learn_from_doing(...)) against
    # the caster's own learn-by-doing PROFICIENCY (player_skill.pct) --
    # separate from the coarse discipline-percentage ACCESS gate set_caster()
    # covers, and 0 by default for a never-practiced skill (a mortal would
    # almost always "fumble the casting" on a first attempt otherwise).
    # Immortals bypass this roll entirely, but a real mortal level is needed
    # here anyway (see set_caster()'s own comment), so this grants
    # proficiency directly instead.
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, 0) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_caster(name, level):
    # A MORTAL level (not the immortal-51 shortcut smoke_test_castpray.py
    # uses) -- `attack`/`kill` gives immortals an instant slay (cmd_table.c:
    # "attack" -> cmd_kill, "instant slay for immortals"), which would kill
    # the sandbox dummy in one blow before the pulse-driven combat loop
    # (and the pet's own strike, phase 4) ever gets a chance to run. Set
    # every discipline percentage to 100 directly instead, bypassing the
    # normal practice-point grind (cmd_practice.c) the same way, without
    # the instant-kill side effect.
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, "
        f"combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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


# Defensive cleanup for a prior run that errored out mid-test (same
# second-granularity vnum from time.time() would otherwise collide).
sql(f"DELETE FROM mob WHERE vnum={MOB_VNUM};")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM},{ROOM2});")
sql(f"DELETE FROM roomexit WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT},{SYMBOL});")

imm_name, imm_pw = f"Petimmb{_suffix}", "petimmpw1234"
s_imm = make_char(imm_name, imm_pw, "1")
set_level(imm_name, 51)
s_imm.close()
s_imm = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Pet Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM2},0,0,0,'Pet Sandbox North','A second bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM}, 0, '', '', 0, 0, 0, 0, 0, {ROOM2});")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")

cols = {
    "vnum": MOB_VNUM, "name": "'petdummy'", "short_desc": "'a pet test dummy'",
    "long_desc": "'A pet test dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 15, "tohit": 0, "ac": 0, "hpbonus": 40,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")


def give_component(sock):
    # `load obj` lands in the LOADING IMMORTAL's own inventory (2026-07-22
    # fix, cmd_load.c), not the room floor -- drop it first so the caster
    # can `get` it from the ground, same fix smoke_test_castpray.py itself
    # never received (a known, previously-flagged stale-test risk, see
    # STATUS.md's material-properties writeup: "the other ~55 test files
    # using `load obj` were NOT audited").
    check("You conjure" in cmd(s_imm, f"load obj {COMPONENT}"), "immortal loads a spell component")
    cmd(s_imm, "drop pouch")
    out = cmd(sock, "get pouch")
    check("you get" in out.lower(), "the caster picks up the component")


# ============================================================
# Phase 1-5: Mage's "conjure elemental fire" -- full pet lifecycle
# ============================================================
mage_name, mage_pw = f"Petmagb{_suffix}", "petmagpw1234"
sm = make_char(mage_name, mage_pw, "1")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mage_name}';")
cmd(sm, "quit!")
sm.close()
set_caster(mage_name, 35)
set_hp(mage_name, 50000, 50000)  # an enormous buffer -- the fight's real duration is unpredictable
                                   # (needs to last more than 1 round so the pet's own strike, phase 4,
                                   # has a chance to land before the dummy dies), so this just removes
                                   # any risk of the master losing the fight first, regardless of how
                                   # long it actually runs
grant_proficiency(mage_name, "conjure elemental fire")
sm = relog(mage_name, mage_pw)
cmd(s_imm, f"goto {ROOM}")

give_component(sm)

# --- 1: cast conjure elemental fire summons a pet ---
out = cmd(sm, "cast conjure elemental fire")
check("fire elemental" in out.lower(), "casting conjure elemental fire produces a fire-elemental flavor message")
out = cmd(sm, "look")
check("a fire elemental" in out.lower(), "the summoned fire elemental is standing in the room")

# --- 2: a second summon while one is active is refused ---
give_component(sm)
out = cmd(sm, "cast conjure elemental fire")
check("already have a charmed creature" in out.lower(), "a second summon attempt is refused while a pet is already active")

# --- 3: the pet follows its master through a room move ---
out = cmd(sm, "north")
check("Pet Sandbox North" in out, "the mage moves to the second room")
out = cmd(sm, "look")
check("a fire elemental" in out.lower(), "the pet followed its master into the new room")

# --- 4: the pet assists in combat ---
cmd(s_imm, f"goto {ROOM2}")
cmd(s_imm, f"load mob {MOB_VNUM}")
out = cmd(sm, "attack petdummy")
check("You attack" in out, "the mage engages the sandbox dummy")

saw_pet_strike = False
killed = False
for _ in range(20):
    out = cmd(sm, "")
    if "fire elemental" in out.lower() and ("strikes" in out.lower() or "misses" in out.lower()):
        saw_pet_strike = True
    if "have slain" in out.lower() or "have defeated" in out.lower():
        killed = True
        break
    time.sleep(1.5)
check(saw_pet_strike, "the pet's own strike/miss message appeared during the fight, not just the master's")

# --- 5: dismiss releases the pet immediately ---
out = cmd(sm, "dismiss")
check("release" in out.lower() and "fades away" in out.lower(), "dismiss confirms the pet was released")
out = cmd(sm, "look")
check("a fire elemental" not in out.lower(), "the dismissed pet is actually gone from the room")

give_component(sm)
out = cmd(sm, "cast conjure elemental fire")
check("already have a charmed creature" not in out.lower(),
      "after dismiss, a fresh summon is no longer refused -- the cap was really cleared, not just the message")
check("fire elemental" in out.lower(), "the fresh summon after dismiss actually succeeds")

# ============================================================
# Phase 7: the pet obeys spoken commands (cmd_say.c's try_pet_command())
# ============================================================
# A fresh dummy -- phase 4's own dummy may already be dead (the combat
# poll loop there breaks as soon as it sees one pet strike, not
# necessarily on the kill, and real time keeps passing across the several
# get/cast round-trips since).
cmd(s_imm, f"load mob {MOB_VNUM}")


def say_until(sock, line, want, tries=8):
    # PET_CONFUSION_CHANCE_PCT (cmd_say.c) gives the pet a real chance of
    # ignoring a command outright -- retry a few times rather than treat
    # one confused roll as a failure. 8 tries at 80% success each leaves
    # under a 0.002% chance of a false failure.
    out = ""
    for _ in range(tries):
        out = cmd(sock, line)
        if want in out.lower():
            return out
        time.sleep(0.3)
    return out


out = say_until(sm, "say attack petdummy", "obeys, and turns to attack")
check("obeys, and turns to attack" in out.lower(), "saying \"attack petdummy\" makes the pet obey and engage")

out = say_until(sm, "say stop", "obeys, and stands down")
check("obeys, and stands down" in out.lower(), "saying \"stop\" makes the pet disengage")

out = say_until(sm, "say dance", "breakdance")
check("breakdance" in out.lower(), "saying \"dance\" makes the pet perform the real dance social")

cmd(sm, "dismiss")

sm.close()

# ============================================================
# Phase 6: Cleric summon swarm / Druid animal companion -- spot check
# ============================================================
cleric_name, cleric_pw = f"Petclrb{_suffix}", "petclrpw1234"
sc = make_char(cleric_name, cleric_pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(sc, "quit!")
sc.close()
set_caster(cleric_name, 35)
grant_proficiency(cleric_name, "summon swarm")
sc = relog(cleric_name, cleric_pw)
cmd(s_imm, f"goto {ROOM}")
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "immortal loads a holy symbol")
cmd(s_imm, "drop symbol")
out = cmd(sc, "get symbol")
check("you get" in out.lower(), "the cleric picks up the holy symbol")
out = cmd(sc, "pray summon swarm")
check("locusts" in out.lower(), "praying for summon swarm produces a locust-swarm flavor message")
out = cmd(sc, "look")
check("a swarm of locusts" in out.lower(), "the summoned locust swarm is standing in the room")
cmd(sc, "dismiss")
sc.close()

druid_name, druid_pw = f"Petdrub{_suffix}", "petdrupw1234"
sd = make_char(druid_name, druid_pw, "5")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{druid_name}';")
cmd(sd, "quit!")
sd.close()
set_caster(druid_name, 35)
grant_proficiency(druid_name, "animal companion")
sd = relog(druid_name, druid_pw)
give_component(sd)
out = cmd(sd, "cast animal companion")
check("beast" in out.lower(), "casting animal companion produces a beast-companion flavor message")
out = cmd(sd, "look")
check("a gray wolf" in out.lower(), "the summoned wolf is standing in the room")
cmd(sd, "dismiss")
sd.close()

s_imm.close()

sql(f"DELETE FROM room WHERE vnum IN ({ROOM},{ROOM2});")
sql(f"DELETE FROM roomexit WHERE vnum={ROOM};")
sql(f"DELETE FROM mob WHERE vnum={MOB_VNUM};")
sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT},{SYMBOL});")
for nm in (imm_name, mage_name, cleric_name, druid_name):
    sql(f"DELETE FROM player_skill WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{nm}');")
    sql(f"DELETE FROM player WHERE name='{nm}';")

announce_done("smoke_test_pet")
print("=== ALL CHECKS PASSED ===")
