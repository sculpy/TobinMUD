/*******************************************************************
 * TobinMUD Client ver. 0.4                                        *
 *******************************************************************/
/* Native Win32 GUI for the TobinMUD Client project (Phase 1c). One
 * window: a read-only RichEdit scrollback pane (colored per ANSI runs
 * via ansi_client.c) and a single-line input box, both set to a
 * monospace font (Consolas). Networking is a non-blocking Winsock2
 * socket polled on a timer, feeding raw bytes through telnet_client.c
 * (handles telnet/GMCP/MSDP negotiation) -> ansi_client.c (SGR-to-
 * colored-runs) -> RichEdit. MSP's `!!SOUND(...)` in-band marker is
 * scanned for and stripped before the ANSI pass, triggering
 * PlaySound() from a `sounds\` folder next to the exe -- MSP MUSIC
 * (`!!MUSIC(...)`/`!!MUSIC(Off)`) loops a random fight-music track for
 * as long as the server thinks you're fighting, distinct from MSP
 * SOUND's one-shot effects (see play_msp()'s own comment on the two
 * sharing one playback channel). GMCP Char.Vitals/Room.Info update
 * the window title as a simple, real status readout -- a dedicated
 * status bar/HP gauge is a natural follow-up once this pipe is proven
 * working, not done here (v1 scope, matches every other "prove the
 * pipe first" precedent in this session). On launch, checks
 * UPDATE_VERSION_URL for a newer release and silently re-installs
 * itself via msiexec if one exists -- see check_and_apply_update()'s
 * own comment.
 *
 * A `File` menu (user, 2026-08-05) offers Connect/Reconnect/
 * Disconnect/Preferences/Exit; a `Help` menu has About. The input box
 * refocuses itself whenever the window is (re)activated, and hitting
 * Enter on an empty input line resends the last real command instead
 * of doing nothing (same user request). Preferences lets the window
 * size and font size be adjusted and persists them to a small INI file
 * next to the exe (`prefs.ini`, via the standard Win32
 * Get/WritePrivateProfileString APIs) so they survive a restart. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <richedit.h>
#include <mmsystem.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "telnet_client.h"
#include "ansi_client.h"
#include "gmcp_json.h"
#include "resource.h"

#define DEFAULT_HOST "tobinmud.com"
#define DEFAULT_PORT 4000
#define ID_INPUT 101
#define ID_OUTPUT 102
#define ID_TIMER_POLL 1
#define WM_APP_SENDLINE (WM_APP + 1)

#define ID_MENU_FILE_CONNECT 201
#define ID_MENU_FILE_RECONNECT 202
#define ID_MENU_FILE_DISCONNECT 203
#define ID_MENU_FILE_PREFERENCES 204
#define ID_MENU_FILE_EXIT 205
#define ID_MENU_HELP_ABOUT 206

#define ID_PREFS_FONTSIZE_EDIT 301
#define ID_PREFS_WIDTH_EDIT 302
#define ID_PREFS_HEIGHT_EDIT 303
#define ID_PREFS_OK 304
#define ID_PREFS_CANCEL 305

/* Sensible defaults + clamps for the two preferences -- guards against
 * a hand-edited/corrupt prefs.ini producing an unusable (zero-size or
 * off-screen) window or an illegibly tiny/huge font. */
#define PREFS_DEFAULT_FONT_PT 10
#define PREFS_MIN_FONT_PT 6
#define PREFS_MAX_FONT_PT 36
#define PREFS_DEFAULT_WIN_W 800
#define PREFS_DEFAULT_WIN_H 600
#define PREFS_MIN_WIN_W 400
#define PREFS_MIN_WIN_H 300

/* Auto-update (user, 2026-08-05): bump this on every release that gets
 * published to the update host below. Compared as a plain string
 * against the published version.txt -- not a numeric/semver compare,
 * since both sides are entirely under this project's own control (no
 * third party ever publishes a version string here) and "different
 * from what I was built with" is all that's actually needed to decide
 * "go get the new one." */
#define CLIENT_VERSION "0.4.1"
#define UPDATE_VERSION_URL "http://tobinmud.com/tobinclient/version.txt"
#define UPDATE_MSI_URL "http://tobinmud.com/tobinclient/TobinMUDClient.msi"

/* Widens a narrow string-literal macro (e.g. CLIENT_VERSION) into a
 * wide one at the preprocessor level, so the titlebar's version text
 * (user, 2026-08-05: "titlebar should say TobinMUD Client version
 * number") can't drift out of sync with CLIENT_VERSION by hand-copying
 * it into a second literal -- one source of truth, same spirit as the
 * README's "keep these in sync" bump instructions for the .wxs file. */
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define CLIENT_TITLE_BASE (L"TobinMUD Client v" WIDEN(CLIENT_VERSION))

