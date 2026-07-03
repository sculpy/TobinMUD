#!/usr/bin/env python3
"""Smoke test for color code translation, checked at the raw-byte level
(not just substring matching, same discipline as the Session 9 CRLF test).

Self-contained as of Session 20: color tags are injected via `say` (whose
message is broadcast verbatim and translated in descriptor_send like all
output), so no hand-staged DB content is needed anymore. Also covers the
Session 20 auto-reset: a message that sets a color and never resets it
gets an ANSI reset appended, so color can't bleed into the prompt or
subsequent messages.

    python3 tests/smoke_test_color.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = str(int(time.time()) % 100000)


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


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def make_player(tag):
    name = f"Clr{tag}{_suffix}"
    pw = "colortestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all_bytes(s)
    send_line(s, name)
    recv_all_bytes(s)
    send_line(s, pw)
    recv_all_bytes(s)
    send_line(s, "new")
    recv_all_bytes(s)
    send_line(s, name)
    recv_all_bytes(s)
    send_line(s, "done")
    recv_all_bytes(s)
    return s


sA = make_player("A")
sB = make_player("B")

# --- Part 1: color on (default) -- tags in a say become real ANSI escapes ---
send_line(sA, "say <p>purple<z> and <r>red")
rawA = recv_all_bytes(sA)
rawB = recv_all_bytes(sB)
print(f"=== say with tags, color on (speaker raw bytes) ===\n{rawA!r}")
check(b"\x1b[35m" in rawA, "purple ANSI escape (\\x1b[35m) present for the speaker")
check(b"\x1b[31m" in rawA, "red ANSI escape (\\x1b[31m) present for the speaker")
check(b"\x1b[0m" in rawA, "explicit <z> reset translated (\\x1b[0m present)")
# Note: rawA also contains the echo of the typed command line, which
# correctly still has literal tags (echo is raw keystrokes, not output
# translation) -- only the translated "You say" message must be tag-free.
said = rawA[rawA.find(b"You say"):]
check(b"<p>" not in said and b"<z>" not in said and b"<r>" not in said,
      "no raw tag text leaks into the translated message")
check(b"\x1b[35m" in rawB and b"\x1b[31m" in rawB,
      "the other player in the room gets the same translated escapes")

# --- Part 2: auto-reset -- an unterminated color can't bleed past its message ---
idxA = rawA.rfind(b"\x1b[31m")
check(rawA.find(b"\x1b[0m", idxA) != -1,
      "a message ending still-colored gets an ANSI reset appended (no bleed)")
send_line(sA, "say plain follow-up")
rawA2 = recv_all_bytes(sA)
check(b"\x1b[35m" not in rawA2 and b"\x1b[31m" not in rawA2,
      "the next message carries no leftover color codes of its own")

# --- Part 3: color off -- tags stripped entirely, no ANSI, no raw tags ---
send_line(sB, "color off")
recv_all_bytes(sB)
send_line(sA, "say <p>purple<z> and <r>red")
recv_all_bytes(sA)
rawB = recv_all_bytes(sB)
print(f"=== say with tags, color off (listener raw bytes) ===\n{rawB!r}")
check(b"\x1b[" not in rawB, "no ANSI escapes at all when color is off")
check(b"<p>" not in rawB and b"<z>" not in rawB and b"<r>" not in rawB,
      "raw tag text is stripped (not leaked) when color is off")
check(b"purple" in rawB and b"red" in rawB,
      "the surrounding plain text survives with color off")

sA.close()
sB.close()
print("=== ALL CHECKS PASSED ===")
