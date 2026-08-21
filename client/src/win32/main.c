/*******************************************************************
 * TobinMUD Client ver. 0.4                                        *
 *******************************************************************/
/* Native Win32 GUI for the TobinMUD Client project (Phase 1c). One
 * window: a read-only RichEdit scrollback pane (colored per ANSI runs
 * via ansi_client.c) and a single-line input box, both set to a
 * monospace font (DejaVu Sans Mono). Networking is a non-blocking Winsock2
 * socket polled on a timer, feeding raw bytes through telnet_client.c
 * (handles telnet/GMCP/MSDP negotiation) -> ansi_client.c (SGR-to-
 * colored-runs) -> RichEdit. MSP's `!!SOUND(...)` in-band marker is
 * scanned for and stripped before the ANSI pass, triggering
 * PlaySound() from a `sounds\` folder next to the exe -- MSP MUSIC
 * (`!!MUSIC(...)`/`!!MUSIC(Off)`) loops a random fight-music track for
 * as long as the server thinks you're fighting, distinct from MSP
 * SOUND's one-shot effects -- MUSIC plays via a dedicated MCI channel
 * so hit sounds can overlap it instead of cutting it off (see
 * play_msp()'s own comment). GMCP Char.Vitals feeds a real HP/Mana/
 * Move gauge strip (three native progress-bar controls between the
 * menu bar and the scrollback, see set_gauge()); Room.Info still
 * updates the window title. On launch, checks
 * UPDATE_VERSION_URL for a newer release and silently re-installs
 * itself via msiexec if one exists -- see check_and_apply_update()'s
 * own comment. A small always-on-top splash ("Updating TobinMUD
 * Client...") covers the up-to-60s download+install wait so a real
 * update is never mistaken for the client failing to launch -- see
 * create_update_splash().
 *
 * A `File` menu (user, 2026-08-05) offers Connect/Reconnect/
 * Disconnect/Preferences/Edit Triggers/Reload Triggers/Edit Aliases/
 * Reload Aliases/Exit; an `Edit` menu (2026-08-21) offers Cut/Copy/
 * Paste/Select All acting on whichever pane has focus, and the
 * read-only scrollback also gets its own right-click Copy/Select All
 * menu (RichEdit has no built-in one the way a plain Edit control
 * does -- see OutputSubclassProc's own comment); a `Help` menu has Check for Updates (manual re-check of the update host, same install path as the silent startup check) and About. Triggers are plain,
 * case-insensitive substring-match rules against incoming lines, each
 * optionally sending a command and/or gagging the line (user,
 * 2026-08-06: "make triggers simple, not lua based" -- see
 * load_triggers()'s own comment for the on-disk triggers.txt format);
 * aliases expand a typed first word into a longer command (see
 * load_aliases()'s own comment for aliases.txt's format). Both are
 * hand-editable text files next to the exe AND now editable in-app
 * (client backlog, logged 2026-08-06) via Edit Triggers.../Edit
 * Aliases... -- a SysListView32 grid of existing entries plus Add/
 * Update Selected/Delete Selected/Save controls, see
 * open_trigger_editor()/open_alias_editor()'s own comments; Save
 * writes straight back to the same tab-delimited file and reloads it
 * live (no separate Reload needed after using the editor -- Reload
 * Triggers/Reload Aliases stay in the menu for the "hand-edit the file
 * in Notepad, then reload" workflow, which never opens a window at
 * all). The input box refocuses itself whenever the window is
 * (re)activated, and hitting Enter on an empty input line resends the
 * last real command instead of doing nothing (same user request).
 * Preferences lets the window size, font size, and font family (via the standard Choose Font dialog) be adjusted and
 * persists them to a small INI file next to the exe (`prefs.ini`, via
 * the standard Win32 Get/WritePrivateProfileString APIs) so they
 * survive a restart. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <wininet.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>

#include "telnet_client.h"
#include "ansi_client.h"
#include "gmcp_json.h"
#include "resource.h"

#define DEFAULT_HOST "tobinmud.com"
#define DEFAULT_PORT 4000
#define ID_INPUT 101
#define ID_OUTPUT 102
#define ID_GAUGE_LABEL_HP 401
#define ID_GAUGE_BAR_HP 402
#define ID_GAUGE_LABEL_MANA 403
#define ID_GAUGE_BAR_MANA 404
#define ID_GAUGE_LABEL_MOVE 405
#define ID_GAUGE_BAR_MOVE 406
#define ID_TIMER_POLL 1
#define WM_APP_SENDLINE (WM_APP + 1)

#define ID_MENU_FILE_CONNECT 201
#define ID_MENU_FILE_RECONNECT 202
#define ID_MENU_FILE_DISCONNECT 203
#define ID_MENU_FILE_PREFERENCES 204
#define ID_MENU_FILE_EXIT 205
#define ID_MENU_FILE_RELOAD_TRIGGERS 207
#define ID_MENU_FILE_RELOAD_ALIASES 208
#define ID_MENU_HELP_ABOUT 206
#define ID_MENU_HELP_CHECK_UPDATE 211
#define ID_MENU_FILE_EDIT_TRIGGERS 209
#define ID_MENU_FILE_EDIT_ALIASES 210
#define ID_MENU_EDIT_CUT 212
#define ID_MENU_EDIT_COPY 213
#define ID_MENU_EDIT_PASTE 214
#define ID_MENU_EDIT_SELECTALL 215

#define ID_PREFS_FONTSIZE_EDIT 301
#define ID_PREFS_WIDTH_EDIT 302
#define ID_PREFS_HEIGHT_EDIT 303
#define ID_PREFS_OK 304
#define ID_PREFS_CANCEL 305
#define ID_PREFS_FULLSCREEN_CHECK 306
#define ID_PREFS_FONT_FACE_STATIC 307
#define ID_PREFS_FONT_CHOOSE 308

/* Trigger/alias GUI editors (client backlog, logged 2026-08-06 --
 * "an in-client GUI editor for triggers and aliases"). Both editors
 * share the same shape: a report-view SysListView32 of existing
 * entries, a couple of EDIT fields + buttons to Add/Update/Delete a
 * row, then Save (writes back to triggers.txt/aliases.txt and
 * reloads the live in-memory tables) or Cancel. See
 * open_trigger_editor()/open_alias_editor()'s own comments. */
#define ID_TRIGEDIT_LIST 601
#define ID_TRIGEDIT_PATTERN_EDIT 602
#define ID_TRIGEDIT_ACTION_EDIT 603
#define ID_TRIGEDIT_GAG_CHECK 604
#define ID_TRIGEDIT_ADD 605
#define ID_TRIGEDIT_UPDATE 606
#define ID_TRIGEDIT_DELETE 607
#define ID_TRIGEDIT_SAVE 608
#define ID_TRIGEDIT_CANCEL 609

#define ID_ALIASEDIT_LIST 701
#define ID_ALIASEDIT_NAME_EDIT 702
#define ID_ALIASEDIT_EXPANSION_EDIT 703
#define ID_ALIASEDIT_ADD 704
#define ID_ALIASEDIT_UPDATE 705
#define ID_ALIASEDIT_DELETE 706
#define ID_ALIASEDIT_SAVE 707
#define ID_ALIASEDIT_CANCEL 708

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
#define CLIENT_VERSION "0.4.32"
#define HISTORY_MAX 100
#define GAUGE_H 34 /* height in px of the HP/Mana/Move gauge strip */

/* Simple triggers (user, 2026-08-06: "make triggers simple, not lua
 * based") -- plain case-insensitive substring match against each
 * complete incoming line, no wildcards/regex/scripting. See
 * load_triggers()'s own comment for the on-disk format. */
#define TRIGGER_MAX 128
#define TRIGGER_PATTERN_MAX 128
#define TRIGGER_ACTION_MAX 256
#define TRIGGER_LINE_BUF 2048

typedef struct {
    char pattern[TRIGGER_PATTERN_MAX];
    char action[TRIGGER_ACTION_MAX];
    bool gag;
} trigger_t;

/* Simple aliases (same "not lua based" spirit as triggers) -- a short
 * typed word expands to a longer command before sending, with
 * whatever else was typed after it carried through unchanged. See
 * load_aliases()'s own comment for the on-disk format. */
#define ALIAS_MAX 128
#define ALIAS_NAME_MAX 32
#define ALIAS_EXPANSION_MAX 256

typedef struct {
    char name[ALIAS_NAME_MAX];
    char expansion[ALIAS_EXPANSION_MAX];
} alias_t;
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
    HWND hwnd_trigedit; /* non-NULL while the trigger editor is open */
    HWND hwnd_aliasedit; /* non-NULL while the alias editor is open */
    SOCKET sock;
    telnet_client_t *telnet;
    ansi_client_t *ansi;
    WNDPROC input_orig_proc;
    WNDPROC output_orig_proc;
    HFONT font;
    /* Directory the .exe itself lives in, with a trailing backslash --
     * MSP sound files resolve from a "sounds\" subfolder next to it
     * (not the process's current working directory, which varies
     * depending on how the exe was launched and would otherwise make
     * sound playback silently fail depending on launch method), and
     * `prefs.ini` lives here too. */
    char exe_dir[MAX_PATH];
    /* System light/dark theme (user, 2026-08-06: "color preferences
     * dictated by the system config") -- read once at startup from the
     * same registry value Windows Settings > Colors > "Choose your
     * mode" writes, AppsUseLightTheme. Output pane bg/default-text and
     * the input box are recolored to match; the 16-color ANSI palette
     * itself stays the same in both modes (standard terminal-emulator
     * convention -- only the neutral bg/fg swap, not the color meanings
     * a MUD script or player might rely on). */
    bool dark_theme;
    HBRUSH input_bg_brush; /* only non-NULL in dark mode; NULL lets
                               DefWindowProc's default (white) stand in
                               light mode */
    /* HP/Mana/Move gauge bar (user, 2026-08-06: "make the client behave
     * as much as possible to Mudlet" -- a real gauge bar was flagged as
     * the natural follow-up to the GMCP Char.Vitals pipe already
     * proven via the title-bar text). Three native progress-bar
     * controls with a label static above each, in a fixed-height strip
     * between the menu bar and the scrollback -- fed by
     * telnet_on_gmcp()'s existing Char.Vitals handler, which now also
     * carries mana (see gmcp.c's server-side change, same date). */
    HWND hwnd_gauge_label_hp, hwnd_gauge_label_mana, hwnd_gauge_label_move;
    HWND hwnd_gauge_bar_hp, hwnd_gauge_bar_mana, hwnd_gauge_bar_move;
    /* Simple triggers -- loaded from triggers.txt (exe_dir) at startup
     * and via File > Reload Triggers. pending_line_text/len accumulate
     * the PLAIN (color-stripped) text of whatever line is currently
     * being displayed, purely for matching -- the actual display
     * always happens immediately via append_output() for full
     * responsiveness (see trigger_scan_feed()'s own comment); a gag
     * match retroactively deletes the just-displayed line's RichEdit
     * range instead of holding output back. */
    trigger_t triggers[TRIGGER_MAX];
    int trigger_count;
    alias_t aliases[ALIAS_MAX];
    int alias_count;
    /* Leading #-comment/blank lines load_triggers()/load_aliases() found
     * before the first real entry, re-emitted verbatim by
     * save_triggers()/save_aliases() so a file's header commentary (the
     * seeded example text, or anything a player hand-writes there)
     * survives a round trip through the GUI editor. Comments that
     * appear AFTER the first real entry are NOT preserved -- a
     * deliberate scope-down, since round-tripping interspersed comments
     * through a row-oriented list editor would need to tie each comment
     * to a specific row and re-detect where it belonged on save, far
     * more machinery than this "simple, not lua based" feature
     * warrants. */
    char trigger_comment_header[2048];
    char alias_comment_header[2048];
    char pending_line_text[TRIGGER_LINE_BUF];
    size_t pending_line_len;
    int line_start_offset;
    /* Which triggers have already fired their ACTION for the line
     * currently accumulating in pending_line_text -- so a prompt-flush
     * (trigger_flush_prompt(), fired when the socket drains mid-line at a
     * no-newline prompt) and the later newline-completion of that same
     * line don't send the action twice. Indexed by trigger index; reset
     * to all-zero each time a line completes (trigger_process_line). */
    unsigned char trigger_fired_this_line[TRIGGER_MAX];
    HBRUSH window_bg_brush; /* dark-mode only, backs the gauge strip's own
                                static labels via WM_CTLCOLORSTATIC and the
                                main window class background */
    /* Partial "!!SOUND(" scan state across on_text() calls -- a marker
     * can straddle two socket reads same as anything else on the wire. */
    char sound_scan_buf[256];
    size_t sound_scan_len;
    bool cr_pending; /* last byte of the previous chunk was a bare \r --
                       * see scan_msp_and_forward()'s CRLF-collapse pass */
    /* Repeat-last-command (user, 2026-08-05: "keep last command so i
     * could just hit enter to repeat command"): the last non-empty
     * line actually sent to the server. Hitting Enter on an EMPTY
     * input box resends this instead of a no-op/blank line; typing a
     * new line and sending it (even a repeat of the same text)
     * refreshes it as usual. */
    char last_line[1024 + ALIAS_EXPANSION_MAX];
    /* Command history (user, 2026-08-06: "make the client behave as
     * much as possible to Mudlet" -- Up/Down arrow recall, picked as
     * the highest-value single Mudlet-ism to add first). history[] is
     * append-only up to HISTORY_MAX, oldest dropped via shift once
     * full; history_pos is the index currently shown in the input box,
     * or == history_count when NOT browsing (the normal state) --
     * history_pending holds whatever the player had actually typed
     * before the first Up press, restored on Down past the newest
     * entry, same convention as bash. */
    char history[HISTORY_MAX][1024];
    int history_count;
    int history_pos;
    char history_pending[1024];
    /* Live preferences -- font point size and window size, loaded from
     * (and saved back to) prefs.ini. Window size is only read at
     * startup (CreateWindowW needs it up front); font size is also
     * re-applied live if changed via the Preferences window. */
    char font_face[64]; /* display font family (Preferences "Choose..."),
                           Courier New default -- see apply_font() */
    int font_pt;
    int win_w;
    int win_h;
    /* Full screen (user, 2026-08-06: "a full screen option should be in
     * preferences") -- borderless, covering the monitor the window is
     * currently on. windowed_rect/windowed_style save what to restore
     * to when it's turned back off. */
    bool fullscreen;
    RECT windowed_rect;
    LONG_PTR windowed_style;
} app_state_t;

