/* Portable proof harness for the TobinMUD client's trigger-match and
 * alias-expand logic. ci_contains() and expand_alias() are copied
 * VERBATIM from client/src/win32/main.c; the trigger firing decision is
 * the exact logic of trigger_process_line() with send() replaced by a
 * captured "sent" string. If these pass, the client's fire path is
 * sound and the "never fire" report is not a logic bug. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define TRIGGER_PATTERN_MAX 128
#define TRIGGER_ACTION_MAX 256
#define ALIAS_NAME_MAX 64
#define ALIAS_EXPANSION_MAX 256

typedef struct { char pattern[TRIGGER_PATTERN_MAX]; char action[TRIGGER_ACTION_MAX]; bool gag; } trigger_t;
typedef struct { char name[ALIAS_NAME_MAX]; char expansion[ALIAS_EXPANSION_MAX]; } alias_t;

static trigger_t triggers[16]; static int trigger_count;
static alias_t aliases[16]; static int alias_count;

/* --- VERBATIM from main.c --- */
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
static void expand_alias(const char *input, char *out, size_t outsize) {
    size_t i = 0;
    while (input[i] && input[i] != ' ') i++;
    size_t wordlen = i;
    const char *rest = input + i;
    while (*rest == ' ') rest++;
    for (int k = 0; k < alias_count; k++) {
        alias_t *a = &aliases[k];
        if (strlen(a->name) != wordlen) continue;
        if (strncasecmp(a->name, input, wordlen) != 0) continue;
        if (rest[0]) snprintf(out, outsize, "%s %s", a->expansion, rest);
        else snprintf(out, outsize, "%s", a->expansion);
        return;
    }
    snprintf(out, outsize, "%s", input);
}
/* trigger_process_line() decision, send() -> captured buffer */
static int fire_line(const char *line, char sent[8][256], bool *gag_out) {
    int nsent = 0; bool gag = false;
    for (int i = 0; i < trigger_count; i++) {
        trigger_t *t = &triggers[i];
        if (!ci_contains(line, t->pattern)) continue;
        if (t->gag) gag = true;
        if (t->action[0]) snprintf(sent[nsent++], 256, "%s", t->action);
    }
    *gag_out = gag; return nsent;
}

static int pass=0, fail=0;
static void ck(bool c, const char *m){ if(c){pass++;printf("  ok: %s\n",m);} else {fail++;printf("  FAIL: %s\n",m);} }

int main(void){
    /* two triggers, one gag */
    trigger_count=0;
    snprintf(triggers[trigger_count].pattern,TRIGGER_PATTERN_MAX,"%s","You are bleeding");
    snprintf(triggers[trigger_count].action,TRIGGER_ACTION_MAX,"%s","bandage self"); triggers[trigger_count].gag=false; trigger_count++;
    snprintf(triggers[trigger_count].pattern,TRIGGER_PATTERN_MAX,"%s","spammy weather"); triggers[trigger_count].action[0]='\0'; triggers[trigger_count].gag=true; trigger_count++;

    char sent[8][256]; bool gag;
    int n=fire_line("The guard says 'You are bleeding badly!'", sent, &gag);
    ck(n==1 && strcmp(sent[0],"bandage self")==0, "substring trigger fires its action mid-line");
    ck(gag==false, "non-gag trigger leaves line visible");

    n=fire_line("YOU ARE BLEEDING", sent, &gag);
    ck(n==1 && strcmp(sent[0],"bandage self")==0, "match is case-insensitive");

    n=fire_line("the spammy weather rolls in", sent, &gag);
    ck(n==0 && gag==true, "gag-only trigger (empty action) hides line, sends nothing");

    n=fire_line("nothing to see here", sent, &gag);
    ck(n==0 && gag==false, "non-matching line does nothing");

    /* aliases */
    alias_count=0;
    snprintf(aliases[alias_count].name,ALIAS_NAME_MAX,"%s","k");
    snprintf(aliases[alias_count].expansion,ALIAS_EXPANSION_MAX,"%s","kill"); alias_count++;
    snprintf(aliases[alias_count].name,ALIAS_NAME_MAX,"%s","gg");
    snprintf(aliases[alias_count].expansion,ALIAS_EXPANSION_MAX,"%s","get all from corpse"); alias_count++;

    char out[512];
    expand_alias("k rat", out, sizeof(out));
    ck(strcmp(out,"kill rat")==0, "alias expands first word, carries args (k rat -> kill rat)");
    expand_alias("gg", out, sizeof(out));
    ck(strcmp(out,"get all from corpse")==0, "bare alias expands with no trailing args");
    expand_alias("kill rat", out, sizeof(out));
    ck(strcmp(out,"kill rat")==0, "whole-word only: 'kill' is not expanded by alias 'k'");
    expand_alias("known spells", out, sizeof(out));
    ck(strcmp(out,"known spells")==0, "whole-word only: 'known' is not expanded by alias 'k'");
    expand_alias("look", out, sizeof(out));
    ck(strcmp(out,"look")==0, "no matching alias passes input through unchanged");

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail?1:0;
}
