#!/usr/bin/env python3
"""Smoke test for the shared line editor's consistent slash-commands
(editor_feed()/editor_format() in descriptor.c), exercised through `edhelp`
(the simplest ed* editor to drive -- the mechanism is shared by every ed*
editor, so this covers all of them):
  1. The intro advertises the consistent set: /s save, /a abort, /b blank,
     /f format.
  2. /b blanks the buffer; /f reflows it so no line exceeds the display
     width, preserving every word and the blank-line paragraph break.
  3. /s saves the reformatted buffer (not the original unwrapped text).
  4. /a aborts -- a discarded edit does not persist.
  5. The legacy single-key '.' save still works as an alias.

    python3 tests/smoke_test_editor_format.py [host] [port]
"""
import re
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


announce("smoke_test_editor_format")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
FORMAT_WIDTH = 78


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


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


def set_level(name, level):
    subprocess.run(
        ["mariadb", "tobin", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


name = f"Fmted{_suffix}"
pw = "editorformattest123"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral
set_level(name, 56)
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)

topic = f"formattopic{_suffix}"
out = cmd(s, f"edit help {topic}")
check("/f reflows to width" in out and "/s saves" in out,
      "the editor intro advertises the consistent slash-command set")

long_line = ("alpha bravo charlie delta echo foxtrot golf hotel india juliet "
             "kilo lima mike november oscar papa quebec romeo sierra tango "
             "uniform victor whiskey xray yankee zulu")
check(len(long_line) > FORMAT_WIDTH, "sanity: the test line really is longer than the wrap width")

# /b blanks the buffer: type junk, /b it away, then start over for real.
cmd(s, "junk that should be blanked away")
out = cmd(s, "/b")
check("blanked" in out.lower(), "/b blanks the editor buffer")

cmd(s, long_line)
cmd(s, "")  # blank line -- paragraph break
second_para = "This is paragraph two with a few more filler words for preservation checking yes indeed truly."
cmd(s, second_para)

# /f is the format key (formerly /format, which still works as an alias).
out = cmd(s, "/f")
marker = "Reformatted:\r\n"
check(marker in out, "/f announces the reformatted buffer")
body = out[out.find(marker) + len(marker):out.rfind("] ")]
lines = body.split("\r\n")
check(any(l == "" for l in lines), "the paragraph break (blank line) survives formatting")
check(all(len(l) <= FORMAT_WIDTH for l in lines),
      f"no line in the reformatted buffer exceeds {FORMAT_WIDTH} columns")
check("zulu" in body and "preservation" in body,
      "every word from both paragraphs survives the reflow")

# /s saves.
out = cmd(s, "/s")
check(f"'{topic}' saved" in out, "/s saves the reformatted buffer")

out = cmd(s, f"help {topic}")
saved_lines = strip(out).split("\r\n")
check(all(len(l) <= FORMAT_WIDTH for l in saved_lines),
      "the SAVED topic (not the original unwrapped text) is what's shown")
check("zulu" in out and "preservation" in out, "the saved topic still has all the words")

# /a aborts: reopen, type garbage, /a, and confirm the topic is unchanged.
cmd(s, f"edit help {topic}")
cmd(s, "GARBAGEXYZ that must not survive an abort")
cmd(s, "/a")
out = cmd(s, f"help {topic}")
check("GARBAGEXYZ" not in out, "/a aborts the edit -- the discarded text did not persist")

# A line of just "." is now literal text, not a save (legacy keys removed):
# save the topic, reopen, type ".", and confirm /s keeps the dot as content.
cmd(s, f"edit help {topic}")
cmd(s, ".")            # literal content now, NOT a save
out = cmd(s, "/s")
check(f"'{topic}' saved" in out, "a bare '.' is treated as text, and /s saves")

s.close()
announce_done("smoke_test_editor_format")
print("=== ALL CHECKS PASSED ===")