static app_state_t g_app;
static void debug_log(const char *msg); /* defined near check_and_apply_update(); forward-declared for play_msp()'s MCI-failure fallback log */
static void check_for_updates_interactive(HWND owner); /* Help > Check for Updates...; defined with the other update helpers below */

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
        cf.crTextColor = g_app.dark_theme ? RGB(200, 200, 200) : RGB(20, 20, 20);

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

static void triggers_path(char *out, size_t outsize) {
    snprintf(out, outsize, "%striggers.txt", g_app.exe_dir);
}

/* Plain, case-insensitive substring search (no strcasestr() dependency --
 * keeps this portable/self-contained, and the whole point of "simple,
 * not lua" triggers is a tiny, obviously-correct matcher). */
static bool ci_contains(const char *hay, const char *needle) {
    if (!needle[0])
        return false;
    size_t hlen = strlen(hay), nlen = strlen(needle);
    if (nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b)
                break;
        }
        if (j == nlen)
            return true;
    }
    return false;
}

/* Loads triggers.txt (next to the exe, same folder as sounds\ and
 * prefs.ini): one trigger per line, tab-separated --
 * PATTERN<TAB>ACTION[<TAB>flags]. PATTERN is a plain, case-insensitive
 * substring match against each complete incoming line (no wildcards/
 * regex/scripting -- user, 2026-08-06: "make triggers simple, not lua
 * based"). ACTION is sent to the server exactly like a typed command;
 * leave it blank to just gag (hide) the line. A trailing "g" (or "G")
 * in the optional third field gags the matched line in addition to
 * sending ACTION. Lines starting with # are comments. First run with
 * no file present seeds a commented example instead of silently doing
 * nothing, so the feature is discoverable without external docs. */
static void load_triggers(void) {
    g_app.trigger_count = 0;
    g_app.trigger_comment_header[0] = '\0';
    char path[MAX_PATH + 32];
    triggers_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            fputs("# TobinMUD Client triggers -- one per line:\r\n"
                  "#   PATTERN<TAB>ACTION[<TAB>g]\r\n"
                  "# PATTERN is a plain, case-insensitive substring match against each\r\n"
                  "# incoming line (no wildcards/regex -- kept simple on purpose).\r\n"
                  "# ACTION is sent to the server exactly like a typed command; leave it\r\n"
                  "# blank to just gag (hide) the line without sending anything.\r\n"
                  "# Add a trailing g in a third tab-separated field to gag the matched\r\n"
                  "# line as well as sending ACTION.\r\n"
                  "# Reload after editing via File > Reload Triggers.\r\n"
                  "#\r\n"
                  "# Example (disabled -- remove the leading # to enable):\r\n"
                  "# You are bleeding\tbandage self\r\n", f);
            fclose(f);
        }
        return;
    }
    char line[512];
    bool seen_entry = false;
    size_t hdr_len = 0;
    while (g_app.trigger_count < TRIGGER_MAX && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0 || line[0] == '#') {
            /* Leading comment/blank line -- stash it verbatim (see
             * trigger_comment_header's own comment) as long as no real
             * trigger has been seen yet. */
            if (!seen_entry && hdr_len < sizeof(g_app.trigger_comment_header)) {
                int added = snprintf(g_app.trigger_comment_header + hdr_len,
                                      sizeof(g_app.trigger_comment_header) - hdr_len,
                                      "%s\r\n", line);
                if (added > 0) {
                    hdr_len += (size_t)added;
                    if (hdr_len >= sizeof(g_app.trigger_comment_header))
                        hdr_len = sizeof(g_app.trigger_comment_header) - 1;
                }
            }
            continue;
        }
        seen_entry = true;
        char *tab1 = strchr(line, '\t');
        if (!tab1)
            continue;
        *tab1 = '\0';
        const char *pattern = line;
        if (!pattern[0])
            continue;
        char *rest = tab1 + 1;
        char *tab2 = strchr(rest, '\t');
        bool gag = false;
        const char *action = rest;
        if (tab2) {
            *tab2 = '\0';
            const char *flags = tab2 + 1;
            if (strchr(flags, 'g') || strchr(flags, 'G'))
                gag = true;
        }
        trigger_t *t = &g_app.triggers[g_app.trigger_count++];
        snprintf(t->pattern, sizeof(t->pattern), "%s", pattern);
        snprintf(t->action, sizeof(t->action), "%s", action);
        t->gag = gag;
    }
    fclose(f);
}

/* Sends the ACTION of every trigger whose pattern matches the text
 * currently in pending_line_text, skipping any trigger that already
 * fired for THIS line (trigger_fired_this_line) so a prompt-flush and
 * the later newline-completion of the same line can't double-send.
 * Returns true if any matching trigger requested a gag; the caller
 * decides whether to act on it (only line-completion gags, since a gag
 * deletes already-displayed text and a mid-line prompt isn't a full
 * displayed line yet). Tracking fired triggers per-index (not a single
 * "line was prompt-fired" flag) means a line that arrives split across
 * TCP reads -- a non-matching partial flushed at a drain, then the rest
 * completing the match on newline -- still fires correctly: the partial
 * matched nothing, marked nothing, so the completed line is free to
 * fire. */
static bool trigger_fire_matches(void) {
    g_app.pending_line_text[g_app.pending_line_len] = '\0';
    bool gag = false;
    for (int i = 0; i < g_app.trigger_count; i++) {
        trigger_t *t = &g_app.triggers[i];
        if (!ci_contains(g_app.pending_line_text, t->pattern))
            continue;
        if (t->gag)
            gag = true;
        if (!g_app.trigger_fired_this_line[i] && t->action[0]
            && g_app.sock != INVALID_SOCKET) {
            char sendbuf[TRIGGER_ACTION_MAX + 4];
            int n = snprintf(sendbuf, sizeof(sendbuf), "%s\r\n", t->action);
            send(g_app.sock, sendbuf, n, 0);
            g_app.trigger_fired_this_line[i] = 1;
        }
    }
    return gag;
}

/* Called once a full incoming line's plain text has accumulated in
 * pending_line_text (a '\n' arrived). Fires any not-yet-fired matching
 * trigger actions; if ANY matching trigger is flagged gag, the line's
 * already-displayed RichEdit range ([line_start_offset, current end)) is
 * deleted right back out -- cheaper and far simpler than holding display
 * back until end-of-line is known, and the flash is imperceptible in
 * practice. Resets the per-line fired set for the next line. */
static void trigger_process_line(void) {
    bool gag = trigger_fire_matches();
    if (gag) {
        int end = GetWindowTextLengthW(g_app.hwnd_output);
        SendMessageW(g_app.hwnd_output, EM_SETSEL, g_app.line_start_offset, end);
        SendMessageW(g_app.hwnd_output, EM_REPLACESEL, FALSE, (LPARAM)L"");
    }
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
    memset(g_app.trigger_fired_this_line, 0, sizeof(g_app.trigger_fired_this_line));
}

/* Fires triggers against a PROMPT -- text the server left sitting in
 * pending_line_text with no trailing newline (a "HP:100 >"-style prompt).
 * The server negotiates SGA (suppress go-ahead), so there is no telnet
 * prompt marker to key off; instead poll_socket() calls this the moment a
 * socket read drains (WSAEWOULDBLOCK), i.e. the server has stopped
 * sending and is waiting on us -- the practical signal that a prompt is
 * up. Only the ACTION side fires here, never gag (the prompt is a live,
 * un-terminated display line, not a completed one to delete); the
 * per-line fired set makes this idempotent, so a static prompt sitting
 * across many 50ms poll ticks fires each matching trigger just once, and
 * the eventual newline that completes the prompt line won't re-send. */
static void trigger_flush_prompt(void) {
    if (g_app.pending_line_len == 0 || g_app.trigger_count == 0)
        return;
    trigger_fire_matches();
}

/* Feeds the same plain text append_output() just displayed into the
 * trigger line accumulator, splitting on '\n' -- text already displays
 * immediately regardless (see trigger_process_line()'s own comment on
 * why gag is retroactive instead of buffered). */
static void trigger_scan_feed(const char *text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            trigger_process_line();
            continue;
        }
        if (g_app.pending_line_len + 1 < sizeof(g_app.pending_line_text))
            g_app.pending_line_text[g_app.pending_line_len++] = text[i];
    }
}

static void aliases_path(char *out, size_t outsize) {
    snprintf(out, outsize, "%saliases.txt", g_app.exe_dir);
}

/* Loads aliases.txt (next to the exe, same folder as triggers.txt):
 * one alias per line, tab-separated -- NAME<TAB>EXPANSION. Whatever
 * the player types as the FIRST word of an input line is matched
 * case-insensitively, whole-word only (not a substring -- typing
 * "kill" must never accidentally trigger an alias named "k"); on a
 * match, EXPANSION replaces that first word and whatever the player
 * typed after it is carried through unchanged (classic "alias k kill"
 * -> "k rat" sends "kill rat"). Lines starting with # are comments.
 * First run with no file present seeds a commented example. */
static void load_aliases(void) {
    g_app.alias_count = 0;
    g_app.alias_comment_header[0] = '\0';
    char path[MAX_PATH + 32];
    aliases_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            fputs("# TobinMUD Client aliases -- one per line:\r\n"
                  "#   NAME<TAB>EXPANSION\r\n"
                  "# Typing NAME as the first word of a line sends EXPANSION instead,\r\n"
                  "# with anything else you typed after NAME carried through unchanged\r\n"
                  "# (e.g. \"k\" -> \"kill\" turns \"k rat\" into \"kill rat\").\r\n"
                  "# Reload after editing via File > Reload Aliases.\r\n"
                  "#\r\n"
                  "# Example (disabled -- remove the leading # to enable):\r\n"
                  "# k\tkill\r\n", f);
            fclose(f);
        }
        return;
    }
    char line[512];
    bool seen_entry = false;
    size_t hdr_len = 0;
    while (g_app.alias_count < ALIAS_MAX && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0 || line[0] == '#') {
            /* Leading comment/blank line -- stash it verbatim (see
             * alias_comment_header's own comment) as long as no real
             * alias has been seen yet. */
            if (!seen_entry && hdr_len < sizeof(g_app.alias_comment_header)) {
                int added = snprintf(g_app.alias_comment_header + hdr_len,
                                      sizeof(g_app.alias_comment_header) - hdr_len,
                                      "%s\r\n", line);
                if (added > 0) {
                    hdr_len += (size_t)added;
                    if (hdr_len >= sizeof(g_app.alias_comment_header))
                        hdr_len = sizeof(g_app.alias_comment_header) - 1;
                }
            }
            continue;
        }
        seen_entry = true;
        char *tab = strchr(line, '\t');
        if (!tab)
            continue;
        *tab = '\0';
        const char *name = line;
        const char *expansion = tab + 1;
        if (!name[0] || !expansion[0])
            continue;
        alias_t *a = &g_app.aliases[g_app.alias_count++];
        snprintf(a->name, sizeof(a->name), "%s", name);
        snprintf(a->expansion, sizeof(a->expansion), "%s", expansion);
    }
    fclose(f);
}

