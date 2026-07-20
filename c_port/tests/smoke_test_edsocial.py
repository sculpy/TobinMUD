#!/usr/bin/env python3
"""Smoke test for the menu-driven social editor (`edit social [name]`,
55+, TODO.md's "Socials -> DB + full Sneezy set + edsocial" item -- the
DB-port half shipped earlier the same session; this is the editor half).
Covers:

  1. A level-1 mortal can't reach `edit social` at all (gated out below
     EDSOCIAL_MIN_LEVEL, same "Command not found" wording every other
     under-level editor gives).
  2. A 55+ immortal typing bare `edit social` sees the full list.
  3. `new <name>` creates a brand-new, blank social and jumps straight to
     its detail view.
  4. Editing one of the 8 message fields takes effect IMMEDIATELY for a
     separate mortal actually using the social -- no restart, no relog
     (this is the whole point of the "commits immediately, cache reload
     after every write" design, socials.h's own doc comment).
  5. Toggling H(ide) round-trips correctly but has no bearing on the
     player-facing `socials` list or the verb's usability -- it's the
     upstream act()'s per-recipient invisibility gate, inert until Tobin
     has an invisibility system.
  6. Setting P(osition) actually gates the social for a mortal (mirrors
     smoke_test_socials.py's dance/min_position check, but exercised
     through the editor instead of raw SQL).
  7. R)ename: the old name stops working, the new one works, and the
     editor's own prompt/detail view follows the rename.
  8. D)elete: the social stops working for players and disappears from
     the list.
  9. Exact-name shortcut: `edit social <existing name>` jumps straight to
     the detail view instead of the list.

Cleans up its own sandbox social row(s) at the end regardless of outcome
(a stray test social left behind would visibly pollute the real
`socials` list for players).

    python3 tests/smoke_test_edsocial.py [host] [port]
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


announce("smoke_test_edsocial")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
SOC1 = f"zzed{_suffix}"
SOC2 = f"zzrn{_suffix}"  # rename target


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


def cmd_paged(sock, line, timeout=1.0, max_pages=30):
    # The player-facing `socials` list is alphabetical and paginated
    # (cmd_socials.c) -- a 'z'-prefixed test social lands on the LAST
    # page, not necessarily the first, so a single-page read can't be
    # trusted to confirm presence OR absence. Same drain-fully pattern as
    # smoke_test_socials.py's own paging check.
    out = cmd(sock, line, timeout)
    full = out
    resp = out
    guard = 0
    while "ENTER for more" in resp and guard < max_pages:
        resp = cmd(sock, "", timeout)
        full += resp
        guard += 1
    return full


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "done", "2"):
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


def cleanup():
    sql(f"DELETE FROM social WHERE name IN ('{SOC1}', '{SOC2}');")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name LIKE 'Zzed{_suffix}%');")
    sql(f"DELETE FROM player WHERE name LIKE 'Zzed{_suffix}%';")


try:
    imm_name = f"Zzed{_suffix}"
    mort_name = f"Zzedm{_suffix}"
    pw = "edsocialpw123"

    make_char(imm_name, pw)
    make_char(mort_name, pw)
    set_level(imm_name, 55)

    # --- 1: below-level mortal can't reach edit social at all ---
    mort_lowlevel = login(mort_name, pw)
    check("Command not found" in cmd(mort_lowlevel, "edit social"),
          "a level-1 mortal cannot reach `edit social` (gated below EDSOCIAL_MIN_LEVEL)")
    mort_lowlevel.close()

    imm = login(imm_name, pw)

    # --- 2: bare `edit social` shows the list ---
    out = cmd(imm, "edit social")
    check("=== Socials" in out, "bare `edit social` shows the full list")
    check("edsocial>" in out, "the list prompt is shown")
    cmd(imm, "")  # blank -> quit back to CONN_PLAYING, don't leave mid-editor

    # --- 3: `new` creates a blank social and jumps to its detail view ---
    out = cmd(imm, "edit social")
    out = cmd(imm, "new")
    check("Enter the new social's name" in out, "list's 'new' prompts for a name")
    out = cmd(imm, SOC1)
    check(f"Editing social: {SOC1}" in out, "new social created and opened for editing")
    check("(empty)" in out, "a brand-new social's message fields start empty")

    # --- 4: editing field 1 (self, no target) takes effect immediately
    # for a SEPARATE mortal, no restart/relog needed. The brand-new social
    # already exists (in cache, verb recognized) but its self_no_arg is
    # still blank, so social_try() matches and handles it -- silently,
    # sending nothing -- rather than falling through to "Command not
    # found". That silence (no crash, no stray text) IS "not usable yet"
    # here. ---
    mort = login(mort_name, pw)
    check("zzed happily" not in cmd(mort, SOC1),
          "the brand-new social produces no text yet (still has no self_no_arg message)")

    out = cmd(imm, "1")
    check("Current (Self (no target))" in out, "field 1 prompts with its label")
    cmd(imm, "You zzed happily.")
    out = cmd(mort, SOC1)
    check("You zzed happily." in out,
          "editing field 1 makes the social usable IMMEDIATELY for another connection, "
          "with no restart -- the whole point of edsocial's immediate-commit design")

    # --- 5: H toggles `hide` -- the upstream act()'s per-recipient
    # invisibility gate (sys/comm.cc: don't show this message to someone
    # who can't currently see the actor), NOT a "remove from the socials
    # list" switch -- Tobin has no invisibility system yet, so this field
    # is inert either way. Just confirm the toggle round-trips and that
    # neither state affects the verb's own listing/usability. ---
    out = cmd(imm, "H")
    check("Hide unseen actor:" in out and " yes" in out.split("Hide unseen actor:")[1].split("\n")[0],
          "H toggles hide-unseen to yes")
    check(SOC1 in cmd_paged(mort, "socials"),
          "hide has no bearing on the socials list (no invisibility system yet)")
    check("You zzed happily." in cmd(mort, SOC1), "hide has no bearing on the verb's usability either")
    cmd(imm, "H")  # toggle back off

    # --- 6: P sets the minimum position, actually gating the mortal ---
    out = cmd(imm, "P")
    check("Enter a new one" in out, "P prompts for a new minimum position")
    cmd(imm, "fighting")
    cmd(mort, "sleep")
    check("can't do that right now" in cmd(mort, SOC1),
          "raising the minimum position actually gates a mortal below it")
    cmd(mort, "wake"); cmd(mort, "stand")
    cmd(imm, "P")
    cmd(imm, "dead")  # back to no restriction (position 0), matches most real socials

    # --- 7: rename ---
    out = cmd(imm, "R")
    check("Current name" in out, "R prompts with the current name")
    out = cmd(imm, SOC2)
    check(f"Editing social: {SOC2}" in out, "rename succeeded and the detail view follows it")
    check("Command not found" in cmd(mort, SOC1), "the OLD name no longer works after rename")
    check("You zzed happily." in cmd(mort, SOC2), "the NEW name works after rename")

    # --- 8: delete ---
    out = cmd(imm, "D")
    check("Really delete" in out, "D asks for confirmation before deleting")
    cmd(imm, "yes")
    check("Command not found" in cmd(mort, SOC2),
          "the deleted social no longer exists as a command at all")
    out = cmd(imm, "")  # back at the list after delete
    check(SOC2 not in out.split("Type a name")[0], "the deleted social is gone from the list")

    # --- 9: exact-name shortcut jumps straight to detail view ---
    out = cmd(imm, "edit social smile")
    check("Editing social: smile" in out,
          "`edit social <existing name>` jumps straight to that social's detail view")
    cmd(imm, "")

    imm.close()
    mort.close()

    announce_done("smoke_test_edsocial")
    print("=== ALL CHECKS PASSED ===")
finally:
    cleanup()
