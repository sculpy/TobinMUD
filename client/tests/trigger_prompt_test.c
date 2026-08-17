/* Portable proof harness for the client's PROMPT-line trigger firing.
 * trigger_fire_matches / trigger_process_line / trigger_flush_prompt /
 * trigger_scan_feed are transcribed from client/src/win32/main.c with
 * send() replaced by a captured list and the RichEdit gag-delete stubbed
 * (display-only, irrelevant to firing). Proves: prompts fire, no
 * double-fire when the prompt later completes with a newline, a TCP-split
 * partial does not suppress the real fire, and a static prompt across
 * many drains fires once. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define TRIGGER_MAX 128
#define TRIGGER_PATTERN_MAX 128
#define TRIGGER_ACTION_MAX 256
#define TRIGGER_LINE_BUF 4096

typedef struct { char pattern[TRIGGER_PATTERN_MAX]; char action[TRIGGER_ACTION_MAX]; bool gag; } trigger_t;

static struct {
    trigger_t triggers[TRIGGER_MAX];
    int trigger_count;
    char pending_line_text[TRIGGER_LINE_BUF];
    size_t pending_line_len;
    unsigned char trigger_fired_this_line[TRIGGER_MAX];
} g_app;

static char sent[64][256];
static int sent_count;

static bool ci_contains(const char *hay, const char *needle) {
    if (!needle[0]) return false;
    size_t hlen = strlen(hay), nlen = strlen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
        }
        if (j == nlen) return true;
    }
    return false;
}
static void do_send(const char *action) { snprintf(sent[sent_count++], 256, "%s", action); }

/* --- transcribed from main.c --- */
static bool trigger_fire_matches(void) {
    g_app.pending_line_text[g_app.pending_line_len] = '\0';
    bool gag = false;
    for (int i = 0; i < g_app.trigger_count; i++) {
        trigger_t *t = &g_app.triggers[i];
        if (!ci_contains(g_app.pending_line_text, t->pattern)) continue;
        if (t->gag) gag = true;
        if (!g_app.trigger_fired_this_line[i] && t->action[0]) {
            do_send(t->action);
            g_app.trigger_fired_this_line[i] = 1;
        }
    }
    return gag;
}
static void trigger_process_line(void) {
    (void)trigger_fire_matches();
    g_app.pending_line_len = 0;
    memset(g_app.trigger_fired_this_line, 0, sizeof(g_app.trigger_fired_this_line));
}
static void trigger_flush_prompt(void) {
    if (g_app.pending_line_len == 0 || g_app.trigger_count == 0) return;
    trigger_fire_matches();
}
static void trigger_scan_feed(const char *text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') { trigger_process_line(); continue; }
        if (g_app.pending_line_len + 1 < sizeof(g_app.pending_line_text))
            g_app.pending_line_text[g_app.pending_line_len++] = text[i];
    }
}

static int pass, fail;
static void ck(bool c, const char *m){ if(c){pass++;printf("  ok: %s\n",m);} else {fail++;printf("  FAIL: %s\n",m);} }
static void reset_line(void){ g_app.pending_line_len=0; memset(g_app.trigger_fired_this_line,0,sizeof(g_app.trigger_fired_this_line)); sent_count=0; }
static void addtrig(const char*p,const char*a){ trigger_t*t=&g_app.triggers[g_app.trigger_count++]; snprintf(t->pattern,TRIGGER_PATTERN_MAX,"%s",p); snprintf(t->action,TRIGGER_ACTION_MAX,"%s",a); t->gag=false; }

int main(void){
    g_app.trigger_count=0;
    addtrig("HP:","quaff red");            /* a classic prompt trigger */
    addtrig("You are bleeding","bandage self");
    reset_line();

    /* 1. A prompt (no newline) fires its trigger when the socket drains. */
    trigger_scan_feed("HP:100 Mana:50 >", 16);
    trigger_flush_prompt();
    ck(sent_count==1 && strcmp(sent[0],"quaff red")==0, "prompt line (no newline) fires its trigger on drain");

    /* 2. The same prompt line then completes with a newline (leading \r\n
     *    of the next server output) -- action must NOT fire a second time. */
    trigger_scan_feed("\r\n", 2);
    ck(sent_count==1, "newline-completing the same prompt line does NOT re-fire the action");

    /* 3. A static prompt sitting across several drains fires only once. */
    reset_line();
    trigger_scan_feed("HP:88 >", 7);
    trigger_flush_prompt(); trigger_flush_prompt(); trigger_flush_prompt();
    ck(sent_count==1, "a static prompt across multiple drains fires once, not once per drain");

    /* 4. TCP split: a non-matching partial is flushed at a drain, then the
     *    rest arrives and the newline completes the match -- must fire. */
    reset_line();
    trigger_scan_feed("You are bl", 10);   /* partial, no match yet */
    trigger_flush_prompt();                 /* drain between TCP reads */
    ck(sent_count==0, "a non-matching partial line fires nothing at the drain");
    trigger_scan_feed("eeding badly!\r\n", 15);
    ck(sent_count==1 && strcmp(sent[0],"bandage self")==0,
       "the completed line still fires even though a partial was flushed first");

    /* 5. Regression: an ordinary newline-terminated line fires normally. */
    reset_line();
    trigger_scan_feed("You are bleeding all over.\r\n", 28);
    ck(sent_count==1 && strcmp(sent[0],"bandage self")==0, "a normal newline-terminated line still fires");

    /* 6. Two matches on one prompt line both fire, once each; drain again = no repeat. */
    reset_line();
    addtrig("HP:100","drink blue");   /* now HP:100 matches two triggers */
    trigger_scan_feed("HP:100 >", 8);
    trigger_flush_prompt();
    int hp = 0, blue = 0;
    for (int i=0;i<sent_count;i++){ if(!strcmp(sent[i],"quaff red"))hp++; if(!strcmp(sent[i],"drink blue"))blue++; }
    trigger_flush_prompt();  /* second drain, same content */
    int hp2=0,blue2=0;
    for (int i=0;i<sent_count;i++){ if(!strcmp(sent[i],"quaff red"))hp2++; if(!strcmp(sent[i],"drink blue"))blue2++; }
    ck(hp==1 && blue==1 && hp2==1 && blue2==1, "two triggers on one prompt each fire once, and a repeat drain adds nothing");

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail?1:0;
}