/* Expands `input`'s first word against g_app.aliases (case-insensitive,
 * whole-word match only) into `out`; copies input unchanged if no
 * alias matches. Safe to call with out == a separate buffer from input. */
static void expand_alias(const char *input, char *out, size_t outsize) {
    size_t i = 0;
    while (input[i] && input[i] != ' ')
        i++;
    size_t wordlen = i;
    const char *rest = input + i;
    while (*rest == ' ')
        rest++;

    for (int k = 0; k < g_app.alias_count; k++) {
        alias_t *a = &g_app.aliases[k];
        if (strlen(a->name) != wordlen)
            continue;
        if (_strnicmp(a->name, input, wordlen) != 0)
            continue;
        if (rest[0])
            snprintf(out, outsize, "%s %s", a->expansion, rest);
        else
            snprintf(out, outsize, "%s", a->expansion);
        return;
    }
    snprintf(out, outsize, "%s", input);
}

/* Writes g_app.triggers/trigger_count back to triggers.txt in the exact
 * same tab-delimited PATTERN<TAB>ACTION[<TAB>g] format load_triggers()
 * reads, so the file stays hand-editable even after being saved from
 * the GUI editor (see open_trigger_editor()). Any leading #-comment/
 * blank lines load_triggers() captured into trigger_comment_header are
 * re-emitted verbatim first -- see that field's own comment for what's
 * NOT preserved (comments after the first real trigger line). */
static bool save_triggers(void) {
    char path[MAX_PATH + 32];
    triggers_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    if (g_app.trigger_comment_header[0])
        fputs(g_app.trigger_comment_header, f);
    for (int i = 0; i < g_app.trigger_count; i++) {
        trigger_t *t = &g_app.triggers[i];
        fprintf(f, "%s\t%s%s\r\n", t->pattern, t->action, t->gag ? "\tg" : "");
    }
    fclose(f);
    return true;
}

/* Same idea as save_triggers(), for aliases.txt's NAME<TAB>EXPANSION
 * format (see open_alias_editor()). */
static bool save_aliases(void) {
    char path[MAX_PATH + 32];
    aliases_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    if (g_app.alias_comment_header[0])
        fputs(g_app.alias_comment_header, f);
    for (int i = 0; i < g_app.alias_count; i++) {
        alias_t *a = &g_app.aliases[i];
        fprintf(f, "%s\t%s\r\n", a->name, a->expansion);
    }
    fclose(f);
    return true;
}

static void ansi_emit_cb(void *ctx, const char *text, size_t len, int color_index, int bold) {
    (void)ctx;
    append_output(text, len, color_index, bold);
    trigger_scan_feed(text, len);
}

/* Plays (or, for `!!MUSIC(Off)`, stops) whatever `!!SOUND(...)`/
 * `!!MUSIC(...)` already extracted as its filename argument (or "Off"
 * for music). Both MUSIC and hit SOUND now loop/play through their
 * OWN dedicated MCI device alias ("tobinmusic" / "tobinhit") -- user,
 * 2026-08-06: "music plays, but as soon as a hit occurs the music
 * stops", confirmed via tobinmud_debug.log showing MCI was NOT
 * failing on this machine (no "MCI failed" line), which ruled out the
 * v0.4.5-era fallback path. The real culprit: PlaySoundA() and an
 * MCI-opened waveaudio device both ultimately reach for the same
 * default wave-out device, and Windows documents that a PlaySound()
 * call can preempt/stop audio already playing through an open MCI
 * wave alias -- using PlaySoundA() for the one-shot hit sound was
 * exactly that collision, even though it looked like "two independent
 * subsystems" from the API names alone. Giving the hit sound its own
 * MCI alias instead keeps both on the mechanism already proven to
 * work without contention on this setup (no MCI errors logged for
 * music), so the OS mixer genuinely overlaps them. PlaySoundA is kept
 * only as a last-resort fallback if MCI itself refuses to open for a
 * given file, same safety net as the music path already had. */
