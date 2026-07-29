#!/usr/bin/env python3
"""Reproduction script for an intermittent connection hang found while
building tests/smoke_test_give_pour_transfer.py (Session 98, 2026-07-29).

Symptom: some connection's request (as simple as sending a character
name at the very first account-menu prompt) silently gets no response
at all -- not an error, not a slow response, just nothing, forever.
Reproduces both with two concurrent connections (one idle, one trying
to log in) AND with a single connection repeated in a tight back-to-back
loop with no concurrency at all -- so the root cause is NOT simply "two
sockets open at once." Confirmed NOT a blocking-socket issue:
main_socket_accept() (main_socket.c) correctly calls
socket_set_nonblocking() on every accepted client fd, not just the
listening socket.

Not yet root-caused further. Worth checking next:
  - descriptor_process_input()'s partial-line buffering (descriptor.c) --
    does a non-blocking recv() returning EAGAIN/EWOULDBLOCK ever get
    treated as "connection dead" or silently drop the partial buffer?
  - The game_loop.c select() loop's readfds/writefds FD_SET bookkeeping
    across a rapid sequence of connects/disconnects -- a stale fd number
    reused by the OS for a NEW connection colliding with bookkeeping for
    an old, just-destroyed descriptor_t could plausibly cause exactly
    this (the new connection's fd never actually gets watched).
  - hostname_resolve_start()/hostname_resolve_poll() (off-thread reverse
    DNS) -- some interaction between it and a freshly accepted
    descriptor_t.

Usage:
    python3 tests/tools/repro_connection_hang.py [host] [port]
Prints step-by-step progress; hangs (no further output) when it hits
the bug. Not reliably reproducible on the FIRST connection -- may take
several connect/disconnect cycles. Run with a wrapping `timeout` command
since this script has no internal timeout of its own on the hang itself.
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


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


for i in range(10):
    _suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 7 ** j) % 26) for j in range(6))
    name, pw = f"Rch{_suffix}", "reprohangpw123"
    print(f"[{i}] {name} connecting", flush=True)
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    print(f"[{i}] sending name", flush=True)
    out = cmd(s, name)
    print(f"[{i}] name response:", repr(out[:80]), flush=True)
    if "New account" in out:
        cmd(s, "n")
    s.close()
    print(f"[{i}] done", flush=True)
