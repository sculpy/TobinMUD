#!/usr/bin/env python3
"""Smoke test for redit's Extra Descriptions submenu (menu 8 of `edit room`)
-- the BUILDER-facing half of extra descriptions (the roomextra table).
The MORTAL-facing half (`look <keyword>` reveals one) already has its own
tests/smoke_test_extra_desc.py; this file covers authoring them in-game
instead of via direct SQL. Unlike the rest of redit, every action here
commits to the DB immediately (no working-copy Save/Quit) -- see
room_repo.h's comment on room_repo_extra_save() et al. for why.

Covered:
  1. Menu 8 opens the Extra Descriptions submenu; empty room shows "none yet".
  2. A)dd: keywords -> description (via the shared line editor) -> saved to
     the roomextra table (vnum, name, description).
  3. The new entry shows up, numbered, back at the list.
  4. Picking a numbered entry opens its detail view (keywords + description).
  5. 1) Keywords renames the entry (description untouched); a rename that
     collides with a different existing entry's exact keyword string fails
     cleanly, leaving both entries alone.
  6. 2) Description edits and re-saves the description text.
  7. 3) Delete removes one entry; Z) Delete ALL clears every entry for the
     room.
  8. Cancelling "Add" (blank at the keywords prompt) creates nothing.
  9. Aborting the description editor (/a) on a BRAND-NEW add creates
     nothing and returns to the list; aborting on an EXISTING entry's
     description leaves it unchanged and returns to that entry's detail
     view instead.
  10. End-to-end: an extra description authored this way is immediately
      revealed by `look <keyword>` (cmd_look.c's existing mortal-facing
      lookup) -- ties the builder half back into the already-shipped read
      path.

All setup is in a SQL-bootstrapped sandbox room at a high vnum (900000+);
the seeded world and its roomextra content are never touched.

    python3 tests/smoke_test_redit_extradesc.py [host] [port]
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


announce("smoke_test_redit_extradesc")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)


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
                          check=True, capture_output=True, text=True).stdout


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def extras_for(vnum):
    """Every (name, description) row in roomextra for `vnum`, alphabetical."""
    out = query(f"SELECT name, description FROM roomextra WHERE vnum={vnum} ORDER BY name;")
    rows = [line.split("\t") for line in out.splitlines() if line]
    return rows


imm_name = f"Xdesc{_suffix}"
imm_pw = "xdescpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "2"); recv_all(s)  # alignment: neutral

set_level(imm_name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Extra Desc Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Extra Desc Sandbox" in cmd(s, f"goto {BASE}"),
      "goto lands in the SQL-bootstrapped sandbox room")

kw_calendar = f"calendar{_suffix}"
kw_clock = f"clock{_suffix}"
kw_poster = f"poster{_suffix}"

# --- 1: menu 8 opens the (empty) Extra Descriptions submenu ---
cmd(s, "edit room")
out = cmd(s, "8")
check("Extra Descriptions for" in out and "none yet" in out and "A) Add new" in out,
      "menu 8 opens the Extra Descriptions submenu, empty for a fresh room")

# --- 2/3: Add a new extra description ---
out = cmd(s, "A")
check("Enter keywords" in out, "A) Add prompts for keywords")
out = cmd(s, kw_calendar)
check("Now enter the description" in out, "keywords accepted, moves straight into the description editor")
cmd(s, "Grumble car payment due on the 25th.")
out = cmd(s, "/s")
check("Extra description saved" in out and f"Extra Description: {kw_calendar}" in out,
      "/s saves the new extra description and lands on its detail view")

rows = extras_for(BASE)
check(len(rows) == 1 and rows[0][0] == kw_calendar
      and rows[0][1].strip() == "Grumble car payment due on the 25th.",
      "the new extra description persisted to roomextra (name + description)")

# --- 4: back to the list -- the new entry is numbered ---
out = cmd(s, "")
check(f" 1) {kw_calendar}" in out, "the list now shows the new entry, numbered")

# --- 5: rename via 1) Keywords ---
out = cmd(s, "1")
check("Keywords" in out and "detail view" not in out, "picking the entry opens its detail view")
out = cmd(s, "1")
check(f"Current keywords: {kw_calendar}" in out, "1) Keywords shows the current keywords")
out = cmd(s, kw_clock)
check("Keywords updated" in out and f"Extra Description: {kw_clock}" in out,
      "renaming updates the detail view's header")

rows = extras_for(BASE)
check(len(rows) == 1 and rows[0][0] == kw_clock
      and rows[0][1].strip() == "Grumble car payment due on the 25th.",
      "the rename persisted; the description was untouched")

# --- add a second entry, then verify a colliding rename fails cleanly ---
cmd(s, "")  # back to list
out = cmd(s, "A")
cmd(s, kw_poster)
cmd(s, "A poster of a smiling farmer advertises fresh produce.")
out = cmd(s, "/s")
check("Extra description saved" in out, "a second, differently-keyworded entry saves fine")

out = cmd(s, "")  # back to list
check(f"{kw_clock}" in out and f"{kw_poster}" in out, "the list now shows both entries")

# Open the clock entry and try to rename it to collide with the poster entry.
idx_clock = 1 if out.index(kw_clock) < out.index(kw_poster) else 2
out = cmd(s, str(idx_clock))
check(f"Extra Description: {kw_clock}" in out, "reopened the clock entry")
cmd(s, "1")
out = cmd(s, kw_poster)
check("Rename failed" in out, "renaming to an already-used keyword string is refused")

rows = extras_for(BASE)
names = sorted(r[0] for r in rows)
check(names == sorted([kw_clock, kw_poster]),
      "both entries still exist, untouched, after the failed rename")

# --- 6: edit description text on an existing entry ---
out = cmd(s, "2")
check("Editing extra description" in out and "Grumble car payment due on the 25th." in out,
      "2) Description opens the line editor pre-loaded with the current text")
cmd(s, "/b")
cmd(s, "Rescheduled: car payment now due on the 28th.")
out = cmd(s, "/s")
check("Extra description saved" in out, "the edited description saves")

rows = extras_for(BASE)
clock_row = next(r for r in rows if r[0] == kw_clock)
check(clock_row[1].strip() == "Rescheduled: car payment now due on the 28th.",
      "the edited description text persisted")

# --- 9a: aborting the description editor on a BRAND-NEW add creates nothing ---
out = cmd(s, "")  # back to list
before = len(extras_for(BASE))
out = cmd(s, "A")
cmd(s, f"ghost{_suffix}")
cmd(s, "This should never be saved.")
out = cmd(s, "/a")
check("unchanged" in out and "Extra Descriptions for" in out,
      "aborting a brand-new add returns to the LIST (nothing to show a detail view for)")
check(len(extras_for(BASE)) == before, "aborting a brand-new add created no row")

# --- 9b: aborting the description editor on an EXISTING entry changes nothing ---
out = cmd(s, str(idx_clock))
check(f"Extra Description: {kw_clock}" in out, "reopened the clock entry")
cmd(s, "2")
cmd(s, "/b")
cmd(s, "This edit should be discarded.")
out = cmd(s, "/a")
check("unchanged" in out and f"Extra Description: {kw_clock}" in out,
      "aborting an existing entry's description edit returns to ITS detail view")
rows = extras_for(BASE)
clock_row = next(r for r in rows if r[0] == kw_clock)
check(clock_row[1].strip() == "Rescheduled: car payment now due on the 28th.",
      "the aborted description edit did not persist")

# --- 8: cancelling Add (blank at the keywords prompt) creates nothing ---
out = cmd(s, "")  # back to list
before = len(extras_for(BASE))
out = cmd(s, "A")
out = cmd(s, "")
check("Cancelled" in out, "blank at the keywords prompt cancels the add")
check(len(extras_for(BASE)) == before, "cancelling Add created no row")

# --- 10: end-to-end -- look <keyword> reveals what redit just authored ---
out = cmd(s, "")  # list -> back to the main room menu (blank always returns
                   # one level up; we're already at the list after the cancel above)
out = cmd(s, "Q")
check("Leaving the room editor" in out, "Q) leaves the room editor")
out = cmd(s, f"look {kw_clock}")
check("Rescheduled: car payment now due on the 28th." in out,
      "look <keyword> reveals the extra description just authored via redit")
out = cmd(s, f"look {kw_poster}")
check("A poster of a smiling farmer" in out,
      "look <keyword> reveals the second entry too")

# --- 7: delete one entry, then delete ALL ---
cmd(s, "edit room")
cmd(s, "8")
out = cmd(s, str(idx_clock))
check(f"Extra Description: {kw_clock}" in out, "reopened the clock entry for deletion")
out = cmd(s, "3")
check("(yes/no)" in out, "3) Delete asks for confirmation")
out = cmd(s, "yes")
check("Extra description deleted" in out, "confirming deletes the entry")
rows = extras_for(BASE)
check(len(rows) == 1 and rows[0][0] == kw_poster,
      "only the clock entry was removed; the poster entry remains")

out = cmd(s, "Z")
check("(yes/no)" in out, "Z) Delete ALL asks for confirmation")
out = cmd(s, "yes")
check("All extra descriptions deleted" in out and "none yet" in out,
      "confirming clears every remaining entry, and the re-shown list reflects it")
check(len(extras_for(BASE)) == 0, "roomextra is now empty for this room")

cmd(s, "")   # list -> back to the main room menu
cmd(s, "Q")  # leave the room editor
set_level(imm_name, 1)
s.close()
announce_done("smoke_test_redit_extradesc")
print("=== ALL CHECKS PASSED ===")