static void play_msp(const char *fname, bool loop) {
    if (loop) {
        char cmd[MAX_PATH + 128 + 64];
        mciSendStringA("close tobinmusic", NULL, 0, NULL);
        if (strcmp(fname, "Off") == 0) {
            /* Real fix for "after a fight ends music continues" (user,
             * 2026-08-06/07) -- confirmed live via tobinmud_debug.log:
             * MCI was failing to open "tobinmusic" at all (error 259,
             * "driver cannot recognize the specified command
             * parameter"), so every fight's music was actually playing
             * through the PlaySoundA(SND_LOOP) fallback below, not MCI.
             * Closing "tobinmusic" above is then a complete no-op --
             * there was never an MCI device to close -- so the looping
             * PlaySoundA fallback ran forever with nothing left to stop
             * it. Stopping PlaySoundA here too costs nothing when MCI
             * DID succeed (stopping a sound that isn't playing is a
             * harmless no-op), and actually silences the fallback case
             * when it didn't. */
            PlaySoundA(NULL, NULL, 0);
            return;
        }
        char fullpath[MAX_PATH + 128 + 16];
        snprintf(fullpath, sizeof(fullpath), "%ssounds\\%s", g_app.exe_dir, fname);
        snprintf(cmd, sizeof(cmd), "open \"%s\" type waveaudio alias tobinmusic", fullpath);
        MCIERROR err = mciSendStringA(cmd, NULL, 0, NULL);
        if (err == 0)
            err = mciSendStringA("play tobinmusic repeat", NULL, 0, NULL);
        if (err == 0)
            return;
        /* MCI failed (open or play) -- user, 2026-08-06: "the client
         * isnt playing music anymore" after the v0.4.5 MCI switch, on
         * some real-world setup MCI silently refused to cooperate.
         * Rather than leave music broken outright, fall back to the
         * pre-0.4.5 PlaySoundA(SND_LOOP) behavior -- loses the
         * play-over-hit-sounds overlap this session, but music
         * actually plays again, which matters more. Logged so a real
         * MCI error code is available if this needs a proper fix. */
        char errbuf[128];
        mciGetErrorStringA(err, errbuf, sizeof(errbuf));
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "play_msp: MCI failed (%lu: %s), falling back to PlaySoundA loop",
                 (unsigned long)err, errbuf);
        debug_log(logmsg);
        PlaySoundA(fullpath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT | SND_LOOP);
        return;
    }
    mciSendStringA("close tobinhit", NULL, 0, NULL);
    if (strcmp(fname, "Off") == 0)
        return;
    char fullpath[MAX_PATH + 128 + 16];
    snprintf(fullpath, sizeof(fullpath), "%ssounds\\%s", g_app.exe_dir, fname);
    char cmd[MAX_PATH + 128 + 64];
    snprintf(cmd, sizeof(cmd), "open \"%s\" type waveaudio alias tobinhit", fullpath);
    MCIERROR err = mciSendStringA(cmd, NULL, 0, NULL);
    if (err == 0)
        err = mciSendStringA("play tobinhit", NULL, 0, NULL);
    if (err == 0)
        return;
    /* Same last-resort fallback as the music path -- a one-shot via
     * PlaySoundA can still momentarily preempt concurrent MCI music if
     * this ever fires, but that only happens when MCI has already
     * failed to open the hit sound at all. */
    char errbuf[128];
    mciGetErrorStringA(err, errbuf, sizeof(errbuf));
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "play_msp: MCI failed for hit sound (%lu: %s), falling back to PlaySoundA",
             (unsigned long)err, errbuf);
    debug_log(logmsg);
    PlaySoundA(fullpath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
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

    /* Collapse "\r\n" to a bare "\r" (RichEdit's native paragraph
     * separator) -- feeding it literal \r\n doubles every line, and
     * fixing that via EM_SETTEXTMODE(TM_PLAINTEXT) instead (v0.4.15)
     * broke per-line ANSI colors, so it's done here instead. cr_pending
     * carries a split \r\n across socket-read boundaries. */
    size_t start_idx = 0;
    if (g_app.cr_pending) {
        g_app.cr_pending = false;
        if (clen > 0 && combined[0] == '\n')
            start_idx = 1;
    }
    {
        size_t w = 0;
        for (size_t r = start_idx; r < clen; r++) {
            if (combined[r] == '\r' && r + 1 < clen && combined[r + 1] == '\n') {
                combined[w++] = '\r';
                r++;
            } else {
                combined[w++] = combined[r];
            }
        }
        if (w > 0 && combined[w - 1] == '\r')
            g_app.cr_pending = true;
        clen = w;
    }

    size_t out_start = 0;
    size_t i = 0;
    while (i < clen) {
        size_t avail = clen - i;
        if (combined[i] == '!') {
            if (avail >= 8) {
                int is_sound = memcmp(combined + i, "!!SOUND(", 8) == 0;
                int is_music = memcmp(combined + i, "!!MUSIC(", 8) == 0;
                if (is_sound || is_music) {
                    /* Flush plain text before the marker. */
                    if (i > out_start)
                        ansi_client_feed(g_app.ansi, combined + out_start, i - out_start);
                    const char *close = memchr(combined + i, ')', avail);
                    if (!close) {
                        /* Marker not fully arrived yet -- hold the rest for next time. */
                        size_t rem = avail;
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
            } else if (memcmp(combined + i, "!!SOUND(", avail) == 0
                       || memcmp(combined + i, "!!MUSIC(", avail) == 0) {
                /* Fewer than 8 bytes left in this chunk, but what's here
                 * still matches the start of a marker -- a `!!MUSIC(...)`
                 * tag that lands right at a TCP packet boundary used to
                 * fall through to the `i++` below and get forwarded as
                 * plain text instead of being held, silently dropping
                 * the marker (the actual cause of "music sometimes just
                 * doesn't play" -- user, 2026-08-08 -- timing-dependent
                 * on how the server's output happened to be chunked,
                 * hence "sporadic"; exit+relog just got lucky with a
                 * different chunking next time). Hold it exactly like
                 * the close-paren-not-found case above. */
                if (i > out_start)
                    ansi_client_feed(g_app.ansi, combined + out_start, i - out_start);
                if (avail <= sizeof(g_app.sound_scan_buf)) {
                    memcpy(g_app.sound_scan_buf, combined + i, avail);
                    g_app.sound_scan_len = avail;
                }
                return;
            }
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

/* Updates one gauge (label text + bar position/color). `maxval <= 0`
 * (no pool at all, e.g. a non-Mage's mana) shows an empty, near-invisible
 * bar rather than a misleading full/zero one -- PBM_SETPOS still wants
 * a real 0-100 range either way. */
static void set_gauge(HWND label, HWND bar, const char *name, int val, int maxval) {
    char text[64];
    snprintf(text, sizeof(text), "%s %d/%d", name, val, maxval);
    wchar_t wtext[64];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 64);
    SetWindowTextW(label, wtext);
    int pct = (maxval > 0) ? (int)((long long)val * 100 / maxval) : 0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    SendMessageW(bar, PBM_SETPOS, (WPARAM)pct, 0);
}

static void telnet_on_gmcp(void *ctx, const char *package, const char *json) {
    (void)ctx;
    if (strcmp(package, "Char.Vitals") == 0) {
        int hp = 0, maxhp = 0, vit = 0, maxvit = 0, mana = 0, maxmana = 0;
        gmcp_json_get_int(json, "hp", &hp);
        gmcp_json_get_int(json, "maxhp", &maxhp);
        gmcp_json_get_int(json, "vit", &vit);
        gmcp_json_get_int(json, "maxvit", &maxvit);
        gmcp_json_get_int(json, "mana", &mana);
        gmcp_json_get_int(json, "maxmana", &maxmana);
        /* Per-class resource label (Mana/Piety/Lifeforce) the server
         * now sends alongside the numbers (gmcp.c
         * class_resource_label()); the gauge titles itself from it
         * instead of a hardcoded "Mana", so a Druid sees "Lifeforce"
         * and a Cleric "Piety". Falls back to "Mana" for an older
         * server that omits the field. */
        char manalabel[24];
        if (!gmcp_json_get_string(json, "manalabel", manalabel, sizeof(manalabel)))
            strcpy(manalabel, "Mana");
        set_gauge(g_app.hwnd_gauge_label_hp, g_app.hwnd_gauge_bar_hp, "HP", hp, maxhp);
        /* User, 2026-08-06: "no use for mana shouldnt display mana" --
         * maxmana is 0 for every class but Mage (being_calc_max_mana(),
         * c_port/src/core/being.c), so this is a real, server-driven
         * class check, not a guess. */
        if (maxmana > 0) {
            ShowWindow(g_app.hwnd_gauge_label_mana, SW_SHOW);
            ShowWindow(g_app.hwnd_gauge_bar_mana, SW_SHOW);
            set_gauge(g_app.hwnd_gauge_label_mana, g_app.hwnd_gauge_bar_mana, manalabel, mana, maxmana);
        } else {
            ShowWindow(g_app.hwnd_gauge_label_mana, SW_HIDE);
            ShowWindow(g_app.hwnd_gauge_bar_mana, SW_HIDE);
        }
        set_gauge(g_app.hwnd_gauge_label_move, g_app.hwnd_gauge_bar_move, "Move", vit, maxvit);
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
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
        return;
    }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET || connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        append_output("Could not connect.\r\n", 20, 1, 1);
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
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
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
}

static void do_reconnect(void) {
    do_disconnect(false);
    do_connect(DEFAULT_HOST, DEFAULT_PORT);
}

/* Clicking anywhere in the scrollback (user, 2026-08-06: "when
 * clicking on window put focus and cursor in the input line") --
 * the RichEdit output is read-only, so there's no editing reason for
 * it to hold keyboard focus; let its own click handling run first
 * (so click-drag text selection for copying still works), then hand
 * focus back to the input box right after. */
static LRESULT CALLBACK OutputSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    /* Right-click Copy/Select All (client TODO, "enable cut/copy/paste
     * in the client", 2026-08-21): a plain Edit control (hwnd_input)
     * already shows its own native Cut/Copy/Paste/Undo/Select All menu
     * on right-click for free, but RichEdit does not -- it needs one
     * built explicitly, or the read-only scrollback (the one place a
     * player actually wants to copy text OUT of -- room descriptions,
     * combat logs, etc.) has no discoverable way to do it besides
     * already-known Ctrl+C. TPM_RETURNCMD avoids routing this through
     * WM_COMMAND on the main window -- simpler to just act on the
     * result directly here. */
    if (msg == WM_CONTEXTMENU) {
        int x = (short)LOWORD(lp);
        int y = (short)HIWORD(lp);
        if (x == -1 && y == -1) { /* keyboard-invoked (Shift+F10/Menu key) */
            RECT rc;
            GetWindowRect(hwnd, &rc);
            x = rc.left + 10;
            y = rc.top + 10;
        }
        HMENU pop = CreatePopupMenu();
        AppendMenuW(pop, MF_STRING, ID_MENU_EDIT_COPY, L"&Copy");
        AppendMenuW(pop, MF_STRING, ID_MENU_EDIT_SELECTALL, L"Select &All");
        int cmd = TrackPopupMenu(pop, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, hwnd, NULL);
        DestroyMenu(pop);
        if (cmd == ID_MENU_EDIT_COPY)
            SendMessageW(hwnd, WM_COPY, 0, 0);
        else if (cmd == ID_MENU_EDIT_SELECTALL)
            SendMessageW(hwnd, EM_SETSEL, 0, -1);
        return 0;
    }
    LRESULT result = CallWindowProc(g_app.output_orig_proc, hwnd, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
        SetFocus(g_app.hwnd_input);
    return result;
}

static LRESULT CALLBACK InputSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    /* A single-line Edit control has no use for a carriage-return
     * character (that is what ES_WANTRETURN/multi-line is for) -- its
     * default WndProc treats WM_CHAR==VK_RETURN as unhandled input and
     * calls MessageBeep(), which is the "ding" the user heard on every
     * command (2026-08-06). WM_KEYDOWN is swallowed just below already
     * (to send the command instead of forwarding it), but WM_CHAR is a
     * separate message the default proc still sees unless this also
     * swallows it. */
    if (msg == WM_CHAR && wp == VK_RETURN)
        return 0;
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
         * always sends as typed (through alias expansion -- user,
         * 2026-08-06: "make the client behave as much as possible to
         * Mudlet") and becomes the new "last command", even if it's
         * identical to the previous one. History stores what the
         * player actually TYPED (pre-expansion), so Up/Down shows
         * their own shorthand back, not the expanded form; last_line
         * stores the EXPANDED form, since a bare-Enter repeat means
         * "do that same network action again." */
        char expanded[1024 + ALIAS_EXPANSION_MAX];
        const char *to_send = buf;
        if (len == 0) {
            if (g_app.last_line[0] == '\0')
                return 0; /* nothing sent yet this session -- truly a no-op */
            to_send = g_app.last_line;
        } else {
            expand_alias(buf, expanded, sizeof(expanded));
            to_send = expanded;
            snprintf(g_app.last_line, sizeof(g_app.last_line), "%s", expanded);
            /* History push -- only real, non-empty, TYPED lines (not
             * the bare-Enter repeat above, which would otherwise pile
             * up duplicate entries every time the player just mashes
             * Enter to repeat an attack). Drop the oldest entry once
             * full rather than growing unbounded. */
            if (g_app.history_count == HISTORY_MAX) {
                memmove(g_app.history[0], g_app.history[1],
                        sizeof(g_app.history[0]) * (HISTORY_MAX - 1));
                g_app.history_count--;
            }
            snprintf(g_app.history[g_app.history_count], sizeof(g_app.history[0]), "%s", buf);
            g_app.history_count++;
        }
        g_app.history_pos = g_app.history_count; /* stop browsing, back to a fresh line */
        g_app.history_pending[0] = '\0';
        if (g_app.sock != INVALID_SOCKET) {
            char line[1024 + ALIAS_EXPANSION_MAX + 8];
            int n = snprintf(line, sizeof(line), "%s\r\n", to_send);
            send(g_app.sock, line, n, 0);
        }
        return 0;
    }
    /* Command history (user, 2026-08-06: "make the client behave as
     * much as possible to Mudlet") -- Up/Down recall the player's own
     * previously SENT lines, shell-style. The first Up press saves
     * whatever's currently typed (possibly nothing) into
     * history_pending so Down can walk back past the newest history
     * entry to restore it, rather than just clearing the box. */
    if (msg == WM_KEYDOWN && (wp == VK_UP || wp == VK_DOWN)) {
        if (g_app.history_count == 0)
            return 0;
        if (wp == VK_UP) {
            if (g_app.history_pos == g_app.history_count)
                GetWindowTextA(hwnd, g_app.history_pending, sizeof(g_app.history_pending));
            if (g_app.history_pos > 0)
                g_app.history_pos--;
        } else {
            if (g_app.history_pos < g_app.history_count)
                g_app.history_pos++;
        }
        const char *show = (g_app.history_pos == g_app.history_count)
                                ? g_app.history_pending
                                : g_app.history[g_app.history_pos];
        SetWindowTextA(hwnd, show);
        int end = (int)strlen(show);
        SendMessageA(hwnd, EM_SETSEL, end, end);
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
        if (err == WSAEWOULDBLOCK) {
            /* The server has stopped sending and is waiting on us -- the
             * practical "a prompt is up" signal (SGA is negotiated, so
             * there's no telnet GA/EOR marker). Fire any triggers that
             * match the un-terminated prompt line still sitting in the
             * accumulator, so prompt triggers work, not just triggers on
             * newline-terminated lines. */
            trigger_flush_prompt();
            return; /* nothing more right now */
        }
        append_output("\r\n-- Connection error --\r\n", 26, 1, 1);
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
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
/* Reads Windows' own light/dark mode setting (Settings > Personalization
 * > Colors > "Choose your mode"/"Choose your default app mode") --
 * the same registry value Explorer/every theme-aware app reads. Missing
 * key (pre-Win10-1903, or a locked-down policy) defaults to dark, this
 * client's original look, so nothing changes for anyone not on a
 * theme-aware build. */
static bool system_prefers_dark_theme(void) {
    HKEY hkey;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hkey);
    if (rc != ERROR_SUCCESS)
        return true;
    DWORD value = 1, size = sizeof(value), type = 0;
    rc = RegQueryValueExA(hkey, "AppsUseLightTheme", NULL, &type,
                           (LPBYTE)&value, &size);
    RegCloseKey(hkey);
    if (rc != ERROR_SUCCESS || type != REG_DWORD)
        return true;
    return value == 0;
}

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

    GetPrivateProfileStringA("Prefs", "FontFace", "Courier New",
                             g_app.font_face, sizeof(g_app.font_face), path);
    if (g_app.font_face[0] == '\0')
        strcpy(g_app.font_face, "Courier New");

    g_app.win_w = GetPrivateProfileIntA("Prefs", "WindowWidth", PREFS_DEFAULT_WIN_W, path);
    if (g_app.win_w < PREFS_MIN_WIN_W) g_app.win_w = PREFS_MIN_WIN_W;

    g_app.win_h = GetPrivateProfileIntA("Prefs", "WindowHeight", PREFS_DEFAULT_WIN_H, path);
    if (g_app.win_h < PREFS_MIN_WIN_H) g_app.win_h = PREFS_MIN_WIN_H;

    g_app.fullscreen = GetPrivateProfileIntA("Prefs", "Fullscreen", 0, path) != 0;
}

static void save_prefs(void) {
    char path[MAX_PATH + 16];
    prefs_ini_path(path, sizeof(path));

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g_app.font_pt);
    WritePrivateProfileStringA("Prefs", "FontSize", buf, path);
    WritePrivateProfileStringA("Prefs", "FontFace", g_app.font_face, path);
    snprintf(buf, sizeof(buf), "%d", g_app.win_w);
    WritePrivateProfileStringA("Prefs", "WindowWidth", buf, path);
    snprintf(buf, sizeof(buf), "%d", g_app.win_h);
    WritePrivateProfileStringA("Prefs", "WindowHeight", buf, path);
    WritePrivateProfileStringA("Prefs", "Fullscreen", g_app.fullscreen ? "1" : "0", path);
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

    wchar_t wface[LF_FACESIZE];
    MultiByteToWideChar(CP_ACP, 0,
                        g_app.font_face[0] ? g_app.font_face : "Courier New", -1,
                        wface, LF_FACESIZE);

    g_app.font = CreateFontW(px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wface);

    if (g_app.hwnd_input)
        SendMessageW(g_app.hwnd_input, WM_SETFONT, (WPARAM)g_app.font, TRUE);

    if (g_app.hwnd_output) {
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE;
        wcscpy(cf.szFaceName, wface);
        cf.yHeight = g_app.font_pt * 20; /* twips (1/20 pt) */

        /* Second, smaller contributor to "still some space" (user,
         * 2026-08-06) on top of the real EM_SETTEXTMODE/TM_PLAINTEXT
         * fix above -- Msftedit's default paragraph format still
         * carries its own non-zero space-after-paragraph value even in
         * plain-text mode. A prior pass added this exact zeroing and
         * saw no visible change, then reverted it as "ineffective" --
         * wrongly: at that point the much larger \r\n-doubling bug
         * was still present and completely masked this smaller effect.
         * Re-added now that the real cause is fixed, so this can
         * actually be seen to work or not. */
        PARAFORMAT2 pf;
        ZeroMemory(&pf, sizeof(pf));
        pf.cbSize = sizeof(pf);
        pf.dwMask = PFM_SPACEBEFORE | PFM_SPACEAFTER | PFM_LINESPACING;
        pf.dySpaceBefore = 0;
        pf.dySpaceAfter = 0;
        pf.bLineSpacingRule = 0; /* single */

        SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);

        LRESULT saved_start, saved_end;
        SendMessageW(g_app.hwnd_output, EM_GETSEL, (WPARAM)&saved_start, (LPARAM)&saved_end);
        SendMessageW(g_app.hwnd_output, EM_SETSEL, 0, -1);
        SendMessageW(g_app.hwnd_output, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        SendMessageW(g_app.hwnd_output, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
        SendMessageW(g_app.hwnd_output, EM_SETSEL, saved_start, saved_end);
    }
}

/* Toggles borderless full screen on `hwnd`, covering whichever monitor
 * it's currently on. Saves the windowed rect/style the first time it's
 * turned on so turning it back off restores exactly where the window
 * was -- guarded by the early-return so a repeat "turn on" call (e.g.
 * re-opening Preferences and hitting OK again with the box still
 * checked) can't clobber that saved state with the current (already
 * full-screen) geometry. */
static void apply_fullscreen(HWND hwnd, bool enable) {
    if (enable == g_app.fullscreen)
        return;
    if (enable) {
        GetWindowRect(hwnd, &g_app.windowed_rect);
        g_app.windowed_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(mon, &mi);
        SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, g_app.windowed_style);
        SetWindowPos(hwnd, NULL, g_app.windowed_rect.left, g_app.windowed_rect.top,
                     g_app.windowed_rect.right - g_app.windowed_rect.left,
                     g_app.windowed_rect.bottom - g_app.windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER);
    }
    g_app.fullscreen = enable;
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
static wchar_t g_prefs_pending_face[LF_FACESIZE]; /* font family picked via
    the Preferences "Choose..." button (ChooseFontW), held between that
    click and OK; seeded from g_app.font_face in open_preferences(). */

static LRESULT CALLBACK PrefsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_PREFS_FONT_CHOOSE) {
            /* Standard font common dialog, seeded with the current face
             * and the size currently in the box; its OK feeds both the
             * face (kept in g_prefs_pending_face) and size back in. */
            LOGFONTW lf;
            ZeroMemory(&lf, sizeof(lf));
            wcsncpy(lf.lfFaceName, g_prefs_pending_face, LF_FACESIZE - 1);
            wchar_t sb[16];
            GetDlgItemTextW(hwnd, ID_PREFS_FONTSIZE_EDIT, sb, 16);
            int pt = _wtoi(sb);
            if (pt < PREFS_MIN_FONT_PT || pt > PREFS_MAX_FONT_PT)
                pt = g_app.font_pt;
            HDC hdc = GetDC(hwnd);
            lf.lfHeight = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
            ReleaseDC(hwnd, hdc);
            CHOOSEFONTW cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.lStructSize = sizeof(cf);
            cf.hwndOwner = hwnd;
            cf.lpLogFont = &lf;
            cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_FORCEFONTEXIST;
            if (ChooseFontW(&cf)) {
                wcsncpy(g_prefs_pending_face, lf.lfFaceName, LF_FACESIZE - 1);
                g_prefs_pending_face[LF_FACESIZE - 1] = L'\0';
                SetDlgItemTextW(hwnd, ID_PREFS_FONT_FACE_STATIC, g_prefs_pending_face);
                int chosen_pt = cf.iPointSize / 10;
                if (chosen_pt >= PREFS_MIN_FONT_PT && chosen_pt <= PREFS_MAX_FONT_PT) {
                    wchar_t nb[16];
                    swprintf(nb, 16, L"%d", chosen_pt);
                    SetDlgItemTextW(hwnd, ID_PREFS_FONTSIZE_EDIT, nb);
                }
            }
            return 0;
        }
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

            bool fullscreen = IsDlgButtonChecked(hwnd, ID_PREFS_FULLSCREEN_CHECK) == BST_CHECKED;

            g_app.font_pt = font_pt;
            g_app.win_w = win_w;
            g_app.win_h = win_h;
            WideCharToMultiByte(CP_ACP, 0, g_prefs_pending_face, -1,
                                g_app.font_face, sizeof(g_app.font_face), NULL, NULL);
            if (g_app.font_face[0] == '\0')
                strcpy(g_app.font_face, "Courier New");
            apply_font();
            HWND main_hwnd = GetParent(g_app.hwnd_output);
            if (fullscreen) {
                apply_fullscreen(main_hwnd, true);
            } else {
                apply_fullscreen(main_hwnd, false);
                SetWindowPos(main_hwnd, NULL, 0, 0, win_w, win_h, SWP_NOMOVE | SWP_NOZORDER);
            }
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
    /* Real root cause of "the windows title bar still says T" (user,
     * 2026-08-06) -- both windows here are registered via RegisterClassW
     * (genuine Unicode windows), but the plain DefWindowProc() macro
     * resolves to DefWindowProcA with no UNICODE/_UNICODE defined
     * anywhere in this build. CreateWindowW sets the initial caption via
     * an internal WM_SETTEXT carrying a wide-string pointer; falling
     * through to the ANSI DefWindowProcA for that unhandled message
     * misreads it byte-by-byte -- 'T' is 0x54,0x00 in UTF-16LE, and that
     * trailing zero byte reads as an immediate ANSI string terminator,
     * so only the first character ever survives. Must be DefWindowProcW
     * for any window whose class was registered with RegisterClassW. */
    return DefWindowProcW(hwnd, msg, wp, lp);
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

    const int win_w = 320, win_h = 256;
    RECT pr;
    GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - win_w) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - win_h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TobinMUDPrefsWindow", L"Preferences",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, win_w, win_h, parent, NULL, GetModuleHandleW(NULL), NULL);
    g_app.hwnd_prefs = hwnd;

    /* Seed the pending font family from the saved preference so the
     * "Choose..." dialog and the face label both start on the current
     * font even if the user never opens the picker. */
    MultiByteToWideChar(CP_ACP, 0,
                        g_app.font_face[0] ? g_app.font_face : "Courier New", -1,
                        g_prefs_pending_face, LF_FACESIZE);

    HFONT dlgfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND lbl1 = CreateWindowW(L"STATIC", L"Font size (points):", WS_CHILD | WS_VISIBLE,
        16, 16, 150, 20, hwnd, NULL, NULL, NULL);
    wchar_t buf[16];
    swprintf(buf, 16, L"%d", g_app.font_pt);
    HWND e1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        190, 14, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_FONTSIZE_EDIT, NULL, NULL);

    /* Font family picker (user: "in preferences to choose your own font
     * for display") -- the current face name plus a Choose... button
     * that opens the standard ChooseFontW dialog (handled in
     * PrefsWndProc). Face is applied on OK from g_prefs_pending_face. */
    HWND lblF = CreateWindowW(L"STATIC", L"Font:", WS_CHILD | WS_VISIBLE,
        16, 50, 40, 20, hwnd, NULL, NULL, NULL);
    HWND faceLbl = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", g_prefs_pending_face,
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_PATHELLIPSIS,
        58, 48, 116, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_FONT_FACE_STATIC, NULL, NULL);
    HWND chooseBtn = CreateWindowW(L"BUTTON", L"Choose...", WS_CHILD | WS_VISIBLE,
        190, 47, 90, 24, hwnd, (HMENU)(INT_PTR)ID_PREFS_FONT_CHOOSE, NULL, NULL);

    HWND lbl2 = CreateWindowW(L"STATIC", L"Window width (px):", WS_CHILD | WS_VISIBLE,
        16, 84, 150, 20, hwnd, NULL, NULL, NULL);
    swprintf(buf, 16, L"%d", g_app.win_w);
    HWND e2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        190, 82, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_WIDTH_EDIT, NULL, NULL);

    HWND lbl3 = CreateWindowW(L"STATIC", L"Window height (px):", WS_CHILD | WS_VISIBLE,
        16, 116, 150, 20, hwnd, NULL, NULL, NULL);
    swprintf(buf, 16, L"%d", g_app.win_h);
    HWND e3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
        190, 114, 90, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_HEIGHT_EDIT, NULL, NULL);

    /* User, 2026-08-06: "a full screen option should be in
     * preferences" -- ignores the width/height fields above while
     * checked (apply_fullscreen() overrides the window's actual
     * geometry), but they stay saved/editable for whenever it's
     * unchecked again. */
    HWND chk1 = CreateWindowW(L"BUTTON", L"Full Screen", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 148, 150, 22, hwnd, (HMENU)(INT_PTR)ID_PREFS_FULLSCREEN_CHECK, NULL, NULL);
    SendMessageW(chk1, BM_SETCHECK, g_app.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);

    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        100, 194, 80, 26, hwnd, (HMENU)(INT_PTR)ID_PREFS_OK, NULL, NULL);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        190, 194, 80, 26, hwnd, (HMENU)(INT_PTR)ID_PREFS_CANCEL, NULL, NULL);

    HWND ctrls[] = { lbl1, e1, lblF, faceLbl, chooseBtn, lbl2, e2, lbl3, e3, chk1, ok, cancel };
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++)
        SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)dlgfont, TRUE);

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(e1);
}