typedef struct {
    HWND hwnd_output;
    HWND hwnd_input;
    HWND hwnd_prefs; /* non-NULL while the Preferences window is open */
    SOCKET sock;
    telnet_client_t *telnet;
    ansi_client_t *ansi;
    WNDPROC input_orig_proc;
    HFONT font;
    /* Directory the .exe itself lives in, with a trailing backslash --
     * MSP sound files resolve from a "sounds\" subfolder next to it
     * (not the process's current working directory, which varies
     * depending on how the exe was launched and would otherwise make
     * sound playback silently fail depending on launch method), and
     * `prefs.ini` lives here too. */
    char exe_dir[MAX_PATH];
    /* Partial "!!SOUND(" scan state across on_text() calls -- a marker
     * can straddle two socket reads same as anything else on the wire. */
    char sound_scan_buf[256];
    size_t sound_scan_len;
    /* Repeat-last-command (user, 2026-08-05: "keep last command so i
     * could just hit enter to repeat command"): the last non-empty
     * line actually sent to the server. Hitting Enter on an EMPTY
     * input box resends this instead of a no-op/blank line; typing a
     * new line and sending it (even a repeat of the same text)
     * refreshes it as usual. */
    char last_line[1024];
    /* Live preferences -- font point size and window size, loaded from
     * (and saved back to) prefs.ini. Window size is only read at
     * startup (CreateWindowW needs it up front); font size is also
     * re-applied live if changed via the Preferences window. */
    int font_pt;
    int win_w;
    int win_h;
} app_state_t;

static app_state_t g_app;

static void append_output(const char *text, size_t len, int color_index, int bold) {
    if (len == 0)
        return;
    /* RichEdit works in UTF-16; the server sends UTF-8 (real box-
     * drawing characters in the connect banner/menu art -- confirmed
     * live 2026-08-05: CP_ACP (Windows-1252 on a US system) misread
     * those multi-byte sequences as individual Latin-1 bytes, producing
     * mojibake like "â•”â•â•" instead of the real "╔══"). Known,
     * disclosed limitation: a UTF-8 multi-byte sequence split exactly
     * across two socket reads could still misrender (each half arrives
     * in a separate telnet_client_feed()/ansi_client_feed() pass, no
     * cross-call UTF-8 reassembly buffer exists yet) -- rare in
     * practice for Tobin's short box-drawing runs, not fixed in this
     * pass. */
    wchar_t wbuf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, (int)len, wbuf,
                                    (int)(sizeof(wbuf) / sizeof(wbuf[0])) - 1);
    if (wlen <= 0)
        return;
    wbuf[wlen] = 0;

    static const COLORREF palette[8] = {
        RGB(0, 0, 0), RGB(170, 0, 0), RGB(0, 170, 0), RGB(170, 85, 0),
        RGB(0, 0, 170), RGB(170, 0, 170), RGB(0, 170, 170), RGB(170, 170, 170),
    };
    static const COLORREF palette_bold[8] = {
        RGB(85, 85, 85), RGB(255, 85, 85), RGB(85, 255, 85), RGB(255, 255, 85),
        RGB(85, 85, 255), RGB(255, 85, 255), RGB(85, 255, 255), RGB(255, 255, 255),
    };

    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    if (color_index >= 0 && color_index <= 7)
        cf.crTextColor = bold ? palette_bold[color_index] : palette[color_index];
    else
        cf.crTextColor = RGB(200, 200, 200); /* default -- matches a dark scrollback background */

    /* Move the caret to the end first (SetCharFormat with SCF_SELECTION
     * applies to the current selection, not "future typed text" unless
     * the selection is already an empty caret at the end). */
    int end = GetWindowTextLengthW(g_app.hwnd_output);
    SendMessageW(g_app.hwnd_output, EM_SETSEL, end, end);
    SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_app.hwnd_output, EM_REPLACESEL, FALSE, (LPARAM)wbuf);
    end = GetWindowTextLengthW(g_app.hwnd_output);
    SendMessageW(g_app.hwnd_output, EM_SETSEL, end, end);
    /* EM_SCROLLCARET (user, 2026-08-05: "doesnt autoscroll to the
     * bottom") only actually scrolls when the control has keyboard
     * focus -- it never does here, since focus stays on the input box
     * the whole time you're playing. WM_VSCROLL/SB_BOTTOM scrolls the
     * scrollbar directly instead, focus-independent, the standard
     * reliable way to keep a read-only log control pinned to the
     * bottom as text streams in. */
    SendMessageW(g_app.hwnd_output, WM_VSCROLL, SB_BOTTOM, 0);
}

static void ansi_emit_cb(void *ctx, const char *text, size_t len, int color_index, int bold) {
    (void)ctx;
    append_output(text, len, color_index, bold);
}

/* Plays (or, for `!!MUSIC(Off)`, stops) whatever `!!SOUND(...)`/
 * `!!MUSIC(...)` already extracted as its filename argument (or "Off"
 * for music). `loop` selects SND_LOOP for MSP MUSIC (real upstream
 * MSP semantics: MUSIC loops until told to stop; SOUND plays once).
 * Known, disclosed limitation: Win32's simple PlaySound() API is a
 * single shared playback channel for the whole process -- a `!!SOUND`
 * hit effect firing while `!!MUSIC` is looping will interrupt the
 * music rather than mixing over it (real audio mixing needs a heavier
 * API, e.g. DirectSound/XAudio2 -- out of scope for this pass). */
