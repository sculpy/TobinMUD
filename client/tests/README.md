# Client trigger/alias logic harnesses

Portable C proofs of the trigger-match, alias-expand, and prompt-flush
logic transcribed verbatim from src/win32/main.c (the Win32 GUI itself
cannot be click-tested from the droplet). Run on the droplet:

    gcc -Wall -o /tmp/t trigger_alias_fire_test.c && /tmp/t
    gcc -Wall -o /tmp/t trigger_prompt_test.c && /tmp/t

trigger_alias_fire_test.c -- substring match + whole-word alias expand.
trigger_prompt_test.c -- prompt-line firing (v0.4.30): no double-fire on
newline-completion, TCP-split safety, static-prompt-once.