/* -- Trigger/alias GUI editors (client backlog, logged 2026-08-06 --
 * "an in-client GUI editor for triggers and aliases") --
 *
 * Same hand-built-popup-window approach as Preferences above (no RC/
 * dialog-template step wired up in CMakeLists.txt), sized up to fit a
 * report-view SysListView32 of the existing entries plus a small
 * add/edit form underneath. Editing happens against a private staging
 * copy (s_trig_edit_buf/s_trig_edit_count below) rather than the live
 * g_app.triggers[] array directly -- the poll timer keeps running while
 * this window is open (EnableWindow(parent, FALSE) blocks player input
 * but not WM_TIMER), so incoming MUD text could otherwise be matched
 * against a half-edited trigger list mid-edit. The staging copy is only
 * committed to g_app.triggers[]/written to disk on Save; Cancel (or the
 * window's own close box) just discards it. */
static trigger_t s_trig_edit_buf[TRIGGER_MAX];
static int s_trig_edit_count;

static void trigedit_setup_columns(HWND list) {
    LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.iSubItem = 0;
    col.cx = 220;
    col.pszText = (LPWSTR)L"Pattern";
    SendMessageW(list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
    col.iSubItem = 1;
    col.cx = 230;
    col.pszText = (LPWSTR)L"Action";
    SendMessageW(list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
    col.iSubItem = 2;
    col.cx = 50;
    col.pszText = (LPWSTR)L"Gag";
    SendMessageW(list, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);
}

static void trigedit_refresh_list(HWND list) {
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
    for (int i = 0; i < s_trig_edit_count; i++) {
        trigger_t *t = &s_trig_edit_buf[i];
        wchar_t wpat[TRIGGER_PATTERN_MAX], wact[TRIGGER_ACTION_MAX];
        MultiByteToWideChar(CP_UTF8, 0, t->pattern, -1, wpat, TRIGGER_PATTERN_MAX);
        MultiByteToWideChar(CP_UTF8, 0, t->action, -1, wact, TRIGGER_ACTION_MAX);

        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = wpat;
        SendMessageW(list, LVM_INSERTITEMW, 0, (LPARAM)&item);

        item.iSubItem = 1;
        item.pszText = wact;
        SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&item);

        item.iSubItem = 2;
        item.pszText = t->gag ? (LPWSTR)L"Yes" : (LPWSTR)L"";
        SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&item);
    }
}

static void trigedit_populate_fields(HWND hwnd, int idx) {
    if (idx < 0 || idx >= s_trig_edit_count)
        return;
    trigger_t *t = &s_trig_edit_buf[idx];
    wchar_t wpat[TRIGGER_PATTERN_MAX], wact[TRIGGER_ACTION_MAX];
    MultiByteToWideChar(CP_UTF8, 0, t->pattern, -1, wpat, TRIGGER_PATTERN_MAX);
    MultiByteToWideChar(CP_UTF8, 0, t->action, -1, wact, TRIGGER_ACTION_MAX);
    SetDlgItemTextW(hwnd, ID_TRIGEDIT_PATTERN_EDIT, wpat);
    SetDlgItemTextW(hwnd, ID_TRIGEDIT_ACTION_EDIT, wact);
    CheckDlgButton(hwnd, ID_TRIGEDIT_GAG_CHECK, t->gag ? BST_CHECKED : BST_UNCHECKED);
}