static void play_msp(const char *fname, bool loop) {
    if (strcmp(fname, "Off") == 0) {
        PlaySoundA(NULL, NULL, 0); /* stop whatever's currently playing */
        return;
    }
    char fullpath[MAX_PATH + 128 + 16];
    snprintf(fullpath, sizeof(fullpath), "%ssounds\\%s", g_app.exe_dir, fname);
    DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
    if (loop)
        flags |= SND_LOOP;
    PlaySoundA(fullpath, NULL, flags);
}

/* Scans (across calls) for `!!SOUND(<file> ...)` and `!!MUSIC(<file>
 * ...)`/`!!MUSIC(Off)` MSP markers in plain display text, acts on
 * them via play_msp(), and forwards everything else (marker text
 * stripped, since it's not meant to be visible) to the ANSI parser. A
 * partial marker at the end of this chunk is held in sound_scan_buf
 * for the next on_text() call, same "may straddle a socket read"
 * reasoning as the telnet/ANSI parsers. */
static void scan_msp_and_forward(const char *data, size_t len) {
    char combined[8192];
    size_t clen = 0;
    if (g_app.sound_scan_len > 0) {
        memcpy(combined, g_app.sound_scan_buf, g_app.sound_scan_len);
        clen = g_app.sound_scan_len;
        g_app.sound_scan_len = 0;
    }
    size_t copy = len;
    if (clen + copy > sizeof(combined))
        copy = sizeof(combined) - clen;
    memcpy(combined + clen, data, copy);
    clen += copy;

    size_t out_start = 0;
    size_t i = 0;
    while (i < clen) {
        int is_sound = (clen - i >= 8 && memcmp(combined + i, "!!SOUND(", 8) == 0);
        int is_music = (clen - i >= 8 && memcmp(combined + i, "!!MUSIC(", 8) == 0);
        if (combined[i] == '!' && i + 1 < clen && combined[i + 1] == '!' && (is_sound || is_music)) {
            /* Flush plain text before the marker. */
            if (i > out_start)
                ansi_client_feed(g_app.ansi, combined + out_start, i - out_start);
            const char *close = memchr(combined + i, ')', clen - i);
            if (!close) {
                /* Marker not fully arrived yet -- hold the rest for next time. */
                size_t rem = clen - i;
                if (rem <= sizeof(g_app.sound_scan_buf)) {
                    memcpy(g_app.sound_scan_buf, combined + i, rem);
                    g_app.sound_scan_len = rem;
                }
                return;
            }
            char fname[128];
            size_t inner_start = i + 8;
            size_t inner_len = (size_t)(close - (combined + inner_start));
            const char *sp = memchr(combined + inner_start, ' ', inner_len);
            size_t namelen = sp ? (size_t)(sp - (combined + inner_start)) : inner_len;
            if (namelen >= sizeof(fname))
                namelen = sizeof(fname) - 1;
            memcpy(fname, combined + inner_start, namelen);
            fname[namelen] = '\0';
            play_msp(fname, is_music);

            i = (size_t)(close - combined) + 1;
            out_start = i;
            continue;
        }
        i++;
    }
    if (clen > out_start)
        ansi_client_feed(g_app.ansi, combined + out_start, clen - out_start);
}

static void telnet_on_text(void *ctx, const char *data, size_t len) {
    (void)ctx;
    scan_msp_and_forward(data, len);
}

/* Base title (CLIENT_TITLE_BASE, always carries the version) plus
 * whatever live status GMCP has most recently reported -- restored
 * here (not just set once at startup) so the version string survives
 * every subsequent Char.Vitals/Room.Info title update instead of being
 * overwritten by status-only text. */
static void set_status_title(const wchar_t *status) {
    wchar_t title[400];
    if (status && status[0])
        swprintf(title, 400, L"%ls -- %ls", CLIENT_TITLE_BASE, status);
    else
        swprintf(title, 400, L"%ls", CLIENT_TITLE_BASE);
    SetWindowTextW(GetParent(g_app.hwnd_output), title);
}

static void telnet_on_gmcp(void *ctx, const char *package, const char *json) {
    (void)ctx;
    if (strcmp(package, "Char.Vitals") == 0) {
        int hp = 0, maxhp = 0, vit = 0, maxvit = 0;
        gmcp_json_get_int(json, "hp", &hp);
        gmcp_json_get_int(json, "maxhp", &maxhp);
        gmcp_json_get_int(json, "vit", &vit);
        gmcp_json_get_int(json, "maxvit", &maxvit);
        wchar_t status[128];
        swprintf(status, 128, L"HP %d/%d  Vit %d/%d", hp, maxhp, vit, maxvit);
        set_status_title(status);
    } else if (strcmp(package, "Room.Info") == 0) {
        char name[128];
        if (gmcp_json_get_string(json, "name", name, sizeof(name))) {
            wchar_t wname[128];
            MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 128);
            set_status_title(wname);
        }
    }
}

