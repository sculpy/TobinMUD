"""Shared character-creation helper for the smoke test suite.

Every test used to hand-roll its own copy of the account-menu "new"
character walk (race pick -> homeland pick -> class pick -> attrs
"done" -> options "done"). When that sequence gained a step (the
homeland/territory pick, added 2026-08-03) or reordered (race-before-
attrs, added 2026-07-12), every single copy broke independently and had
to be found and fixed one file at a time -- twice now. This module is
the one place that sequence lives; when it changes again, only this
file needs editing.

Scope is deliberately narrow: this covers ONLY the part of creation
that is identical everywhere (from the account menu's 'new' prompt
through to a playing character). It does NOT cover logging into the
account itself (name/password/color/timezone prompts) -- that preamble
varies too much test-to-test (new account vs existing, color/timezone
prompts appearing only on some paths) to safely centralize the same
way.

Usage:
    from mud_creation import finish_char_creation
    send_line(sock, "new"); recv_all(sock)
    out = finish_char_creation(sock, char_name, send_line, recv_all)
"""


def finish_char_creation(sock, char_name, send_line, recv_all,
                          race="1", territory="1", char_class="1"):
    """Drive character creation from the 'new character name' prompt
    (i.e. right after 'new' was sent and its response consumed) through
    to a fully created, playing character. Sends: char_name, a race
    pick, a homeland pick, a class pick, then 'done' twice (attrs
    screen, then options screen). Returns the final output -- the
    auto-look text shown on entering the world.

    send_line/recv_all are passed in (not imported) so this works with
    any test's own socket-I/O helpers, including the recv_all_bytes
    variant some tests use instead of the str-decoding one.
    """
    send_line(sock, char_name)
    recv_all(sock)
    send_line(sock, race)
    recv_all(sock)
    send_line(sock, territory)
    recv_all(sock)
    send_line(sock, char_class)
    recv_all(sock)
    send_line(sock, "done")
    recv_all(sock)
    send_line(sock, "done")
    return recv_all(sock)


def create_character(sock, char_name, send_line, recv_all,
                      race="1", territory="1", char_class="1"):
    """Like finish_char_creation(), but also sends the 'new' step
    itself -- for the common case of a test that's just landed on the
    account menu and wants a full character in one call."""
    send_line(sock, "new")
    recv_all(sock)
    return finish_char_creation(sock, char_name, send_line, recv_all,
                                 race=race, territory=territory, char_class=char_class)