static int trigedit_get_selected(HWND list) {
    return (int)SendMessageW(list, LVM_GETNEXTITEM, (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
}

static LRESULT CALLBACK TrigEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == ID_TRIGEDIT_LIST && nm->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *nmlv = (NMLISTVIEW *)lp;
            if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED))
                trigedit_populate_fields(hwnd, nmlv->iItem);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        HWND list = GetDlgItem(hwnd, ID_TRIGEDIT_LIST);
        switch (id) {
        case ID_TRIGEDIT_ADD: {
            if (s_trig_edit_count >= TRIGGER_MAX) {
                MessageBoxW(hwnd, L"Maximum number of triggers reached.", L"Triggers", MB_OK | MB_ICONWARNING);
                return 0;
            }
            wchar_t wpat[TRIGGER_PATTERN_MAX], wact[TRIGGER_ACTION_MAX];
            GetDlgItemTextW(hwnd, ID_TRIGEDIT_PATTERN_EDIT, wpat, TRIGGER_PATTERN_MAX);
            GetDlgItemTextW(hwnd, ID_TRIGEDIT_ACTION_EDIT, wact, TRIGGER_ACTION_MAX);
            if (wpat[0] == 0) {
                MessageBoxW(hwnd, L"Pattern cannot be empty.", L"Triggers", MB_OK | MB_ICONWARNING);
                return 0;
            }
            trigger_t *t = &s_trig_edit_buf[s_trig_edit_count++];
            WideCharToMultiByte(CP_UTF8, 0, wpat, -1, t->pattern, (int)sizeof(t->pattern), NULL, NULL);
            t->pattern[sizeof(t->pattern) - 1] = '\0';
            WideCharToMultiByte(CP_UTF8, 0, wact, -1, t->action, (int)sizeof(t->action), NULL, NULL);
            t->action[sizeof(t->action) - 1] = '\0';
            t->gag = IsDlgButtonChecked(hwnd, ID_TRIGEDIT_GAG_CHECK) == BST_CHECKED;
            trigedit_refresh_list(list);
            SetDlgItemTextW(hwnd, ID_TRIGEDIT_PATTERN_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_TRIGEDIT_ACTION_EDIT, L"");
            CheckDlgButton(hwnd, ID_TRIGEDIT_GAG_CHECK, BST_UNCHECKED);
            SetFocus(GetDlgItem(hwnd, ID_TRIGEDIT_PATTERN_EDIT));
            return 0;
        }
        case ID_TRIGEDIT_UPDATE: {
            int sel = trigedit_get_selected(list);
            if (sel < 0) {
                MessageBoxW(hwnd, L"Select a trigger to update first.", L"Triggers", MB_OK | MB_ICONWARNING);
                return 0;
            }
            wchar_t wpat[TRIGGER_PATTERN_MAX], wact[TRIGGER_ACTION_MAX];
            GetDlgItemTextW(hwnd, ID_TRIGEDIT_PATTERN_EDIT, wpat, TRIGGER_PATTERN_MAX);
            GetDlgItemTextW(hwnd, ID_TRIGEDIT_ACTION_EDIT, wact, TRIGGER_ACTION_MAX);
            if (wpat[0] == 0) {
                MessageBoxW(hwnd, L"Pattern cannot be empty.", L"Triggers", MB_OK | MB_ICONWARNING);
                return 0;
            }
            trigger_t *t = &s_trig_edit_buf[sel];
            WideCharToMultiByte(CP_UTF8, 0, wpat, -1, t->pattern, (int)sizeof(t->pattern), NULL, NULL);
            t->pattern[sizeof(t->pattern) - 1] = '\0';
            WideCharToMultiByte(CP_UTF8, 0, wact, -1, t->action, (int)sizeof(t->action), NULL, NULL);
            t->action[sizeof(t->action) - 1] = '\0';
            t->gag = IsDlgButtonChecked(hwnd, ID_TRIGEDIT_GAG_CHECK) == BST_CHECKED;
            trigedit_refresh_list(list);
            return 0;
        }
        case ID_TRIGEDIT_DELETE: {
            int sel = trigedit_get_selected(list);
            if (sel < 0) {
                MessageBoxW(hwnd, L"Select a trigger to delete first.", L"Triggers", MB_OK | MB_ICONWARNING);
                return 0;
            }
            for (int i = sel; i < s_trig_edit_count - 1; i++)
                s_trig_edit_buf[i] = s_trig_edit_buf[i + 1];
            s_trig_edit_count--;
            trigedit_refresh_list(list);
            SetDlgItemTextW(hwnd, ID_TRIGEDIT_PATTERN_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_TRIGEDIT_ACTION_EDIT, L"");
            CheckDlgButton(hwnd, ID_TRIGEDIT_GAG_CHECK, BST_UNCHECKED);
            return 0;
        }
        case ID_TRIGEDIT_SAVE: {
            g_app.trigger_count = s_trig_edit_count;
            for (int i = 0; i < s_trig_edit_count; i++)
                g_app.triggers[i] = s_trig_edit_buf[i];
            bool ok = save_triggers();
            EnableWindow(GetParent(g_app.hwnd_output), TRUE);
            DestroyWindow(hwnd);
            SetFocus(g_app.hwnd_input);
            char msg[80];
            int mlen;
            if (ok)
                mlen = snprintf(msg, sizeof(msg), "Saved %d trigger(s) to triggers.txt.\r\n", g_app.trigger_count);
            else
                mlen = snprintf(msg, sizeof(msg), "Failed to save triggers.txt!\r\n");
            append_output(msg, (size_t)mlen, ok ? 3 : 1, 0);
            g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
            g_app.pending_line_len = 0;
            return 0;
        }
        case ID_TRIGEDIT_CANCEL:
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
        g_app.hwnd_trigedit = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void open_trigger_editor(HWND parent) {
    if (g_app.hwnd_trigedit) {
        SetForegroundWindow(g_app.hwnd_trigedit);
        return;
    }
    load_triggers(); /* pick up any hand-edits made outside the client since it started */
    s_trig_edit_count = g_app.trigger_count;
    for (int i = 0; i < s_trig_edit_count; i++)
        s_trig_edit_buf[i] = g_app.triggers[i];

    static bool cls_registered = false;
    if (!cls_registered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = TrigEditWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"TobinMUDTrigEditWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        cls_registered = true;
    }

    const int win_w = 600, win_h = 430;
    RECT pr;
    GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - win_w) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - win_h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TobinMUDTrigEditWindow", L"Edit Triggers",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, win_w, win_h, parent, NULL, GetModuleHandleW(NULL), NULL);
    g_app.hwnd_trigedit = hwnd;

    HFONT dlgfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 16, 16, 552, 190,
        hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_LIST, NULL, NULL);
    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    trigedit_setup_columns(list);
    trigedit_refresh_list(list);

    HWND lbl1 = CreateWindowW(L"STATIC", L"Pattern:", WS_CHILD | WS_VISIBLE,
        16, 216, 60, 20, hwnd, NULL, NULL, NULL);
    HWND e1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        80, 214, 220, 22, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_PATTERN_EDIT, NULL, NULL);
    HWND lbl2 = CreateWindowW(L"STATIC", L"Action:", WS_CHILD | WS_VISIBLE,
        312, 216, 50, 20, hwnd, NULL, NULL, NULL);
    HWND e2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        366, 214, 200, 22, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_ACTION_EDIT, NULL, NULL);
    HWND chk = CreateWindowW(L"BUTTON", L"Gag (hide the matched line)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 244, 260, 20, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_GAG_CHECK, NULL, NULL);

    HWND add = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE,
        16, 276, 90, 26, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_ADD, NULL, NULL);
    HWND upd = CreateWindowW(L"BUTTON", L"Update Selected", WS_CHILD | WS_VISIBLE,
        114, 276, 130, 26, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_UPDATE, NULL, NULL);
    HWND del = CreateWindowW(L"BUTTON", L"Delete Selected", WS_CHILD | WS_VISIBLE,
        252, 276, 130, 26, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_DELETE, NULL, NULL);

    HWND save = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        400, 320, 80, 28, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_SAVE, NULL, NULL);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        488, 320, 80, 28, hwnd, (HMENU)(INT_PTR)ID_TRIGEDIT_CANCEL, NULL, NULL);

    HWND ctrls[] = { lbl1, e1, lbl2, e2, chk, add, upd, del, save, cancel };
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++)
        SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)dlgfont, TRUE);
    SendMessageW(list, WM_SETFONT, (WPARAM)dlgfont, TRUE);

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(e1);
}

/* -- Alias GUI editor -- same shape as the trigger editor above, just
 * two columns (Name/Expansion) and no gag checkbox. See
 * open_trigger_editor()'s own comment for the staging-copy rationale. */
static alias_t s_alias_edit_buf[ALIAS_MAX];
static int s_alias_edit_count;