static void telnet_on_msdp_var(void *ctx, const char *name, const char *value) {
    (void)ctx; (void)name; (void)value;
    /* MSDP delivers the same vitals GMCP already handles for the title
     * bar; no second consumer yet (a client only needs one source of
     * truth) -- kept as a real, wired callback so MSDP-only clients
     * (no GMCP support) still have somewhere to plug in later. */
}

static void telnet_send_bytes(void *ctx, const unsigned char *data, size_t len) {
    (void)ctx;
    if (g_app.sock != INVALID_SOCKET)
        send(g_app.sock, (const char *)data, (int)len, 0);
}

static void do_connect(const char *host, int port) {
    struct addrinfo hints, *res = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        append_output("Could not resolve host.\r\n", 26, 1, 1);
        return;
    }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET || connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        append_output("Could not connect.\r\n", 20, 1, 1);
        if (s != INVALID_SOCKET)
            closesocket(s);
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    g_app.sock = s;
}

/* File menu "Disconnect"/"Reconnect" (user, 2026-08-05) -- closes the
 * current socket, if any, and tells the user so. Shared by both menu
 * commands (Reconnect is just Disconnect followed by a fresh
 * do_connect()) and by a real peer-initiated close (poll_socket()
 * below), so the "-- Disconnected --" message is never duplicated or
 * skipped depending on which path triggered it. */
static void do_disconnect(bool announce) {
    if (g_app.sock == INVALID_SOCKET)
        return;
    closesocket(g_app.sock);
    g_app.sock = INVALID_SOCKET;
    if (announce)
        append_output("\r\n-- Disconnected --\r\n", 22, 1, 1);
}

static void do_reconnect(void) {
    do_disconnect(false);
    do_connect(DEFAULT_HOST, DEFAULT_PORT);
}

static LRESULT CALLBACK InputSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        int len = GetWindowTextLengthA(hwnd);
        char buf[1024];
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        GetWindowTextA(hwnd, buf, sizeof(buf));
        SetWindowTextA(hwnd, "");
        /* Repeat-last-command (user, 2026-08-05): an empty input line
         * resends the last real command instead of sending a blank
         * line -- most MUD/telnet clients treat a bare Enter as "do it
         * again", and this is the same convention. A non-empty line
         * always sends as typed and becomes the new "last command",
         * even if it's identical to the previous one. */
        const char *to_send = buf;
        if (len == 0) {
            if (g_app.last_line[0] == '\0')
                return 0; /* nothing sent yet this session -- truly a no-op */
            to_send = g_app.last_line;
        } else {
            snprintf(g_app.last_line, sizeof(g_app.last_line), "%s", buf);
        }
        if (g_app.sock != INVALID_SOCKET) {
            char line[1030];
            int n = snprintf(line, sizeof(line), "%s\r\n", to_send);
            send(g_app.sock, line, n, 0);
        }
        return 0;
    }
    return CallWindowProc(g_app.input_orig_proc, hwnd, msg, wp, lp);
}

static void poll_socket(void) {
    if (g_app.sock == INVALID_SOCKET)
        return;
    unsigned char buf[4096];
    for (;;) {
        int n = recv(g_app.sock, (char *)buf, sizeof(buf), 0);
        if (n > 0) {
            telnet_client_feed(g_app.telnet, buf, (size_t)n);
            continue;
        }
        if (n == 0) { /* peer closed */
            do_disconnect(true);
            return;
        }
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return; /* nothing more right now */
        append_output("\r\n-- Connection error --\r\n", 26, 1, 1);
        do_disconnect(false);
        return;
    }
}

/* Fills g_app.exe_dir with the directory the running .exe lives in
 * (trailing backslash included), for resolving MSP sound files from a
 * `sounds\` subfolder next to it, and prefs.ini, regardless of the
 * process's current working directory (which varies by how the exe
 * was launched -- double-click, Start Menu shortcut, or a shell in
 * some other dir). */
static void resolve_exe_dir(void) {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        g_app.exe_dir[0] = '\0';
        return;
    }
    char *slash = strrchr(path, '\\');
    if (!slash) {
        g_app.exe_dir[0] = '\0';
        return;
    }
    size_t dirlen = (size_t)(slash - path) + 1; /* keep the trailing backslash */
    if (dirlen >= sizeof(g_app.exe_dir))
        dirlen = sizeof(g_app.exe_dir) - 1;
    memcpy(g_app.exe_dir, path, dirlen);
    g_app.exe_dir[dirlen] = '\0';
}

static void prefs_ini_path(char *out, size_t outsize) {
    snprintf(out, outsize, "%sprefs.ini", g_app.exe_dir);
}

/* Loads font_pt/win_w/win_h from prefs.ini next to the exe (via the
 * standard Win32 GetPrivateProfileInt -- no hand-rolled parser needed,
 * and it already returns the given default for a missing file/key, so
 * a first run or a partially-edited ini both just fall back cleanly).
 * Clamped to sane ranges in case of a hand-edited/corrupt value. */
