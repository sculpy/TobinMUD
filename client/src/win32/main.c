/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
/* Native Win32 GUI for the TobinMUD Client project (Phase 1c). One
 * window: a read-only RichEdit scrollback pane (colored per ANSI runs
 * via ansi_client.c) and a single-line input box. Networking is a
 * non-blocking Winsock2 socket polled on a timer, feeding raw bytes
 * through telnet_client.c (handles telnet/GMCP/MSDP negotiation) ->
 * ansi_client.c (SGR-to-colored-runs) -> RichEdit. MSP's `!!SOUND(...)`
 * in-band marker is scanned for and stripped before the ANSI pass,
 * triggering PlaySound(). GMCP Char.Vitals/Room.Info update the window
 * title as a simple, real status readout -- a dedicated status bar/HP
 * gauge is a natural follow-up once this pipe is proven working, not
 * done here (v1 scope, matches every other "prove the pipe first"
 * precedent in this session). */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <richedit.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

#include "telnet_client.h"
#include "ansi_client.h"
#include "gmcp_json.h"

#define DEFAULT_HOST "tobinmud.com"
#define DEFAULT_PORT 4000
#define ID_INPUT 101
#define ID_OUTPUT 102
#define ID_TIMER_POLL 1
#define WM_APP_SENDLINE (WM_APP + 1)

typedef struct {
    HWND hwnd_output;
    HWND hwnd_input;
    SOCKET sock;
    telnet_client_t *telnet;
    ansi_client_t *ansi;
    WNDPROC input_orig_proc;
    /* Partial "!!SOUND(" scan state across on_text() calls -- a marker
     * can straddle two socket reads same as anything else on the wire. */
    char sound_scan_buf[256];
    size_t sound_scan_len;
} app_state_t;

static app_state_t g_app;

static void append_output(const char *text, size_t len, int color_index, int bold) {
    if (len == 0)
        return;
    /* RichEdit works in UTF-16; the server sends plain ASCII/latin-ish
     * bytes (Tobin's own DB text, colorstring.c output) -- a direct
     * byte-to-wchar widen is correct for that range and avoids pulling
     * in a real codepage conversion for content that's already ASCII
     * in practice. */
    wchar_t wbuf[4096];
    int wlen = MultiByteToWideChar(CP_ACP, 0, text, (int)len, wbuf,
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
    SendMessageW(g_app.hwnd_output, EM_SCROLLCARET, 0, 0);
}

static void ansi_emit_cb(void *ctx, const char *text, size_t len, int color_index, int bold) {
    (void)ctx;
    append_output(text, len, color_index, bold);
}

/* Scans (across calls) for `!!SOUND(<file> ...)` MSP markers in plain
 * display text, plays the sound, and forwards everything else (marker
 * text stripped, since it's not meant to be visible) to the ANSI
 * parser. A partial marker at the end of this chunk is held in
 * sound_scan_buf for the next on_text() call, same "may straddle a
 * socket read" reasoning as the telnet/ANSI parsers. */
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
        if (combined[i] == '!' && i + 1 < clen && combined[i + 1] == '!'
            && clen - i >= 8 && memcmp(combined + i, "!!SOUND(", 8) == 0) {
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
            PlaySoundA(fname, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);

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
            MultiByteToWideChar(CP_ACP, 0, name, -1, wname, 128);
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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_app.hwnd_output = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_OUTPUT, NULL, NULL);
        SendMessageW(g_app.hwnd_output, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
        SendMessageW(g_app.hwnd_output, EM_SETEVENTMASK, 0, 0);

        g_app.hwnd_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_INPUT, NULL, NULL);
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
        int input_h = 24;
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
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
    (void)hPrev; (void)cmdline;

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