static void aliasedit_setup_columns(HWND list) {
    LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.iSubItem = 0;
    col.cx = 140;
    col.pszText = (LPWSTR)L"Name";
    SendMessageW(list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
    col.iSubItem = 1;
    col.cx = 380;
    col.pszText = (LPWSTR)L"Expansion";
    SendMessageW(list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
}

static void aliasedit_refresh_list(HWND list) {
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
    for (int i = 0; i < s_alias_edit_count; i++) {
        alias_t *a = &s_alias_edit_buf[i];
        wchar_t wname[ALIAS_NAME_MAX], wexp[ALIAS_EXPANSION_MAX];
        MultiByteToWideChar(CP_UTF8, 0, a->name, -1, wname, ALIAS_NAME_MAX);
        MultiByteToWideChar(CP_UTF8, 0, a->expansion, -1, wexp, ALIAS_EXPANSION_MAX);

        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = wname;
        SendMessageW(list, LVM_INSERTITEMW, 0, (LPARAM)&item);

        item.iSubItem = 1;
        item.pszText = wexp;
        SendMessageW(list, LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&item);
    }
}

static void aliasedit_populate_fields(HWND hwnd, int idx) {
    if (idx < 0 || idx >= s_alias_edit_count)
        return;
    alias_t *a = &s_alias_edit_buf[idx];
    wchar_t wname[ALIAS_NAME_MAX], wexp[ALIAS_EXPANSION_MAX];
    MultiByteToWideChar(CP_UTF8, 0, a->name, -1, wname, ALIAS_NAME_MAX);
    MultiByteToWideChar(CP_UTF8, 0, a->expansion, -1, wexp, ALIAS_EXPANSION_MAX);
    SetDlgItemTextW(hwnd, ID_ALIASEDIT_NAME_EDIT, wname);
    SetDlgItemTextW(hwnd, ID_ALIASEDIT_EXPANSION_EDIT, wexp);
}

static LRESULT CALLBACK AliasEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == ID_ALIASEDIT_LIST && nm->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *nmlv = (NMLISTVIEW *)lp;
            if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED))
                aliasedit_populate_fields(hwnd, nmlv->iItem);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        HWND list = GetDlgItem(hwnd, ID_ALIASEDIT_LIST);
        switch (id) {
        case ID_ALIASEDIT_ADD: {
            if (s_alias_edit_count >= ALIAS_MAX) {
                MessageBoxW(hwnd, L"Maximum number of aliases reached.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            wchar_t wname[ALIAS_NAME_MAX], wexp[ALIAS_EXPANSION_MAX];
            GetDlgItemTextW(hwnd, ID_ALIASEDIT_NAME_EDIT, wname, ALIAS_NAME_MAX);
            GetDlgItemTextW(hwnd, ID_ALIASEDIT_EXPANSION_EDIT, wexp, ALIAS_EXPANSION_MAX);
            if (wname[0] == 0 || wexp[0] == 0) {
                MessageBoxW(hwnd, L"Both Name and Expansion are required.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (wcschr(wname, L' ')) {
                MessageBoxW(hwnd, L"Alias name cannot contain spaces.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            alias_t *a = &s_alias_edit_buf[s_alias_edit_count++];
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, a->name, (int)sizeof(a->name), NULL, NULL);
            a->name[sizeof(a->name) - 1] = '\0';
            WideCharToMultiByte(CP_UTF8, 0, wexp, -1, a->expansion, (int)sizeof(a->expansion), NULL, NULL);
            a->expansion[sizeof(a->expansion) - 1] = '\0';
            aliasedit_refresh_list(list);
            SetDlgItemTextW(hwnd, ID_ALIASEDIT_NAME_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_ALIASEDIT_EXPANSION_EDIT, L"");
            SetFocus(GetDlgItem(hwnd, ID_ALIASEDIT_NAME_EDIT));
            return 0;
        }
        case ID_ALIASEDIT_UPDATE: {
            int sel = (int)SendMessageW(list, LVM_GETNEXTITEM, (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
            if (sel < 0) {
                MessageBoxW(hwnd, L"Select an alias to update first.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            wchar_t wname[ALIAS_NAME_MAX], wexp[ALIAS_EXPANSION_MAX];
            GetDlgItemTextW(hwnd, ID_ALIASEDIT_NAME_EDIT, wname, ALIAS_NAME_MAX);
            GetDlgItemTextW(hwnd, ID_ALIASEDIT_EXPANSION_EDIT, wexp, ALIAS_EXPANSION_MAX);
            if (wname[0] == 0 || wexp[0] == 0) {
                MessageBoxW(hwnd, L"Both Name and Expansion are required.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (wcschr(wname, L' ')) {
                MessageBoxW(hwnd, L"Alias name cannot contain spaces.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            alias_t *a = &s_alias_edit_buf[sel];
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, a->name, (int)sizeof(a->name), NULL, NULL);
            a->name[sizeof(a->name) - 1] = '\0';
            WideCharToMultiByte(CP_UTF8, 0, wexp, -1, a->expansion, (int)sizeof(a->expansion), NULL, NULL);
            a->expansion[sizeof(a->expansion) - 1] = '\0';
            aliasedit_refresh_list(list);
            return 0;
        }
        case ID_ALIASEDIT_DELETE: {
            int sel = (int)SendMessageW(list, LVM_GETNEXTITEM, (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
            if (sel < 0) {
                MessageBoxW(hwnd, L"Select an alias to delete first.", L"Aliases", MB_OK | MB_ICONWARNING);
                return 0;
            }
            for (int i = sel; i < s_alias_edit_count - 1; i++)
                s_alias_edit_buf[i] = s_alias_edit_buf[i + 1];
            s_alias_edit_count--;
            aliasedit_refresh_list(list);
            SetDlgItemTextW(hwnd, ID_ALIASEDIT_NAME_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_ALIASEDIT_EXPANSION_EDIT, L"");
            return 0;
        }
        case ID_ALIASEDIT_SAVE: {
            g_app.alias_count = s_alias_edit_count;
            for (int i = 0; i < s_alias_edit_count; i++)
                g_app.aliases[i] = s_alias_edit_buf[i];
            bool ok = save_aliases();
            EnableWindow(GetParent(g_app.hwnd_output), TRUE);
            DestroyWindow(hwnd);
            SetFocus(g_app.hwnd_input);
            char msg[80];
            int mlen;
            if (ok)
                mlen = snprintf(msg, sizeof(msg), "Saved %d alias(es) to aliases.txt.\r\n", g_app.alias_count);
            else
                mlen = snprintf(msg, sizeof(msg), "Failed to save aliases.txt!\r\n");
            append_output(msg, (size_t)mlen, ok ? 3 : 1, 0);
            g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
            g_app.pending_line_len = 0;
            return 0;
        }
        case ID_ALIASEDIT_CANCEL:
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
        g_app.hwnd_aliasedit = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void open_alias_editor(HWND parent) {
    if (g_app.hwnd_aliasedit) {
        SetForegroundWindow(g_app.hwnd_aliasedit);
        return;
    }
    load_aliases(); /* pick up any hand-edits made outside the client since it started */
    s_alias_edit_count = g_app.alias_count;
    for (int i = 0; i < s_alias_edit_count; i++)
        s_alias_edit_buf[i] = g_app.aliases[i];

    static bool cls_registered = false;
    if (!cls_registered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = AliasEditWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"TobinMUDAliasEditWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        cls_registered = true;
    }

    const int win_w = 600, win_h = 390;
    RECT pr;
    GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - win_w) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - win_h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TobinMUDAliasEditWindow", L"Edit Aliases",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, win_w, win_h, parent, NULL, GetModuleHandleW(NULL), NULL);
    g_app.hwnd_aliasedit = hwnd;

    HFONT dlgfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 16, 16, 552, 190,
        hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_LIST, NULL, NULL);
    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    aliasedit_setup_columns(list);
    aliasedit_refresh_list(list);

    HWND lbl1 = CreateWindowW(L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE,
        16, 216, 60, 20, hwnd, NULL, NULL, NULL);
    HWND e1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        80, 214, 120, 22, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_NAME_EDIT, NULL, NULL);
    HWND lbl2 = CreateWindowW(L"STATIC", L"Expansion:", WS_CHILD | WS_VISIBLE,
        216, 216, 70, 20, hwnd, NULL, NULL, NULL);
    HWND e2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        290, 214, 276, 22, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_EXPANSION_EDIT, NULL, NULL);

    HWND add = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE,
        16, 246, 90, 26, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_ADD, NULL, NULL);
    HWND upd = CreateWindowW(L"BUTTON", L"Update Selected", WS_CHILD | WS_VISIBLE,
        114, 246, 130, 26, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_UPDATE, NULL, NULL);
    HWND del = CreateWindowW(L"BUTTON", L"Delete Selected", WS_CHILD | WS_VISIBLE,
        252, 246, 130, 26, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_DELETE, NULL, NULL);

    HWND save = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        400, 288, 80, 28, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_SAVE, NULL, NULL);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        488, 288, 80, 28, hwnd, (HMENU)(INT_PTR)ID_ALIASEDIT_CANCEL, NULL, NULL);

    HWND ctrls[] = { lbl1, e1, lbl2, e2, add, upd, del, save, cancel };
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++)
        SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)dlgfont, TRUE);
    SendMessageW(list, WM_SETFONT, (WPARAM)dlgfont, TRUE);

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
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_EDIT_TRIGGERS, L"Edit &Triggers...");
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_RELOAD_TRIGGERS, L"&Reload Triggers");
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_EDIT_ALIASES, L"Edit A&liases...");
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_RELOAD_ALIASES, L"Reload &Aliases");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, ID_MENU_FILE_EXIT, L"E&xit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    /* Edit menu (client TODO, "enable cut/copy/paste in the client",
     * 2026-08-21): the input box (a plain Win32 Edit control) already
     * gets Cut/Copy/Paste for free from Windows, both via Ctrl+X/C/V
     * and its own native right-click menu -- neither is intercepted
     * anywhere in InputSubclassProc. The read-only scrollback
     * (RichEdit) is different: Ctrl+C already copies a selection (that
     * works read-only or not), but RichEdit has no built-in right-click
     * menu of its own, so OutputSubclassProc grows one (Copy/Select
     * All) below. This menu just makes both paths discoverable/
     * consistent from the menu bar too -- each item acts on whichever
     * control currently has keyboard focus. */
    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, ID_MENU_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING, ID_MENU_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, ID_MENU_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, ID_MENU_EDIT_SELECTALL, L"Select &All\tCtrl+A");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, ID_MENU_HELP_CHECK_UPDATE, L"Check for &Updates...");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hHelp, MF_STRING, ID_MENU_HELP_ABOUT, L"&About TobinMUD Client");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenuBar;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_app.dark_theme = system_prefers_dark_theme();
        if (g_app.dark_theme) {
            g_app.input_bg_brush = CreateSolidBrush(RGB(30, 30, 30));
            g_app.window_bg_brush = CreateSolidBrush(RGB(20, 20, 20));
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_app.window_bg_brush);
        }

        {
            INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES };
            InitCommonControlsEx(&icc);
        }
        /* HP/Mana/Move gauge strip -- one (label, progress bar) pair per
         * vital, laid out by WM_SIZE below. Labels start blank/empty bar
         * until the first Char.Vitals GMCP push arrives (telnet_on_gmcp()). */
        g_app.hwnd_gauge_label_hp = CreateWindowExW(0, L"STATIC", L"HP",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_LABEL_HP, NULL, NULL);
        g_app.hwnd_gauge_bar_hp = CreateWindowExW(0, PROGRESS_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_BAR_HP, NULL, NULL);
        SendMessageW(g_app.hwnd_gauge_bar_hp, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_app.hwnd_gauge_bar_hp, PBM_SETBARCOLOR, 0, (LPARAM)RGB(190, 40, 40));

        /* Starts hidden -- no character loaded yet, class unknown until
         * the first Char.Vitals GMCP push (telnet_on_gmcp() shows/hides
         * it from there based on maxmana). */
        g_app.hwnd_gauge_label_mana = CreateWindowExW(0, L"STATIC", L"Mana",
            WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_LABEL_MANA, NULL, NULL);
        g_app.hwnd_gauge_bar_mana = CreateWindowExW(0, PROGRESS_CLASSW, L"",
            WS_CHILD | PBS_SMOOTH, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_BAR_MANA, NULL, NULL);
        SendMessageW(g_app.hwnd_gauge_bar_mana, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_app.hwnd_gauge_bar_mana, PBM_SETBARCOLOR, 0, (LPARAM)RGB(40, 90, 200));

        g_app.hwnd_gauge_label_move = CreateWindowExW(0, L"STATIC", L"Move",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_LABEL_MOVE, NULL, NULL);
        g_app.hwnd_gauge_bar_move = CreateWindowExW(0, PROGRESS_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_GAUGE_BAR_MOVE, NULL, NULL);
        SendMessageW(g_app.hwnd_gauge_bar_move, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_app.hwnd_gauge_bar_move, PBM_SETBARCOLOR, 0, (LPARAM)RGB(40, 160, 60));

        /* Monospace font throughout (user, 2026-08-05: default felt
         * "scrunched" -- a proportional font on a MUD's column-aligned
         * output, plus the default Edit/RichEdit font size, is what
         * that was). Switched Consolas -> Lucida Console -> Bitstream
         * Vera Sans Mono -> DejaVu Sans Mono (user, 2026-08-06).
         * Bitstream Vera Sans Mono was requested by name, bundled and
         * loaded successfully (AddFontResourceExW below), but STILL
         * misaligned every box-drawing screen -- verified via fontTools
         * that VeraMono.ttf has only 256 glyphs total (basic Latin/
         * Latin-1), zero coverage of the U+2500 box-drawing block Tobin
         * uses throughout its menus/banners, forcing Windows to
         * silently substitute a different fallback font for just those
         * characters, which doesn't share Vera's exact monospace cell
         * width. DejaVu Sans Mono is Vera's actively-maintained,
         * visually-near-identical descendant with full Unicode
         * coverage (3300+ glyphs, confirmed real glyphs for
         * U+2550/2551/2554/2557/255A/255D via the same fontTools
         * check) -- same look, but the box art this game actually
         * relies on renders correctly. Actual size comes from
         * g_app.font_pt (Preferences, default 10pt) via apply_font(). */
        g_app.hwnd_output = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_OUTPUT, NULL, NULL);
        /* NOT TM_PLAINTEXT here (tried in v0.4.15, reverted in v0.4.21)
         * -- it fixed \r\n doubling but silently restricts the WHOLE
         * control to one uniform character format, which is why ANSI
         * colors stopped rendering (user, 2026-08-06: "color got lost
         * in the client display, toggling color on and off does
         * nothing" -- the toggle had no per-run format left to act on).
         * The real doubling fix now lives in scan_msp_and_forward()
         * instead, collapsing \r\n to a bare \r before this text ever
         * reaches the RichEdit control -- that doesn't touch character
         * formatting, so per-line color keeps working. */
        SendMessageW(g_app.hwnd_output, EM_SETBKGNDCOLOR, 0,
                     g_app.dark_theme ? RGB(10, 10, 10) : RGB(250, 250, 250));
        SendMessageW(g_app.hwnd_output, EM_SETEVENTMASK, 0, 0);
        g_app.output_orig_proc = (WNDPROC)SetWindowLongPtr(g_app.hwnd_output, GWLP_WNDPROC,
                                                             (LONG_PTR)OutputSubclassProc);
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
            cf.crTextColor = g_app.dark_theme ? RGB(200, 200, 200) : RGB(20, 20, 20);
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
        int input_h = 28; /* fits the DejaVu Sans Mono font set in WM_CREATE without clipping descenders */
        int col_w = w / 3;
        int label_h = 14, bar_h = 16, pad = 2;
        int gauge_y = h - input_h - GAUGE_H;
        MoveWindow(g_app.hwnd_gauge_label_hp, 0, gauge_y + pad, col_w - pad, label_h, TRUE);
        MoveWindow(g_app.hwnd_gauge_bar_hp, 0, gauge_y + pad + label_h, col_w - pad, bar_h, TRUE);
        MoveWindow(g_app.hwnd_gauge_label_mana, col_w, gauge_y + pad, col_w - pad, label_h, TRUE);
        MoveWindow(g_app.hwnd_gauge_bar_mana, col_w, gauge_y + pad + label_h, col_w - pad, bar_h, TRUE);
        MoveWindow(g_app.hwnd_gauge_label_move, col_w * 2, gauge_y + pad, w - col_w * 2 - pad, label_h, TRUE);
        MoveWindow(g_app.hwnd_gauge_bar_move, col_w * 2, gauge_y + pad + label_h, w - col_w * 2 - pad, bar_h, TRUE);
        MoveWindow(g_app.hwnd_output, 0, 0, w, gauge_y, TRUE);
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
    case WM_CTLCOLOREDIT:
        if (g_app.dark_theme && (HWND)lp == g_app.hwnd_input) {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(220, 220, 220));
            SetBkColor(hdc, RGB(30, 30, 30));
            return (LRESULT)g_app.input_bg_brush;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (g_app.dark_theme) {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(220, 220, 220));
            SetBkColor(hdc, RGB(20, 20, 20));
            return (LRESULT)g_app.window_bg_brush;
        }
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
    g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
    g_app.pending_line_len = 0;
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
        case ID_MENU_FILE_RELOAD_TRIGGERS: {
            load_triggers();
            char msg[64];
            int mlen = snprintf(msg, sizeof(msg), "Reloaded %d trigger(s) from triggers.txt.\r\n",
                                 g_app.trigger_count);
            append_output(msg, (size_t)mlen, 3, 0);
            g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
            g_app.pending_line_len = 0;
            return 0;
        }
        case ID_MENU_FILE_RELOAD_ALIASES: {
            load_aliases();
            char msg[64];
            int mlen = snprintf(msg, sizeof(msg), "Reloaded %d alias(es) from aliases.txt.\r\n",
                                 g_app.alias_count);
            append_output(msg, (size_t)mlen, 3, 0);
            g_app.line_start_offset = GetWindowTextLengthW(g_app.hwnd_output);
            g_app.pending_line_len = 0;
            return 0;
        }
        case ID_MENU_FILE_EDIT_TRIGGERS:
            open_trigger_editor(hwnd);
            return 0;
        case ID_MENU_FILE_EDIT_ALIASES:
            open_alias_editor(hwnd);
            return 0;
        case ID_MENU_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_MENU_HELP_CHECK_UPDATE:
            check_for_updates_interactive(hwnd);
            return 0;
        /* Edit menu -- acts on whichever of hwnd_input/hwnd_output
         * currently has keyboard focus. Sending WM_CUT/WM_PASTE to the
         * read-only output pane is a harmless no-op (RichEdit ignores
         * both under ES_READONLY); no extra guard needed. */
        case ID_MENU_EDIT_CUT:
            SendMessageW(GetFocus(), WM_CUT, 0, 0);
            return 0;
        case ID_MENU_EDIT_COPY:
            SendMessageW(GetFocus(), WM_COPY, 0, 0);
            return 0;
        case ID_MENU_EDIT_PASTE:
            SendMessageW(GetFocus(), WM_PASTE, 0, 0);
            return 0;
        case ID_MENU_EDIT_SELECTALL:
            SendMessageW(GetFocus(), EM_SETSEL, 0, -1);
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
        mciSendStringA("close tobinmusic", NULL, 0, NULL);
        if (g_app.sock != INVALID_SOCKET)
            closesocket(g_app.sock);
        telnet_client_destroy(g_app.telnet);
        ansi_client_destroy(g_app.ansi);
        if (g_app.font)
            DeleteObject(g_app.font);
        if (g_app.input_bg_brush)
            DeleteObject(g_app.input_bg_brush);
        if (g_app.window_bg_brush)
            DeleteObject(g_app.window_bg_brush);
        PostQuitMessage(0);
        return 0;
    }
    /* See the Prefs WndProc's matching comment above -- same
     * ANSI/Unicode DefWindowProc mismatch, same "title bar just says T"
     * symptom, same fix. */
    return DefWindowProcW(hwnd, msg, wp, lp);
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