static void load_prefs(void) {
    char path[MAX_PATH + 16];
    prefs_ini_path(path, sizeof(path));

    g_app.font_pt = GetPrivateProfileIntA("Prefs", "FontSize", PREFS_DEFAULT_FONT_PT, path);
    if (g_app.font_pt < PREFS_MIN_FONT_PT) g_app.font_pt = PREFS_MIN_FONT_PT;
    if (g_app.font_pt > PREFS_MAX_FONT_PT) g_app.font_pt = PREFS_MAX_FONT_PT;

    g_app.win_w = GetPrivateProfileIntA("Prefs", "WindowWidth", PREFS_DEFAULT_WIN_W, path);
    if (g_app.win_w < PREFS_MIN_WIN_W) g_app.win_w = PREFS_MIN_WIN_W;

    g_app.win_h = GetPrivateProfileIntA("Prefs", "WindowHeight", PREFS_DEFAULT_WIN_H, path);
    if (g_app.win_h < PREFS_MIN_WIN_H) g_app.win_h = PREFS_MIN_WIN_H;
}

static void save_prefs(void) {
    char path[MAX_PATH + 16];
    prefs_ini_path(path, sizeof(path));

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g_app.font_pt);
    WritePrivateProfileStringA("Prefs", "FontSize", buf, path);
    snprintf(buf, sizeof(buf), "%d", g_app.win_w);
    WritePrivateProfileStringA("Prefs", "WindowWidth", buf, path);
    snprintf(buf, sizeof(buf), "%d", g_app.win_h);
    WritePrivateProfileStringA("Prefs", "WindowHeight", buf, path);
}

/* (Re)builds g_app.font from g_app.font_pt and applies it to the input
 * box and the RichEdit's default AND already-typed-in text (so changing
 * font size from Preferences visibly resizes the whole scrollback, not
 * just newly-arriving lines) -- face/size only, deliberately leaving
 * SCF_SELECTION's color alone so existing colored lines keep their
 * color while just changing size. Called once at startup and again
 * every time Preferences applies a new size. */
static void apply_font(void) {
    if (g_app.font)
        DeleteObject(g_app.font);

    HDC hdc = GetDC(NULL);
    int px = -MulDiv(g_app.font_pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);

    g_app.font = CreateFontW(px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    if (g_app.hwnd_input)
        SendMessageW(g_app.hwnd_input, WM_SETFONT, (WPARAM)g_app.font, TRUE);

    if (g_app.hwnd_output) {
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE;
        wcscpy(cf.szFaceName, L"Consolas");
        cf.yHeight = g_app.font_pt * 20; /* twips (1/20 pt) */
        SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);

        LRESULT saved_start, saved_end;
        SendMessageW(g_app.hwnd_output, EM_GETSEL, (WPARAM)&saved_start, (LPARAM)&saved_end);
        SendMessageW(g_app.hwnd_output, EM_SETSEL, 0, -1);
        SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        SendMessageW(g_app.hwnd_output, EM_SETSEL, saved_start, saved_end);
    }
}

/* -- Preferences window (user, 2026-08-05: "preferences section to
 * adjust window size and font size") --
 *
 * A plain hand-built popup window (three labeled edit boxes + OK/
 * Cancel) rather than a resource-file dialog template, since this
 * project's CMake setup has no RC/windres step wired up yet (see
 * CMakeLists.txt) and adding one just for this would be a bigger
 * change than the feature itself warrants. Modal-ish: disables the
 * main window while open and re-enables it on close either way, same
 * user-facing effect as a real modal dialog. */
