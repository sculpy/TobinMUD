/*******************************************************************
 * TobinMUD Client ver. 0.2                                        *
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
 * own comment. */
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

#define DEFAULT_HOST "tobinmud.com"
#define DEFAULT_PORT 4000
#define ID_INPUT 101
#define ID_OUTPUT 102
#define ID_TIMER_POLL 1
#define WM_APP_SENDLINE (WM_APP + 1)

/* Auto-update (user, 2026-08-05): bump this on every release that gets
 * published to the update host below. Compared as a plain string
 * against the published version.txt -- not a numeric/semver compare,
 * since both sides are entirely under this project's own control (no
 * third party ever publishes a version string here) and "different
 * from what I was built with" is all that's actually needed to decide
 * "go get the new one." */
#define CLIENT_VERSION "0.2.0"
#define UPDATE_VERSION_URL "http://tobinmud.com/tobinclient/version.txt"
#define UPDATE_MSI_URL "http://tobinmud.com/tobinclient/TobinMUDClient.msi"

typedef struct {
    HWND hwnd_output;
    HWND hwnd_input;
    SOCKET sock;
    telnet_client_t *telnet;
    ansi_client_t *ansi;
    WNDPROC input_orig_proc;
    HFONT font;
    /* Directory the .exe itself lives in, with a trailing backslash --
     * MSP sound files resolve from a "sounds\" subfolder next to the
     * exe (not the process's current working directory, which varies
     * depending on how the exe was launched and would otherwise make
     * sound playback silently fail depending on launch method). */
    char exe_dir[MAX_PATH];
    /* Partial "!!SOUND(" scan state across on_text() calls -- a marker
     * can straddle two socket reads same as anything else on the wire. */
    char sound_scan_buf[256];
    size_t sound_scan_len;
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

static void telnet_on_gmcp(void *ctx, const char *package, const char *json) {
    (void)ctx;
    if (strcmp(package, "Char.Vitals") == 0) {
        int hp = 0, maxhp = 0, vit = 0, maxvit = 0;
        gmcp_json_get_int(json, "hp", &hp);
        gmcp_json_get_int(json, "maxhp", &maxhp);
        gmcp_json_get_int(json, "vit", &vit);
        gmcp_json_get_int(json, "maxvit", &maxvit);
        wchar_t title[256];
        swprintf(title, 256, L"TobinMUD Client -- HP %d/%d  Vit %d/%d", hp, maxhp, vit, maxvit);
        SetWindowTextW(GetParent(g_app.hwnd_output), title);
    } else if (strcmp(package, "Room.Info") == 0) {
        char name[128];
        if (gmcp_json_get_string(json, "name", name, sizeof(name))) {
            wchar_t wname[128];
            MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 128);
            wchar_t title[300];
            swprintf(title, 300, L"TobinMUD Client -- %ls", wname);
            SetWindowTextW(GetParent(g_app.hwnd_output), title);
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

static LRESULT CALLBACK InputSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        int len = GetWindowTextLengthA(hwnd);
        char buf[1024];
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        GetWindowTextA(hwnd, buf, sizeof(buf));
        SetWindowTextA(hwnd, "");
        if (g_app.sock != INVALID_SOCKET) {
            char line[1030];
            int n = snprintf(line, sizeof(line), "%s\r\n", buf);
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
            closesocket(g_app.sock);
            g_app.sock = INVALID_SOCKET;
            append_output("\r\n-- Disconnected --\r\n", 22, 1, 1);
            return;
        }
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return; /* nothing more right now */
        closesocket(g_app.sock);
        g_app.sock = INVALID_SOCKET;
        append_output("\r\n-- Connection error --\r\n", 26, 1, 1);
        return;
    }
}

/* Fills g_app.exe_dir with the directory the running .exe lives in
 * (trailing backslash included), for resolving MSP sound files from a
 * `sounds\` subfolder next to it regardless of the process's current
 * working directory (which varies by how the exe was launched --
 * double-click, Start Menu shortcut, or a shell in some other dir). */
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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        resolve_exe_dir();

        /* Monospace font throughout (user, 2026-08-05: default felt
         * "scrunched" -- a proportional font on a MUD's column-aligned
         * output, plus the default Edit/RichEdit font size, is what
         * that was). Consolas ships with every Windows version this
         * targets; CreateFontW silently falls back to a default font
         * if it's somehow missing rather than failing outright, so no
         * extra fallback logic is needed here. */
        g_app.font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        g_app.hwnd_output = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_OUTPUT, NULL, NULL);
        SendMessageW(g_app.hwnd_output, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
        SendMessageW(g_app.hwnd_output, EM_SETEVENTMASK, 0, 0);
        {
            /* Sets the RichEdit's DEFAULT character formatting (not a
             * selection) so every future EM_REPLACESEL in append_output()
             * inherits this face/size, not just whatever the control
             * happened to start with. */
            CHARFORMAT2W cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
            wcscpy(cf.szFaceName, L"Consolas");
            cf.yHeight = 200; /* twips (1/20 pt) -- 10pt */
            cf.crTextColor = RGB(200, 200, 200);
            SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
        }

        g_app.hwnd_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_INPUT, NULL, NULL);
        SendMessageW(g_app.hwnd_input, WM_SETFONT, (WPARAM)g_app.font, TRUE);
        g_app.input_orig_proc = (WNDPROC)SetWindowLongPtr(g_app.hwnd_input, GWLP_WNDPROC,
                                                            (LONG_PTR)InputSubclassProc);
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
    case WM_TIMER:
        if (wp == ID_TIMER_POLL)
            poll_socket();
        return 0;
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
 * temp file and launches `msiexec /i <temp>.msi /quiet` -- reuses the
 * MSI installer this project already has (MajorUpgrade in the .wxs
 * handles replacing the previous install) rather than a separate
 * updater program. Best-effort throughout: no internet, an
 * unreachable host, or any download failure just returns false and
 * the app starts normally -- an update check must never block someone
 * from playing. Short (3s) connect/receive timeouts keep a dead host
 * from stalling startup.
 *
 * Returns true if an update install was actually launched -- the
 * caller should exit immediately without showing the main window,
 * since msiexec needs this process to let go of its own .exe file
 * (still open/locked while running) before it can replace it. */
static bool check_and_apply_update(void) {
    HINTERNET hInternet = InternetOpenA("TobinMUDClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
        return false;
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
    }
    for (char *p = remote_version; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }

    if (remote_version[0] == '\0' || strcmp(remote_version, CLIENT_VERSION) == 0) {
        InternetCloseHandle(hInternet); /* up to date, or the check itself failed -- either way, nothing to do */
        return false;
    }

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

    char cmdline[MAX_PATH + 64];
    snprintf(cmdline, sizeof(cmdline), "/i \"%s\" /quiet /norestart", temp_msi);
    HINSTANCE r = ShellExecuteA(NULL, "open", "msiexec.exe", cmdline, NULL, SW_HIDE);
    return (INT_PTR)r > 32;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
    (void)hPrev; (void)cmdline;

    if (check_and_apply_update())
        return 0; /* update installer launched -- let it take over, don't show our window */

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    LoadLibraryW(L"Msftedit.dll"); /* registers the MSFTEDIT_CLASS RichEdit window class */

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TobinMUDClientWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"TobinMUDClientWindow", L"TobinMUD Client",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    WSACleanup();
    return 0;
}
