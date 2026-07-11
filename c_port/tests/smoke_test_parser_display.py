#!/usr/bin/env python3
"""Smoke test for three related fixes:
  1. Abbreviation-based command parsing (e.g. "sc" reaches "score", "l"
     reaches "look") -- but "quit" is deliberately excluded, only the
     exact literal "quit!" works (covered more thoroughly in the other
     smoke_test_quit*.py files; this file just spot-checks that "qu"
     does NOT reach quit).
  2. A trailing "> " prompt shown after every command while playing.
  3. CRLF normalization: room descriptions (DB-sourced, Unix line endings
     internally) must render with a real \\r\\n before every line, not a
     bare \\n -- checked at the raw-byte level, not just substring matching.

    python3 tests/smoke_test_parser_display.py [host] [port]
"""
import socket
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


announce("smoke_test_parser_display")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"ParserTester{_suffix}"
char_name = f"Parser{_suffix}"
password = "parsertestpw123"


def recv_all_bytes(sock, timeout=1.0):
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
    return b"".join(chunks)


def recv_all(sock, timeout=1.0):
    return recv_all_bytes(sock, timeout).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def step(sock, label, line):
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name", account_name)
step(s, "confirm new account creation", "y")
step(s, "password (first entry)", password)
step(s, "confirm password -> menu", password)
step(s, "new", "new")
step(s, "char name -> attr screen", char_name)
out = step(s, "done -> playing (auto look)", "done")
check(out.rstrip().endswith(">"), "auto-look after character creation ends with a prompt")

# --- 1. Abbreviation matching ---
out = step(s, "'l' should reach look", "l")
check("Center Square" in out, "'l' abbreviates to look (new default load room 100)")

# Movement sits above who in the table (classic Diku): the single letter
# 'w' means WEST now; who needs 'wh'. Either outcome of the walk (moving,
# or "can't go that way") proves 'w' matched west rather than who.
out = step(s, "'w' should reach west (movement), not who", "w")
check("Who's online" not in out and "Huh?!" not in out,
      "'w' abbreviates to west, not who")

out = step(s, "'wh' should reach who", "wh")
check("Who's online" in out, "'wh' abbreviates to who")

out = step(s, "'sc' should reach score", "sc")
check(char_name in out and "Strength" in out, "'sc' abbreviates to score")

out = step(s, "'sco' should also reach score", "sco")
check("Strength" in out, "'sco' (longer prefix) also abbreviates to score")

out = step(s, "'qu' must NOT reach quit", "qu")
check("Huh?!" in out, "'qu' does not abbreviate to quit -- quit is excluded from the parser")

# --- 2. Trailing prompt after every command ---
out = step(s, "look shows a trailing prompt", "look")
check(out.rstrip().endswith(">"), "look's output ends with a prompt")

# Prompt customization (Session 21): prompt hp puts HP in the prompt.
out = step(s, "toggle hp into the prompt", "prompt hp")
check("now show hit points" in out, "prompt hp confirms the toggle")
out = step(s, "prompt now shows HP", "look")
tail = out.rstrip()
check(tail.endswith(">") and "HP:" in tail[-20:], "the prompt line carries HP: <n>")
out = step(s, "toggle hp back off", "prompt hp")
check("no longer" in out, "prompt hp toggles back off")
out = step(s, "prompt is plain again", "look")
check("HP:" not in out.rstrip()[-20:], "the plain prompt returns")

out = step(s, "who shows a trailing prompt", "who")
check(out.rstrip().endswith(">"), "who's output ends with a prompt")

out = step(s, "score shows a trailing prompt", "score")
check(out.rstrip().endswith(">"), "score's output ends with a prompt")

out = step(s, "even an unknown command shows a trailing prompt", "gibberish")
check(out.rstrip().endswith(">"), "unknown-command output ends with a prompt too")

# --- 3. CRLF normalization, checked at the raw-byte level ---
send_line(s, "look")
raw = recv_all_bytes(s)
print(f"=== look (raw bytes) ===\n{raw!r}")
# Every '\n' must be immediately preceded by '\r' -- no bare LFs anywhere,
# including inside the room description text itself (which is DB-sourced
# with Unix line endings baked in).
bare_lf_positions = [i for i in range(len(raw)) if raw[i:i+1] == b"\n" and raw[i-1:i] != b"\r"]
check(bare_lf_positions == [], f"no bare LF (missing \\r) anywhere in the room output, found at {bare_lf_positions}")
check(b"\r\n" in raw, "sanity check: the output does contain proper CRLF sequences")

s.close()
announce_done("smoke_test_parser_display")
print("=== ALL CHECKS PASSED ===")