static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_PREFS_OK) {
            wchar_t buf[16];
            int font_pt = g_app.font_pt, win_w = g_app.win_w, win_h = g_app.win_h;

            GetDlgItemTextW(hwnd, ID_PREFS_FONTSIZE_EDIT, buf, 16);
            int v = _wtoi(buf);
            if (v >= PREFS_MIN_FONT_PT && v <= PREFS_MAX_FONT_PT)
                font_pt = v;

            GetDlgItemTextW(hwnd, ID_PREFS_WIDTH_EDIT, buf, 16);
            v = _wtoi(buf);
            if (v >= PREFS_MIN_WIN_W)
                win_w = v;

            GetDlgItemTextW(hwnd, ID_PREFS_HEIGHT_EDIT, buf, 16);
            v = _wtoi(buf);
            if (v >= PREFS_MIN_WIN_H)
                win_h = v;

            g_app.font_pt = font_pt;
            g_app.win_w = win_w;
            g_app.win_h = win_h;
            apply_font();
            SetWindowPos(GetParent(g_app.hwnd_output), NULL, 0, 0, win_w, win_h,
                         SWP_NOMOVE | SWP_NOZORDER);
            save_prefs();

            EnableWindow(GetParent(g_app.hwnd_output), TRUE);
            DestroyWindow(hwnd);
            SetFocus(g_app.hwnd_input);
            return 0;
        }
        if (id == ID_PREFS_CANCEL) {
            EnableWindow(GetParent(g_app.hwnd_output), TRUE);
            DestroyWindow(hwnd);
            SetFocus(g_app.hwnd_input);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        EnableWindow(GetParent(g_app.hwnd_output), TRUE);
        DestroyWindow(hwnd);
        SetFocus(g_app.hwnd_input);
        return 0;
    case WM_DESTROY:
        g_app.hwnd_prefs = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void open_preferences(HWND parent) {
    if (g_app.hwnd_prefs) {
        SetForegroundWindow(g_app.hwnd_prefs);
        return;
    }

    static bool cls_registered = false;
    if (!cls_registered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = PrefsWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"TobinMUDPrefsWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        cls_registered = true;
    }

    const int win_w = 300, win_h = 190;
    RECT pr;
    GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - win_w) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - win_h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TobinMUDPrefsWindow", L"Preferences",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, win_w, win_h, parent, NULL, GetModuleHandleW(NULL), NULL);
    g_app.hwnd_prefs = hwnd;

    HFONT dlgfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND lbl1 = CreateWindowW(L"STATIC", L"Font size (points):", WS_CHILD | WS_VISIBLE,
        16, 16, 150, 20, hwnd, NULL, NULL, NULL);
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", g_app.font_pt);
    HWND e1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180, 14, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_FONTSIZE_EDIT, NULL, NULL);

    HWND lbl2 = CreateWindowW(L"STATIC", L"Window width (px):", WS_CHILD | WS_VISIBLE,
        16, 48, 150, 20, hwnd, NULL, NULL, NULL);
    swprintf(buf, 16, L"%d", g_app.win_w);
    HWND e2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180, 46, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_WIDTH_EDIT, NULL, NULL);

    HWND lbl3 = CreateWindowW(L"STATIC", L"Window height (px):", WS_CHILD | WS_VISIBLE,
        16, 80, 150, 20, hwnd, NULL, NULL, NULL);
    swprintf(buf, 16, L"%d", g_app.win_h);
    HWND e3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180, 78, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_HEIGHT_EDIT, NULL, NULL);

    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        90, 130, 80, 26, hwnd, (HMENU)(INT_PTR)ID_PREFS_OK, NULL, NULL);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        180, 130, 80, 26, hwnd, (HMENU)(INT_PTR)ID_PREFS_CANCEL, NULL, NULL);

    HWND ctrls[] = { lbl1, e1, lbl2, e2, lbl3, e3, ok, cancel };
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++)
        SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)dlgfont, TRUE);

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(e1);
}

