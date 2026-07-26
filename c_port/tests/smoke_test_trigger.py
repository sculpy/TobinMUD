#!/usr/bin/env python3
"""Smoke test for the mob/object/room scripting system (user, 2026-07-11:
"implement mob object and room scripting -- examine sneezy for ideas --
we want interaction with mobs objs and room via scripts"). The in-game-
authorable alternative to SneezyMUD's hardcoded spec procs (see
db/sneezy/trigger.sql's header comment) -- a builder attaches a trigger
via the menu-driven `edit trigger <room|mob|obj> <vnum>` (2026-07-25
redesign -- see author_trigger() below), then writes a short script
(echo/echoroom/emote/teleport/give/damage/log, one action per line) in
the same shared line editor `edit news`/`edit rules` already use.

  1. `edit trigger` is hidden below BUILD_MIN_LEVEL (the `edit` gate).
  2. Room `enter`: walking into a room fires its script.
  3. Mob `greet`: walking into a mob's room fires its script.
  4. Mob `speech`: saying a matching keyword near a mob fires its script.
  5. Mob `death`: killing a mob fires its script before it's destroyed.
  6. Mob `random`: forced via `aitick`, fires ambient scripts.
  7. Room `random`: forced via `aitick`, fires ambient scripts.
  8. Obj `get`: picking up an object fires its script (damage action).
  9. Obj `wear`: wearing an object fires its script (echo action).
  10. `edit trigger list`/`edit trigger delete` manage existing triggers.

    python3 tests/smoke_test_trigger.py [host] [port]
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


announce("smoke_test_trigger")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time()) % 60000)
ROOM_B = ROOM_A + 1
MOB_GREET = ROOM_A + 2
MOB_DEATH = ROOM_A + 3
MOB_RANDOM = ROOM_A + 4
OBJ_GET = ROOM_A + 5
OBJ_WEAR = ROOM_A + 6


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


def author_trigger(sock, target_type, vnum, trigger_type, match_or_chance, script_lines):
    """Authors a trigger via the menu-driven `edit trigger` flow (2026-07-25
    redesign, replacing the old one-shot `edit trigger <type> <vnum>
    <trigger_type> [match|chance]` command). Returns the concatenated
    transcript of every response along the way, so existing call sites can
    still check for "Writing trigger"/"Trigger saved" substrings in it."""
    out = cmd(sock, f"edit trigger {target_type} {vnum}")
    out += cmd(sock, "a")
    out += cmd(sock, trigger_type)
    if trigger_type == "speech" or (trigger_type == "random" and match_or_chance is not None):
        out += cmd(sock, str(match_or_chance))
    for line in script_lines:
        out += cmd(sock, line)
    out += cmd(sock, "/s")
    cmd(sock, "")  # leave the trigedit menu
    return out


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keyword, actions=0):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'desc',{actions},0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


WEAR_TAKE = 1
WEAR_BODY = 8


def make_obj(vnum, keyword, wear_flag):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} is lying here.',12,{wear_flag},1);")


imm_name = f"Trigimm{_suffix}"
imm_pw = "trigimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- 1: edit trigger is hidden below BUILD_MIN_LEVEL ---
mort_name = f"Trigmort{_suffix}"
mort_pw = "trigmortpw123"
sm = socket.create_connection((host, port), timeout=5)
make_char(sm, mort_name, mort_pw)
out = cmd(sm, f"edit trigger room {ROOM_A}")
check("Command not found" in out, "edit trigger is hidden from a mortal")
sm.close()

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Trigger Room A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Trigger Room B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_A}, 1, '', '', 0, 0, 0, 0, 0, {ROOM_B});")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_B}, 3, '', '', 0, 0, 0, 0, 0, {ROOM_A});")

check("Trigger Room A" in cmd(s, f"goto {ROOM_A}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 2: room "enter" trigger ---
out = author_trigger(s, "room", ROOM_B, "enter", None, ["echo Welcome to the shrine."])
check("Writing trigger" in out, "edit trigger opens the script editor")
check("Trigger saved" in out, "the room enter trigger saves")

mort2_name = f"Trigwalk{_suffix}"
mort2_pw = "trigwalkpw123"
sw = socket.create_connection((host, port), timeout=5)
make_char(sw, mort2_name, mort2_pw)
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mort2_name}';")
cmd(sw, "quit!")
sw.close()
sw = login(mort2_name, mort2_pw)
check("Trigger Room A" in cmd(sw, "look"), "the walker lands in room A")

out = cmd(sw, "east")
check("Welcome to the shrine." in out, "the room enter trigger fired for the walker")

# --- 3: mob "greet" trigger ---
make_mob(MOB_GREET, f"greeter{_suffix}")
check("Trigger Room B" in cmd(s, f"goto {ROOM_B}"), "immortal goes to room B to load the greeter")
check("You conjure" in cmd(s, f"load mob {MOB_GREET}"), "the greeter mob is loaded")
out = author_trigger(s, "mob", MOB_GREET, "greet", None, ["emote nods at the newcomer."])
check("Writing trigger" in out, "edit trigger mob greet opens the script editor")
check("Trigger saved" in out, "the mob greet trigger saves")

cmd(sw, "west")
out = cmd(sw, "east")
check("nods at the newcomer." in out, "the mob greet trigger fired for the walker")

# --- 4: mob "speech" trigger ---
out = author_trigger(s, "mob", MOB_GREET, "speech", "password",
                     ["echo The greeter winks knowingly."])
check("Writing trigger" in out, "edit trigger mob speech opens the script editor")
check("Trigger saved" in out, "the mob speech trigger saves")

out = cmd(sw, "say password")
check("The greeter winks knowingly." in out, "the mob speech trigger fired on the matching keyword")
out = cmd(sw, "say something unrelated")
check("winks knowingly" not in out, "the mob speech trigger does NOT fire on a non-matching phrase")

# --- 5: mob "death" trigger ---
make_mob(MOB_DEATH, f"victimmob{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB_DEATH}"), "the death-trigger mob is loaded")
out = author_trigger(s, "mob", MOB_DEATH, "death", None,
                     ["echoroom The crowd cheers as the beast falls!"])
check("Writing trigger" in out, "edit trigger mob death opens the script editor")
check("Trigger saved" in out, "the mob death trigger saves")

out = cmd(s, f"kill victimmob{_suffix}")
witness = recv_all(sw, timeout=1.0)
check("The crowd cheers as the beast falls!" in (out + witness),
      "the mob death trigger fired when the mob was killed")

# --- 6: mob "random" trigger, forced via aitick ---
make_mob(MOB_RANDOM, f"ambientmob{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB_RANDOM}"), "the random-trigger mob is loaded")
out = author_trigger(s, "mob", MOB_RANDOM, "random", 100,
                     ["echoroom The ambient mob mutters to itself."])
check("Writing trigger" in out, "edit trigger mob random opens the script editor")
check("Trigger saved" in out, "the mob random trigger saves")

out = cmd(s, "aitick 1")
check("The ambient mob mutters to itself." in out, "the mob random trigger fired via aitick (100% chance)")

# --- 7: room "random" trigger, forced via aitick ---
out = author_trigger(s, "room", ROOM_B, "random", 100, ["echoroom The floor hums faintly."])
check("Writing trigger" in out, "edit trigger room random opens the script editor")
check("Trigger saved" in out, "the room random trigger saves")

out = cmd(s, "aitick 1")
check("The floor hums faintly." in out, "the room random trigger fired via aitick (100% chance)")

# --- 8: obj "get" trigger (damage action) ---
make_obj(OBJ_GET, f"stickerbush{_suffix}", WEAR_TAKE)
check("You conjure" in cmd(s, f"load obj {OBJ_GET}"), "the sticker-bush object is loaded")
# `load` now puts the item straight into the loading immortal's own
# inventory (2026-07-22), not the room floor -- drop it explicitly so
# `sw` (a different character) can pick it up.
cmd(s, f"drop stickerbush{_suffix}")
out = author_trigger(s, "obj", OBJ_GET, "get", None,
                     ["echo Ouch! The thorns prick your fingers.", "damage 3"])
check("Writing trigger" in out, "edit trigger obj get opens the script editor")
check("Trigger saved" in out, "the obj get trigger saves")

hp_before_m = re.search(r"HP:\s*(-?\d+) \((\d+) Max", cmd(sw, "score"))
out = cmd(sw, f"get stickerbush{_suffix}")
check("Ouch! The thorns prick your fingers." in out, "the obj get trigger fired (echo)")
hp_after_m = re.search(r"HP:\s*(-?\d+) \((\d+) Max", cmd(sw, "score"))
check(int(hp_after_m.group(1)) == int(hp_before_m.group(1)) - 3,
      "the obj get trigger fired (damage 3 applied)")

# --- 9: obj "wear" trigger (echo action) ---
make_obj(OBJ_WEAR, f"warmcloak{_suffix}", WEAR_TAKE | WEAR_BODY)
check("You conjure" in cmd(s, f"load obj {OBJ_WEAR}"), "the warm cloak object is loaded")
cmd(s, f"drop warmcloak{_suffix}")
out = author_trigger(s, "obj", OBJ_WEAR, "wear", None, ["echo You feel a strange warmth."])
check("Writing trigger" in out, "edit trigger obj wear opens the script editor")
check("Trigger saved" in out, "the obj wear trigger saves")

cmd(sw, f"get warmcloak{_suffix}")
out = cmd(sw, f"wear warmcloak{_suffix}")
check("You feel a strange warmth." in out, "the obj wear trigger fired")

# --- 10: list/delete ---
out = cmd(s, f"edit trigger list {MOB_GREET}")
check("greet" in out and "speech" in out, "edit trigger list shows both triggers on the greeter")

m = re.search(r"#(\d+) mob \d+ speech", out)
check(m is not None, "the speech trigger's id is visible in the listing")
trig_id = m.group(1)
out = cmd(s, f"edit trigger delete {trig_id}")
check("Trigger deleted" in out, "edit trigger delete removes it")
out = cmd(s, f"edit trigger list {MOB_GREET}")
check("speech" not in out, "the deleted speech trigger no longer appears")
check("greet" in out, "the greet trigger is untouched")

# --- Teardown: this test's fixtures (rooms/mobs/objs) are throwaway, but
# every OTHER trigger created above is still live -- including two 100%-
# chance "random" ones that fire on every real aitick pulse. Left alone,
# these turned into permanent ambient noise that corrupted unrelated
# tests' output when the ambient mob wandered into a busy shared room
# (discovered 2026-07-11: 91 of 93 rows in `trigger` were orphans from
# earlier runs of this exact file). Delete every trigger still attached
# to any target this run created, not just the one demo-deleted above.
for vnum in (ROOM_A, ROOM_B, MOB_GREET, MOB_DEATH, MOB_RANDOM, OBJ_GET, OBJ_WEAR):
    listing = cmd(s, f"edit trigger list {vnum}")
    for trig_id in re.findall(r"#(\d+)", listing):
        cmd(s, f"edit trigger delete {trig_id}")

s.close()
sw.close()
announce_done("smoke_test_trigger")
print("=== ALL CHECKS PASSED ===")
