#!/usr/bin/env python3
"""Smoke test for objects (Phase 2C: obj_t, oload, get/drop/inventory/wear/
remove/equipment, persistence, and drop-on-death). Covers:
  1. `oload` is immortal-only (invisible to a mortal, "Huh?!").
  2. `oload <vnum>` spawns a prototype into the room; `look` lists it.
     `oload <name>` works too (a substring match against `obj.name`).
     `look <item>` (room floor or carried) shows its description.
  3. `get` moves a floor object into inventory; a no-TAKE object is refused.
  4. `wear` moves a carried item into the right body slot (or the primary
     hand for a holdable item); refuses an already-occupied slot.
  5. `remove`/`drop` reverse those moves; `equipment`/`inventory` reflect
     the current state.
  6. Carried + worn + held instances persist across a reconnect.
  7. Losing a fight drops everything the loser had (carried, worn, held)
     into the room they died in.

All setup happens in SQL-bootstrapped sandbox rooms/objects at high vnums
(900000+); the seeded world and its `obj` table content are never touched.

    python3 tests/smoke_test_objects.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_objects")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 80000)

ROOM = BASE
WEARABLE = BASE + 1   # takeable, wearable on the body
FIXED = BASE + 2      # not takeable -- fixed scenery
WEAPON = BASE + 3     # takeable, holdable (wielded)
WEARABLE2 = BASE + 4  # a second body-slot item, for the already-worn check
WEARABLE3 = BASE + 5  # the victim's gear, for the drop-on-death check
NAMESEARCH = BASE + 6 # a uniquely-named fixture for the oload-by-name check

WEAR_TAKE = 1
WEAR_BODY = 8
WEAR_HOLD = 16384


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


def query(stmt):
    return subprocess.run(["mariadb", "-N", "sneezy", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def obj_insert(vnum, name, short_desc, long_desc, item_type, wear_flag):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{name}','{short_desc}','{long_desc}',{item_type},{wear_flag},1);")


imm_name = f"Objtest{_suffix}"
imm_pw = "objtestpw123"
victim_name = f"Objvict{_suffix}"
victim_pw = "objvictpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- bootstrap: one sandbox room, five sandbox object prototypes ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Object Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
obj_insert(WEARABLE, "tunic", "<w>a plain tunic<z>", "A plain tunic is lying here.", 11, WEAR_TAKE | WEAR_BODY)
obj_insert(FIXED, "statue", "<w>a large statue<z>", "A large statue stands here, far too heavy to lift.", 39, 0)
obj_insert(WEAPON, "dagger", "<w>a rusty dagger<z>", "A rusty dagger is lying here.", 5, WEAR_TAKE | WEAR_HOLD)
obj_insert(WEARABLE2, "vest", "<w>a leather vest<z>", "A leather vest is lying here.", 11, WEAR_TAKE | WEAR_BODY)
obj_insert(WEARABLE3, "cloak", "<w>a tattered cloak<z>", "A tattered cloak is lying here.", 11, WEAR_TAKE | WEAR_BODY)
# A distinctively-named fixture (unique random tag) so "oload <name>" can be
# tested deterministically -- a common word like "sword" could match some
# real seeded object at a lower vnum instead of this fixture.
namesearch_word = f"zzztestsword{_suffix}"
obj_insert(NAMESEARCH, namesearch_word, f"<w>a {namesearch_word}<z>",
           f"A {namesearch_word} is lying here.", 5, WEAR_TAKE)

check("Object Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: oload is immortal-only ---
s2 = socket.create_connection((host, port), timeout=5)
mort_name = f"Objmort{_suffix}"
make_char(s2, mort_name, "objmortpw123")
cmd(s2, "color off")
check("Huh?!" in cmd(s2, f"oload {WEARABLE}"), "oload is invisible to a mortal")
s2.close()

# --- 2: oload spawns each prototype; look lists them ---
check("You conjure" in cmd(s, f"oload {WEARABLE}"), "oload confirms (wearable)")
check("You conjure" in cmd(s, f"oload {FIXED}"), "oload confirms (fixed scenery)")
check("You conjure" in cmd(s, f"oload {WEAPON}"), "oload confirms (weapon)")
out = cmd(s, "look")
check("A plain tunic is lying here." in out, "look lists the wearable object's long_desc")
check("too heavy to lift" in out, "look lists the fixed object's long_desc")
check("A rusty dagger is lying here." in out, "look lists the weapon's long_desc")

# --- 2b: oload also accepts a name/keyword, not just a vnum ---
check("You conjure" in cmd(s, f"oload {namesearch_word}"), "oload accepts a name in place of a vnum")
check(f"A {namesearch_word} is lying here." in cmd(s, "look"), "the name-looked-up object actually spawned")

# --- 2c: look <item> shows an object's description (and condition, if it has one) ---
out = cmd(s, "look tunic")
check("A plain tunic is lying here." in out, "look <item> on the room floor shows its description")
out = cmd(s, "look dagger")
check("A rusty dagger is lying here." in out, "look <item> works for a second room-floor object too")

# --- 3: get / no-TAKE refusal / inventory ---
check("can't take that" in cmd(s, "get statue"), "get refuses a fixed (no-TAKE) object")
check("statue" in cmd(s, "look"), "the refused statue is still on the floor")
check("You get" in cmd(s, "get tunic"), "get picks up the wearable object")
check("tunic" not in cmd(s, "look"), "the room no longer lists the tunic after get")
out = cmd(s, "inventory")
check("tunic" in out, "inventory lists the carried tunic")
check("You get" in cmd(s, "get dagger"), "get picks up the dagger")

# look <item> also finds something you're merely carrying (no longer on the floor).
out = cmd(s, "look tunic")
check("A plain tunic is lying here." in out, "look <item> also finds a carried (non-floor) item")

# --- 4: wear / already-occupied refusal / equipment ---
out = cmd(s, "wear tunic")
check("wear" in out and "body" in out, "wear equips the tunic on the body slot")
out = cmd(s, "equipment")
check("<body>" in out and "tunic" in out, "equipment shows the tunic on the body")

cmd(s, f"oload {WEARABLE2}")
cmd(s, "get vest")
check("already wearing something there" in cmd(s, "wear vest"), "wear refuses an occupied slot")

out = cmd(s, "wear dagger")
check("wield" in out, "wear on a holdable item wields it instead")
out = cmd(s, "equipment")
check("primary hand" in out and "dagger" in out, "equipment shows the dagger in the primary hand")

# --- 5: remove / drop ---
out = cmd(s, "remove tunic")
check("remove" in out, "remove takes the tunic back off")
check("tunic" in cmd(s, "inventory"), "the removed tunic is back in inventory")
out = cmd(s, "drop tunic")
check("drop" in out, "drop confirms")
check("tunic" not in cmd(s, "inventory"), "inventory no longer lists the dropped tunic")
check("A plain tunic is lying here." in cmd(s, "look"), "look shows the dropped tunic back on the floor")

# --- 6: persistence across a reconnect (still carrying vest, still wielding dagger) ---
s.close()
s = login(imm_name, imm_pw)
out = cmd(s, "equipment")
check("primary hand" in out and "dagger" in out, "the held dagger survived a reconnect")
check("vest" in cmd(s, "inventory"), "the carried vest survived a reconnect")

# --- 7: drop-on-death -- the victim's gear scatters into the room they died in ---
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sv.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
sv = login(victim_name, victim_pw)
check("Object Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")
check("Object Sandbox" in cmd(s, f"goto {ROOM}"), "the immortal returns to the sandbox room (a reconnect lands at the default room, not where they last stood)")
check("You conjure" in cmd(s, f"oload {WEARABLE3}"), "oload confirms (the victim's gear)")
check("You get" in cmd(sv, "get cloak"), "the victim picks up their gear")
check("wear" in cmd(sv, "wear cloak"), "the victim wears their gear")

check("slain" in cmd(s, f"kill {victim_name}").lower(), "an immortal's kill instakills the victim")
out = cmd(s, "look")
check("A tattered cloak is lying here." in out, "the victim's gear dropped into the room on defeat")

sv.close()
sv = login(victim_name, victim_pw)
check("Nothing." in cmd(sv, "inventory"), "the victim's inventory is empty after reconnecting post-defeat")
out = cmd(sv, "equipment")
check("nothing" in out and "cloak" not in out, "the victim isn't still wearing the dropped cloak")

s.close()
sv.close()
announce_done("smoke_test_objects")
print("=== ALL CHECKS PASSED ===")