static HMENU build_menu(void) {
    HMENU hMenuBar = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_CONNECT, L"&Connect");
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_RECONNECT, L"&Reconnect");
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_DISCONNECT, L"&Disconnect");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_PREFERENCES, L"&Preferences...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_EXIT, L"E&xit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, ID_MENU_HELP_ABOUT, L"&About TobinMUD Client");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenuBar;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        /* Monospace font throughout (user, 2026-08-05: default felt
         * "scrunched" -- a proportional font on a MUD's column-aligned
         * output, plus the default Edit/RichEdit font size, is what
         * that was). Consolas ships with every Windows version this
         * targets; CreateFontW silently falls back to a default font
         * if it's somehow missing rather than failing outright, so no
         * extra fallback logic is needed here. Actual size comes from
         * g_app.font_pt (Preferences, default 10pt) via apply_font(). */
        g_app.hwnd_output = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_OUTPUT, NULL, NULL);
        SendMessageW(g_app.hwnd_output, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
        SendMessageW(g_app.hwnd_output, EM_SETEVENTMASK, 0, 0);
        {
            /* Sets the RichEdit's DEFAULT text color (not a selection)
             * so every future EM_REPLACESEL in append_output() inherits
             * it -- face/size are set right after via apply_font(),
             * which also (re)applies to already-typed text, so this
             * only needs to seed the color once here. */
            CHARFORMAT2W cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = RGB(200, 200, 200);
            SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
        }

        g_app.hwnd_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_INPUT, NULL, NULL);
        g_app.input_orig_proc = (WNDPROC)SetWindowLongPtr(g_app.hwnd_input, GWLP_WNDPROC,
                                                            (LONG_PTR)InputSubclassProc);
        apply_font();
        SetFocus(g_app.hwnd_input);

        g_app.sock = INVALID_SOCKET;
        telnet_client_callbacks_t tcb = { 0 };
        tcb.send_bytes = telnet_send_bytes;
        tcb.on_text = telnet_on_text;
        tcb.on_gmcp = telnet_on_gmcp;
        tcb.on_msdp_var = telnet_on_msdp_var;
        g_app.telnet = telnet_client_create(&tcb);

        ansi_client_callbacks_t acb = { 0 };
        acb.emit = ansi_emit_cb;
        g_app.ansi = ansi_client_create(&acb);

        do_connect(DEFAULT_HOST, DEFAULT_PORT);
        SetTimer(hwnd, ID_TIMER_POLL, 50, NULL);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        int input_h = 28; /* fits the Consolas font set in WM_CREATE without clipping descenders */
        MoveWindow(g_app.hwnd_output, 0, 0, w, h - input_h, TRUE);
        MoveWindow(g_app.hwnd_input, 0, h - input_h, w, input_h, TRUE);
        return 0;
    }
    /* Input focus (user, 2026-08-05: "client should focus on input") --
     * whenever this window becomes the active/foreground window (first
     * launch, alt-tab back, clicking the titlebar/scrollback, dismissing
     * Preferences), the input box gets keyboard focus so the player can
     * start typing immediately without an extra click. */
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE)
            SetFocus(g_app.hwnd_input);
        break;
    case WM_TIMER:
        if (wp == ID_TIMER_POLL)
            poll_socket();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case ID_MENU_FILE_CONNECT:
            if (g_app.sock == INVALID_SOCKET)
                do_connect(DEFAULT_HOST, DEFAULT_PORT);
            else
                append_output("Already connected.\r\n", 21, 3, 0);
            return 0;
        case ID_MENU_FILE_RECONNECT:
            do_reconnect();
            return 0;
        case ID_MENU_FILE_DISCONNECT:
            do_disconnect(true);
            return 0;
        case ID_MENU_FILE_PREFERENCES:
            open_preferences(hwnd);
            return 0;
        case ID_MENU_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_MENU_HELP_ABOUT: {
            wchar_t msg[128];
            swprintf(msg, 128, L"%ls\n\nA native client for TobinMUD.", CLIENT_TITLE_BASE);
            MessageBoxW(hwnd, msg, L"About", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_POLL);
        if (g_app.sock != INVALID_SOCKET)
            closesocket(g_app.sock);
        telnet_client_destroy(g_app.telnet);
        ansi_client_destroy(g_app.ansi);
        if (g_app.font)
            DeleteObject(g_app.font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* Auto-update (user, 2026-08-05). Fetches UPDATE_VERSION_URL (a plain
 * one-line text file, published alongside every release) and compares
 * it to CLIENT_VERSION; if different, downloads UPDATE_MSI_URL to a
 * temp file, runs `msiexec /i <temp>.msi /quiet` and WAITS for it (up
 * to 60s) -- reuses the MSI installer this project already has
 * (MajorUpgrade in the .wxs handles replacing the previous install)
 * rather than a separate updater program. Once the install genuinely
 * finishes, launches the freshly-installed exe directly from Program
 * Files so something visibly opens. Best-effort throughout: no
 * internet, an unreachable host, a failed download, or a failed
 * launch at any step just returns false and the CALLER shows the old
 * window instead -- an update must never leave the user with nothing
 * open (see the "client wont open" note below for why this matters:
 * the original fire-and-forget version updated correctly but gave
 * zero visible feedback, which from the outside is indistinguishable
 * from broken). Short (3s) connect/receive timeouts keep a dead host
 * from stalling startup.
 *
 * Returns true only once the NEW exe has actually been launched --
 * the caller should exit immediately without showing its own window
 * in that case (both so the new process owns the visible window, and
 * because msiexec needed this process to let go of its own locked
 * .exe file to replace it in the first place). */
/* TEMPORARY diagnostic (user, 2026-08-05: "not working again" -- exits
 * near-instantly with no crash log and no AV interference found).
 * Appends to %TEMP%\tobinmud_debug.log so a run's exact failure point
 * is visible after the fact, since the window (if any) vanishes too
 * fast to observe directly. Remove once root-caused. */
static void debug_log(const char *msg) {
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strncat(path, "tobinmud_debug.log", sizeof(path) - strlen(path) - 1);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

static bool check_and_apply_update(void) {
    HINTERNET hInternet = InternetOpenA("TobinMUDClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        debug_log("update: InternetOpenA FAILED");
        return false;
    }
    DWORD timeout_ms = 3000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    char remote_version[64] = { 0 };
    HINTERNET hVersion = InternetOpenUrlA(hInternet, UPDATE_VERSION_URL, NULL, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (hVersion) {
        DWORD read = 0;
        InternetReadFile(hVersion, remote_version, sizeof(remote_version) - 1, &read);
        remote_version[read] = '\0';
        InternetCloseHandle(hVersion);
    } else {
        char errbuf[128];
        snprintf(errbuf, sizeof(errbuf), "update: InternetOpenUrlA FAILED, GetLastError=%lu", (unsigned long)GetLastError());
        debug_log(errbuf);
    }
    for (char *p = remote_version; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }

    {
        char cmpbuf[160];
        snprintf(cmpbuf, sizeof(cmpbuf), "update: remote_version=\"%s\" CLIENT_VERSION=\"%s\"", remote_version, CLIENT_VERSION);
        debug_log(cmpbuf);
    }

    if (remote_version[0] == '\0' || strcmp(remote_version, CLIENT_VERSION) == 0) {
        debug_log("update: up to date (or check failed) -- skipping update");
        InternetCloseHandle(hInternet); /* up to date, or the check itself failed -- either way, nothing to do */
        return false;
    }
    debug_log("update: versions differ -- proceeding to download+install");

    char temp_dir[MAX_PATH], temp_msi[MAX_PATH + 32];
    GetTempPathA(MAX_PATH, temp_dir);
    snprintf(temp_msi, sizeof(temp_msi), "%sTobinMUDClient_update.msi", temp_dir);

    HINTERNET hMsi = InternetOpenUrlA(hInternet, UPDATE_MSI_URL, NULL, 0,
        INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!hMsi) {
        InternetCloseHandle(hInternet);
        return false;
    }
    HANDLE hFile = CreateFileA(temp_msi, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hMsi);
        InternetCloseHandle(hInternet);
        return false;
    }
    char buf[8192];
    DWORD n = 0;
    bool ok = true;
    while (InternetReadFile(hMsi, buf, sizeof(buf), &n) && n > 0) {
        DWORD written = 0;
        if (!WriteFile(hFile, buf, n, &written, NULL) || written != n) {
            ok = false;
            break;
        }
    }
    CloseHandle(hFile);
    InternetCloseHandle(hMsi);
    InternetCloseHandle(hInternet);
    if (!ok) {
        DeleteFileA(temp_msi);
        return false;
    }

    /* Run msiexec and WAIT for it (user, 2026-08-05: "client wont
     * open" -- the original ShellExecute-and-immediately-exit version
     * updated correctly but gave zero visible feedback: the old exe
     * just vanished with no window, no message, nothing, while
     * msiexec quietly finished in the background. From the outside
     * that's indistinguishable from "broken." Now: block until the
     * install genuinely finishes (up to 60s), then launch the freshly
     * installed exe directly, so something visibly opens either way. */
    char cmdline[sizeof(temp_msi) + 64];
    snprintf(cmdline, sizeof(cmdline), "msiexec.exe /i \"%s\" /quiet /norestart", temp_msi);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return false; /* couldn't even launch msiexec -- fall through, show the old window instead of nothing */

    WaitForSingleObject(pi.hProcess, 60000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteFileA(temp_msi);

    /* Per-user install path (tobinmud.wxs's own InstallScope="perUser",
     * see that file's header comment for why perMachine/Program Files
     * was the actual root cause of "client wont open" -- a non-
     * elevated msiexec can't service a perMachine install, so every
     * silent self-update was failing invisibly). */
    char local_appdata[MAX_PATH];
    if (!GetEnvironmentVariableA("LOCALAPPDATA", local_appdata, sizeof(local_appdata)))
        return false; /* installed, but can't find where -- fall through and show the old window */
    char new_exe[MAX_PATH + 64];
    snprintf(new_exe, sizeof(new_exe), "%s\\Programs\\TobinMUD Client\\TobinMUDClient.exe", local_appdata);

    STARTUPINFOA si2;
    PROCESS_INFORMATION pi2;
    ZeroMemory(&si2, sizeof(si2));
    si2.cb = sizeof(si2);
    ZeroMemory(&pi2, sizeof(pi2));
    if (!CreateProcessA(new_exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si2, &pi2))
        return false; /* the new install exists but wouldn't launch -- fall through rather than leave the user with nothing */
    CloseHandle(pi2.hThread);
    CloseHandle(pi2.hProcess);
    return true;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
    (void)hPrev; (void)cmdline;

    debug_log("WinMain: start, CLIENT_VERSION=" CLIENT_VERSION);

    resolve_exe_dir();
    load_prefs();

    bool updated = check_and_apply_update();
    debug_log(updated ? "WinMain: check_and_apply_update returned TRUE, exiting" :
                         "WinMain: check_and_apply_update returned FALSE, continuing normal startup");
    if (updated)
        return 0; /* update installer launched -- let it take over, don't show our window */

    WSADATA wsa;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);
    debug_log(wsa_result == 0 ? "WinMain: WSAStartup OK" : "WinMain: WSAStartup FAILED");
    LoadLibraryW(L"Msftedit.dll"); /* registers the MSFTEDIT_CLASS RichEdit window class */
    debug_log("WinMain: Msftedit.dll loaded");

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TobinMUDClientWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    /* Real embedded icon (tobinmud.rc) -- without this, Windows
     * synthesizes a generic letter-avatar icon for the titlebar/
     * taskbar/Alt-Tab (the "just shows a T" symptom, user 2026-08-05).
     * WNDCLASSW (unlike WNDCLASSEXW) has no separate small-icon field --
     * Windows derives the small titlebar icon from this same handle,
     * and the .ico already carries every size (16-256px) windres needs. */
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
    ATOM cls = RegisterClassW(&wc);
    debug_log(cls != 0 ? "WinMain: RegisterClassW OK" : "WinMain: RegisterClassW FAILED");

    HWND hwnd = CreateWindowW(L"TobinMUDClientWindow", CLIENT_TITLE_BASE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, g_app.win_w, g_app.win_h,
        NULL, NULL, hInst, NULL);
    debug_log(hwnd != NULL ? "WinMain: CreateWindowW OK" : "WinMain: CreateWindowW FAILED");
    SetMenu(hwnd, build_menu());
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    debug_log("WinMain: entering message loop");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    WSACleanup();
    return 0;
}
