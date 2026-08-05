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
  RichEdit scrollback pane (monospace/Consolas, real UTF-8 decoding --
  the server's connect-banner box-drawing art is UTF-8, not ANSI/CP_ACP),
  a single-line input box, a Winsock2 socket polled on a timer. GMCP
  `Char.Vitals`/`Room.Info` currently just update the window title (a
  real HP-bar widget is a natural follow-up once this pipe is proven,
  not yet built). MSP's `!!SOUND(file V=volume)` in-band marker plays
  `sounds\<file>` (next to the exe) via `PlaySound()`. Also checks for
  and silently self-updates to a newer release on launch -- see "Auto-
  update" below.

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

**Per-user install** (`InstallScope="perUser"` in tobinmud.wxs) --
installs to `%LocalAppData%\Programs\TobinMUD Client\`, NOT Program
Files, and needs no Administrator elevation. This was a deliberate fix
(2026-08-05, see "Auto-update" below for the "client wont open" story
behind it): a `perMachine` install requires elevation for msiexec to
service it at all, which silently broke the whole self-update flow
since it runs as a normal user with no UAC prompt. Adds a Start Menu
shortcut. Uninstall via the normal Windows "Apps & features" list, or
`msiexec /x TobinMUDClient.msi`.

## Testing

The GUI can't be click-tested from the Linux droplet -- build it here,
then run/verify on a real Windows machine: connects to
`tobinmud.com:4000` automatically on launch, colored text should
render, and the window title should update with HP/Vitality after
taking a hit in combat (proves the GMCP pipe end-to-end).

## Auto-update

On launch, the client fetches `http://tobinmud.com/tobinclient/version.txt`
and compares it (plain string compare) to `CLIENT_VERSION` in
`src/win32/main.c`. If different, it downloads
`http://tobinmud.com/tobinclient/TobinMUDClient.msi` to a temp file,
runs `msiexec /i <temp>.msi /quiet /norestart` and WAITS for it (up to
60s), then launches the freshly-installed exe directly so a real
window actually opens. Best-effort throughout -- no internet, host
down, a failed download, or a failed launch at any step just falls
through to showing the OLD window instead of nothing; an update check
must never leave the user looking at a blank screen.

**"client wont open" postmortem (2026-08-05)**: the very first version
of this feature fired-and-forgot `ShellExecuteA(msiexec, ...)` and
exited immediately, with zero feedback either way. Combined with the
installer originally being `perMachine`-scoped (Program Files, needs
Administrator elevation), every real self-update was failing silently
with Error 1730/exit 1603 -- msiexec running as a normal user simply
can't service a perMachine install -- and the client just vanished
with no window, no message, nothing. Two fixes, together: (1) the
installer is now `perUser`-scoped (see "Installing" above), so
msiexec needs no elevation at all; (2) the update code now waits for
msiexec and explicitly launches the result, so something always
visibly opens.

**Publishing a new release** (on the droplet):
```sh
# 1. Bump the version in two places (keep them in sync):
#    - CLIENT_VERSION in client/src/win32/main.c
#    - Version="X.Y.Z.0" in client/installer/windows/tobinmud.wxs

# 2. Rebuild
cd ~/NewMUD/client
rm -rf build-win64
cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win64

# 3. Rebuild the MSI
cd installer/windows
cp ../../build-win64/TobinMUDClient.exe .
wixl -v -a x64 -o TobinMUDClient.msi tobinmud.wxs

# 4. Publish
sudo cp TobinMUDClient.msi /usr/share/nginx/html/tobinclient/TobinMUDClient.msi
echo "X.Y.Z" | sudo tee /usr/share/nginx/html/tobinclient/version.txt
sudo chmod -R a+rX /usr/share/nginx/html/tobinclient
```
Every client still running an older version will pick up the new one
automatically the next time it's launched.

### Update host setup (already done on the current droplet, for reference)

- `nginx` installed (`sudo dnf install -y nginx`), serving its default
  webroot `/usr/share/nginx/html` on port 80 (`sudo systemctl enable
  --now nginx`).
- Files published to `/usr/share/nginx/html/tobinclient/`.
- **Port 80 needed opening on TWO separate firewall layers** -- see
  `../ENVIRONMENT.md`'s "Open the firewall" section for the full
  gotcha (this droplet runs `ufw` locally, not `firewall-cmd` despite
  being Fedora, plus the separate DigitalOcean cloud firewall).
