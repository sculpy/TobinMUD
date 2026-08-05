# TobinMUD Client

A native Win32 client for [Tobin](../c_port/) with GMCP/MSDP/MSP
support (structured game state + sound cues), built alongside the
server's own protocol layer (`c_port/src/net/gmcp.c`/`msdp.c`,
`descriptor.c`'s telnet negotiation). See `../CLAUDE.md`'s TODO.md
backlog for the project this belongs to.

Phase 1: Windows only. macOS was in scope originally but dropped by
user request (2026-08-05) -- see STATUS.md.

## Architecture

- `include/`, `src/core/` -- portable protocol layer, no OS calls:
  `telnet_client.c` (telnet/GMCP/MSDP negotiation, mirrors the
  server's own parser in `descriptor.c`), `ansi_client.c` (ANSI SGR
  escapes -> colored text runs), `gmcp_json.c` (minimal field
  extraction for the server's small, flat GMCP payloads -- not a
  general JSON parser).
- `src/win32/main.c` -- the actual GUI: one window, a read-only
  RichEdit scrollback pane, a single-line input box, a Winsock2 socket
  polled on a timer. GMCP `Char.Vitals`/`Room.Info` currently just
  update the window title (a real HP-bar widget is a natural follow-up
  once this pipe is proven, not yet built). MSP's `!!SOUND(file
  V=volume)` in-band marker triggers `PlaySound()`.

## Building (on the droplet, per CLAUDE.md's droplet-only rule -- never locally)

One-time setup (already done on the current droplet):
```sh
sudo dnf install -y mingw64-gcc msitools
```

Cross-compile:
```sh
cd ~/NewMUD/client
cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win64
```
Produces `build-win64/TobinMUDClient.exe` -- zero-warning build
required, same as the server.

## Building the MSI installer

```sh
cd ~/NewMUD/client/installer/windows
cp ../../build-win64/TobinMUDClient.exe .
wixl -v -a x64 -o TobinMUDClient.msi tobinmud.wxs
```

## Installing

Normal (interactive) install:
```
msiexec /i TobinMUDClient.msi
```
or just double-click the .msi.

**Silent install** (the user's original ask): standard MSI behavior,
nothing custom built for it --
```
msiexec /i TobinMUDClient.msi /quiet
```
`/quiet` shows no UI but a brief progress indicator; `/qn` suppresses
even that for a fully silent, unattended install. Both work because
this is a real MSI package, not anything TobinMUD-specific.

Installs to `Program Files\TobinMUD Client\` and adds a Start Menu
shortcut. Uninstall via the normal Windows "Apps & features" list, or
`msiexec /x TobinMUDClient.msi`.

## Testing

The GUI can't be click-tested from the Linux droplet -- build it here,
then run/verify on a real Windows machine: connects to
`tobinmud.com:4000` automatically on launch, colored text should
render, and the window title should update with HP/Vitality after
taking a hit in combat (proves the GMCP pipe end-to-end).