#define UPDATE_SPLASH_CLASS L"TobinMUDUpdateSplash"
static HWND g_update_splash = NULL;

/* Drains any pending Windows messages without blocking, so the update
 * splash window (below) keeps repainting/responding instead of looking
 * "(Not Responding)" while check_and_apply_update()'s download+msiexec
 * wait runs on this same (only) thread -- see download_and_install_
 * update()'s own pump_messages() calls during that wait. */
static void pump_messages(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* "client wont open" was fixed 2026-08-05 (see download_and_install_
 * update()'s own comment), but the fix only made the update itself
 * finish reliably -- there was still NOTHING on screen for however
 * long that takes (up to 60s: MSI download + a waited-on msiexec
 * install), same problem from the outside as before ("client hasnt
 * launched", TODO.md 2026-08-21). This tiny always-on-top popup (one
 * centered label, no controls) is shown by check_and_apply_update()
 * the moment a version mismatch is confirmed -- BEFORE download_and_
 * install_update() starts -- and torn down right after, whether that
 * call succeeds or fails, so normal startup is never left looking like
 * a stalled launch either way. */
static HWND create_update_splash(void) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = UPDATE_SPLASH_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    int w = 340, h = 110;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, UPDATE_SPLASH_CLASS, L"TobinMUD Client",
        WS_POPUP | WS_CAPTION | WS_VISIBLE,
        (sw - w) / 2, (sh - h) / 2, w, h, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd)
        return NULL;
    HWND label = CreateWindowExW(0, L"STATIC",
        L"Updating TobinMUD Client...\nThis will only take a moment.",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 30, w - 20, 50, hwnd, NULL, wc.hInstance, NULL);
    if (label)
        SendMessageW(label, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    pump_messages(); /* make sure it actually paints before the blocking network call starts */
    return hwnd;
}

static bool fetch_remote_version(char *out, size_t outsize) {
    if (outsize)
        out[0] = '\0';
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
    InternetCloseHandle(hInternet);

    for (char *p = remote_version; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }
    if (remote_version[0] == '\0')
        return false;
    strncpy(out, remote_version, outsize - 1);
    out[outsize - 1] = '\0';
    return true;
}

/* Downloads UPDATE_MSI_URL to a temp file, installs it silently via
 * msiexec (waiting up to 60s), then launches the freshly-installed exe
 * from the per-user install path. Returns true only once the new exe
 * has actually launched -- the caller must then exit (so the new
 * process owns the window, and so msiexec could replace this locked
 * .exe in the first place). Any failure at any step returns false,
 * leaving the current install running and its window to be shown. */
static bool download_and_install_update(void) {
    HINTERNET hInternet = InternetOpenA("TobinMUDClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
        return false;
    DWORD timeout_ms = 3000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

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
        pump_messages(); /* keep the update splash responsive during the download too */
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

    /* Same plain WaitForSingleObject(pi.hProcess, 60000) as before, but
     * broken into a poll loop so pump_messages() runs between waits --
     * keeps the update splash (if shown) repainting/responding for the
     * whole up-to-60s install instead of freezing the moment this call
     * starts (create_update_splash()'s own comment). Harmless when no
     * splash exists (pump_messages() with no windows just returns). */
    DWORD wait_deadline = GetTickCount() + 60000;
    for (;;) {
        DWORD now = GetTickCount();
        DWORD remaining = (now >= wait_deadline) ? 0 : (wait_deadline - now);
        DWORD wr = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, remaining, QS_ALLINPUT);
        if (wr != WAIT_OBJECT_0 + 1) /* process signaled, or timed out -- either way, done waiting */
            break;
        pump_messages();
    }
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

/* Silent startup auto-update (user, 2026-08-05): compares the published
 * version.txt to CLIENT_VERSION and, if they differ, downloads+installs
 * the new MSI and launches it. Returns true only once the new exe is
 * launched -- WinMain then exits without showing its own window. Any
 * network/install failure returns false and normal startup continues.
 * See fetch_remote_version()/download_and_install_update() above, and
 * check_for_updates_interactive() below for the manual Help-menu path. */
static bool check_and_apply_update(void) {
    char remote_version[64] = { 0 };
    if (!fetch_remote_version(remote_version, sizeof(remote_version))) {
        debug_log("update: version check failed -- skipping update");
        return false;
    }
    {
        char cmpbuf[160];
        snprintf(cmpbuf, sizeof(cmpbuf), "update: remote_version=\"%s\" CLIENT_VERSION=\"%s\"", remote_version, CLIENT_VERSION);
        debug_log(cmpbuf);
    }
    if (strcmp(remote_version, CLIENT_VERSION) == 0) {
        debug_log("update: up to date -- skipping update");
        return false;
    }
    debug_log("update: versions differ -- proceeding to download+install");
    /* "client hasnt launched" notice (TODO.md, 2026-08-21): this whole
     * silent startup path runs before WinMain ever creates the real
     * window, so without this splash there is NOTHING on screen for
     * the download+install below (up to 60s) -- see create_update_
     * splash()'s own comment. Torn down whether the update succeeds or
     * fails; on success the freshly-installed exe owns the screen
     * next, on failure normal startup falls through to the real window. */
    g_update_splash = create_update_splash();
    bool launched = download_and_install_update();
    if (g_update_splash) {
        DestroyWindow(g_update_splash);
        g_update_splash = NULL;
        pump_messages(); /* let WM_DESTROY actually run before moving on */
    }
    return launched;
}

/* Manual "Help > Check for Updates..." (user request). Unlike the
 * silent startup path, this always reports back: an unreachable server,
 * an up-to-date install, or a found update the user is asked to confirm
 * before anything downloads. On a confirmed, successful update it hands
 * off to the freshly-installed exe and tears this window down. */
static void check_for_updates_interactive(HWND owner) {
    char remote_version[64] = { 0 };
    if (!fetch_remote_version(remote_version, sizeof(remote_version))) {
        MessageBoxW(owner,
            L"Could not reach the update server.\n\nPlease check your internet connection and try again.",
            L"Check for Updates", MB_OK | MB_ICONWARNING);
        return;
    }

    if (strcmp(remote_version, CLIENT_VERSION) == 0) {
        MessageBoxW(owner,
            L"You are running the latest version (" WIDEN(CLIENT_VERSION) L").",
            L"Check for Updates", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t wremote[64];
    MultiByteToWideChar(CP_ACP, 0, remote_version, -1, wremote, 64);
    wchar_t prompt[256];
    swprintf(prompt, 256,
        L"A new version is available.\n\nInstalled: %ls\nAvailable: %ls\n\nDownload and install it now?",
        WIDEN(CLIENT_VERSION), wremote);
    if (MessageBoxW(owner, prompt, L"Check for Updates", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    if (download_and_install_update()) {
        /* New exe launched -- tear this window down (its WM_DESTROY
         * cleanup runs and PostQuitMessage ends the loop) so the fresh
         * install takes over and this locked .exe is released. */
        DestroyWindow(owner);
    } else {
        MessageBoxW(owner,
            L"The update could not be installed.\n\nYou can try again later, or reinstall from tobinmud.com.",
            L"Check for Updates", MB_OK | MB_ICONERROR);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
    (void)hPrev; (void)cmdline;

    debug_log("WinMain: start, CLIENT_VERSION=" CLIENT_VERSION);

    resolve_exe_dir();
    load_prefs();
    load_triggers();
    load_aliases();

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

    /* DejaVu Sans Mono (Vera Sans Mono's full-Unicode-coverage
     * descendant -- see apply_font()'s own comment for why) does NOT
     * ship with Windows, so it's bundled next to the exe
     * (fonts\DejaVuSansMono.ttf, same "resource folder beside the
     * binary" pattern as sounds\) and loaded privately here via
     * AddFontResourceExW(..., FR_PRIVATE, ...) -- visible only to this
     * process's own GDI calls (CreateFontW in apply_font()), no admin
     * rights, no per-user registry/relogin timing to worry about, and
     * guaranteed available on every single launch regardless of
     * install state. Must happen before the first apply_font() call. */
    {
        wchar_t wfontpath[MAX_PATH + 32];
        swprintf(wfontpath, MAX_PATH + 32, L"%hsfonts\\DejaVuSansMono.ttf", g_app.exe_dir);
        int added = AddFontResourceExW(wfontpath, FR_PRIVATE, 0);
        debug_log(added > 0 ? "WinMain: bundled font loaded (FR_PRIVATE)"
                             : "WinMain: bundled font FAILED to load -- falling back to whatever CreateFontW substitutes");
    }

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
    if (g_app.fullscreen) {
        g_app.fullscreen = false; /* apply_fullscreen(..., true) no-ops if already true */
        apply_fullscreen(hwnd, true);
    }
    debug_log("WinMain: entering message loop");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    WSACleanup();
    return 0;
}
