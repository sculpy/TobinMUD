/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trigger_script.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "being.h"

/* ---- line splitting ---------------------------------------------------- */

int trig_script_split(const char *script_text, char *buf, size_t bufsz,
                      char *lines_out[TRIG_LINES_MAX]) {
    snprintf(buf, bufsz, "%s", script_text ? script_text : "");
    int n = 0;
    char *cursor = buf;
    while (*cursor && n < TRIG_LINES_MAX) {
        lines_out[n++] = cursor;
        char *nl = strchr(cursor, '\n');
        if (!nl)
            break;
        *nl = '\0';
        cursor = nl + 1;
    }
    return n;
}

/* Splits the leading whitespace-delimited word off `line` into `verb`
 * (truncated to verbsz) and returns a pointer to the rest of the line past
 * it, with any leading spaces skipped. The basic tokenizer every line of
 * script text goes through first, since every line starts with either a
 * control-flow keyword (if/while/set/...) or an action verb. */
static const char *first_token(const char *line, char *verb, size_t verbsz) {
    while (*line == ' ')
        line++;
    size_t i = 0;
    while (line[i] && line[i] != ' ' && i < verbsz - 1) {
        verb[i] = line[i];
        i++;
    }
    verb[i] = '\0';
    const char *rest = line + i;
    while (*rest == ' ')
        rest++;
    return rest;
}

/* ---- variables ----------------------------------------------------------
 * Global vars (`global <name> <value>`) persist to `trigger_global_var` via
 * trigger_global_get()/trigger_global_set() -- implemented in
 * trigger_var_repo.c (needs db.h, which this file otherwise has no reason
 * to pull in). */

static trig_var_t *find_local(trig_ctx_t *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++)
        if (strcasecmp(ctx->vars[i].name, name) == 0)
            return &ctx->vars[i];
    return NULL;
}

/* Sets a local (script-scoped) variable, creating it in ctx->vars if it
 * doesn't already exist. Silently drops the assignment if the scope is
 * already full (TRIG_VAR_MAX) -- same typo-tolerant, never-crash posture as
 * the rest of this interpreter. Backs the `set` command (do_set()). */
static void var_set(trig_ctx_t *ctx, const char *name, const char *value) {
    trig_var_t *v = find_local(ctx, name);
    if (!v) {
        if (ctx->var_count >= TRIG_VAR_MAX)
            return; /* scope full -- silently dropped, same typo-tolerant spirit */
        v = &ctx->vars[ctx->var_count++];
        snprintf(v->name, sizeof(v->name), "%s", name);
    }
    snprintf(v->value, sizeof(v->value), "%s", value);
}

/* Removes a local variable by name, if it exists, via swap-with-last so the
 * ctx->vars array stays dense. Backs the `unset` command (do_unset()). */
static void var_unset(trig_ctx_t *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcasecmp(ctx->vars[i].name, name) == 0) {
            ctx->vars[i] = ctx->vars[ctx->var_count - 1];
            ctx->var_count--;
            return;
        }
    }
}

/* Resolves one %token% (already stripped of its surrounding percents) to a
 * value string. Reserved names first (self/actor/arg/time/random.N), then
 * locals (`set`), then persisted globals (`global`); unknown -> "" (DG
 * itself resolves an unset variable to empty too). */
static void resolve_var(trig_ctx_t *ctx, const char *token, char *out, size_t outsz) {
    out[0] = '\0';
    if (strcasecmp(token, "self") == 0) {
        snprintf(out, outsz, "%s", ctx->self_name ? ctx->self_name : "");
        return;
    }
    if (strcasecmp(token, "actor") == 0) {
        snprintf(out, outsz, "%s", ctx->actor ? being_display_name(ctx->actor) : "");
        return;
    }
    if (strcasecmp(token, "arg") == 0) {
        snprintf(out, outsz, "%s", ctx->arg ? ctx->arg : "");
        return;
    }
    if (strcasecmp(token, "time") == 0) {
        snprintf(out, outsz, "%ld", (long)time(NULL));
        return;
    }
    if (strncasecmp(token, "random.", 7) == 0) {
        int n = atoi(token + 7);
        if (n < 1)
            n = 1;
        snprintf(out, outsz, "%d", (rand() % n) + 1);
        return;
    }
    trig_var_t *v = find_local(ctx, token);
    if (v) {
        snprintf(out, outsz, "%s", v->value);
        return;
    }
    trigger_global_get(token, out, outsz);
}

/* Substitutes every %...% span in `line` in one pass. A lone unmatched '%'
 * (no closing partner before end of line) is copied through literally --
 * typo-tolerant, matching the rest of this language's error handling. */
static void subst(trig_ctx_t *ctx, const char *line, char *out, size_t outsz) {
    size_t oi = 0;
    const char *p = line;
    while (*p && oi + 1 < outsz) {
        if (*p == '%') {
            const char *close = strchr(p + 1, '%');
            if (close) {
                char token[TRIG_VAR_NAME_LEN + 16];
                size_t tlen = (size_t)(close - (p + 1));
                if (tlen >= sizeof(token))
                    tlen = sizeof(token) - 1;
                memcpy(token, p + 1, tlen);
                token[tlen] = '\0';
                char val[TRIG_VAR_VALUE_LEN];
                resolve_var(ctx, token, val, sizeof(val));
                size_t vlen = strlen(val);
                if (vlen > outsz - oi - 1)
                    vlen = outsz - oi - 1;
                memcpy(out + oi, val, vlen);
                oi += vlen;
                p = close + 1;
                continue;
            }
        }
        out[oi++] = *p++;
    }
    out[oi] = '\0';
}

/* ---- expression evaluation ----------------------------------------------
 * Boolean: `a && b`, `a || b` (|| lowest precedence, left-to-right, no
 * parens -- DG's own real-world scripts are almost always this flat too),
 * each atom optionally `!`-negated, comparisons ==/!=/<=/>=/</> (numeric if
 * both sides parse as a full number, else string comparison), bare atom is
 * truthy if non-empty and not the literal "0". */

static void trim(char *s) {
    while (*s == ' ')
        memmove(s, s + 1, strlen(s));
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ' ')
        s[--n] = '\0';
}

/* Evaluates a single boolean atom: strips surrounding whitespace and any
 * leading `!` negations, then checks for a comparison operator (==, !=,
 * <=, >=, <, >). If both sides parse fully as numbers the comparison is
 * numeric; otherwise it falls back to a plain string comparison. An atom
 * with no operator is truthy iff it's non-empty and not the literal "0" --
 * the base case eval_bool()'s &&/|| splitting bottoms out to. */
static bool eval_atom(const char *atom_in) {
    char atom[256];
    snprintf(atom, sizeof(atom), "%s", atom_in);
    trim(atom);
    bool neg = false;
    while (atom[0] == '!') {
        neg = !neg;
        memmove(atom, atom + 1, strlen(atom));
        trim(atom);
    }

    static const char *ops[] = {"==", "!=", "<=", ">=", "<", ">"};
    bool result;
    const char *found_op = NULL;
    size_t op_i = 0;
    for (op_i = 0; op_i < sizeof(ops) / sizeof(ops[0]); op_i++) {
        char *hit = strstr(atom, ops[op_i]);
        if (hit) {
            found_op = ops[op_i];
            char lhs[256], rhs[256];
            size_t llen = (size_t)(hit - atom);
            if (llen >= sizeof(lhs))
                llen = sizeof(lhs) - 1;
            memcpy(lhs, atom, llen);
            lhs[llen] = '\0';
            snprintf(rhs, sizeof(rhs), "%s", hit + strlen(found_op));
            trim(lhs);
            trim(rhs);

            char *lend, *rend;
            double lnum = strtod(lhs, &lend);
            double rnum = strtod(rhs, &rend);
            bool both_numeric = (lend != lhs && *lend == '\0') && (rend != rhs && *rend == '\0');

            int cmp = both_numeric ? (lnum < rnum ? -1 : (lnum > rnum ? 1 : 0)) : strcmp(lhs, rhs);
            if (strcmp(found_op, "==") == 0)
                result = (cmp == 0);
            else if (strcmp(found_op, "!=") == 0)
                result = (cmp != 0);
            else if (strcmp(found_op, "<=") == 0)
                result = (cmp <= 0);
            else if (strcmp(found_op, ">=") == 0)
                result = (cmp >= 0);
            else if (strcmp(found_op, "<") == 0)
                result = (cmp < 0);
            else
                result = (cmp > 0);
            return neg ? !result : result;
        }
    }
    result = atom[0] != '\0' && strcmp(atom, "0") != 0;
    return neg ? !result : result;
}

/* Evaluates a full boolean expression for `if`/`while`/`elseif`: splits on
 * `||` (lowest precedence) into OR-segments, each of which is split on
 * `&&` and requires every side to be true, then eval_atom()'s each leaf.
 * No parentheses/precedence beyond that -- deliberately flat, matching how
 * real-world DG scripts are almost always written. */
static bool eval_bool(const char *expr) {
    /* || first (lowest precedence): any true side wins. Split on the
     * literal two-char "||"/"&&" tokens (not strtok on a single '|'/'&',
     * which would also split mid-operator). */
    bool any_or = false;
    const char *or_cursor = expr;
    char or_seg[512];
    while (1) {
        const char *hit = strstr(or_cursor, "||");
        size_t seglen = hit ? (size_t)(hit - or_cursor) : strlen(or_cursor);
        if (seglen >= sizeof(or_seg))
            seglen = sizeof(or_seg) - 1;
        memcpy(or_seg, or_cursor, seglen);
        or_seg[seglen] = '\0';

        /* && within this OR-segment: all sides must be true. */
        bool all_and = true;
        const char *and_cursor = or_seg;
        char and_seg[512];
        while (1) {
            const char *ahit = strstr(and_cursor, "&&");
            size_t alen = ahit ? (size_t)(ahit - and_cursor) : strlen(and_cursor);
            if (alen >= sizeof(and_seg))
                alen = sizeof(and_seg) - 1;
            memcpy(and_seg, and_cursor, alen);
            and_seg[alen] = '\0';
            if (!eval_atom(and_seg))
                all_and = false;
            if (!ahit)
                break;
            and_cursor = ahit + 2;
        }
        if (all_and)
            any_or = true;

        if (!hit)
            break;
        or_cursor = hit + 2;
    }
    return any_or;
}

/* Evaluates a simple numeric expression for `eval`: parses a leading
 * integer, then repeatedly consumes a single-char operator (+ - * / %) and
 * the next integer, applying strictly left-to-right with no operator
 * precedence. Division/modulo by zero yields 0 rather than crashing. */
static long eval_num(const char *expr) {
    /* Left-to-right, no precedence -- "a op b op c" evaluates strictly
     * left-to-right, same simplification DG's own eval effectively behaves
     * like for the vast majority of real scripts (single binary op). */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", expr);
    trim(buf);
    char *p = buf;
    char *end;
    long acc = strtol(p, &end, 10);
    p = end;
    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        char op = *p++;
        while (*p == ' ')
            p++;
        long rhs = strtol(p, &end, 10);
        if (end == p)
            break;
        p = end;
        switch (op) {
            case '+': acc += rhs; break;
            case '-': acc -= rhs; break;
            case '*': acc *= rhs; break;
            case '/': acc = rhs != 0 ? acc / rhs : 0; break;
            case '%': acc = rhs != 0 ? acc % rhs : 0; break;
            default: break;
        }
    }
    return acc;
}

/* ---- block structure (static, computed on demand from the line array --
 * no runtime block stack is kept, so nothing extra needs to survive a
 * `wait` pause beyond the variable scope + the plain resume line index) -- */

static bool verb_is(const char *line, const char *want) {
    char v[16];
    first_token(line, v, sizeof(v));
    return strcasecmp(v, want) == 0;
}

/* From `start_pc` (an "if"/"elseif"/"else" line, or any line -- only lines
 * AFTER start_pc are inspected), finds the next depth-0 "end", tracking
 * nested "if"/"end" pairs. Used both to close out an if-block and to skip
 * past unreached elseif/else siblings once a taken branch's body finishes. */
static int find_if_end(char *const lines[], int nlines, int start_pc) {
    int depth = 0;
    for (int i = start_pc + 1; i < nlines; i++) {
        if (verb_is(lines[i], "if"))
            depth++;
        else if (verb_is(lines[i], "end")) {
            if (depth == 0)
                return i;
            depth--;
        }
    }
    return nlines;
}

/* Same idea for while/switch, whose shared closer is "done". */
static int find_done(char *const lines[], int nlines, int start_pc) {
    int depth = 0;
    for (int i = start_pc + 1; i < nlines; i++) {
        if (verb_is(lines[i], "while") || verb_is(lines[i], "switch"))
            depth++;
        else if (verb_is(lines[i], "done")) {
            if (depth == 0)
                return i;
            depth--;
        }
    }
    return nlines;
}

#define MAX_IF_BRANCHES 8
static void find_if_branches(char *const lines[], int if_pc, int end_pc,
                             int branch_pcs[MAX_IF_BRANCHES], int *branch_count) {
    int depth = 0;
    *branch_count = 0;
    for (int i = if_pc + 1; i < end_pc; i++) {
        if (verb_is(lines[i], "if"))
            depth++;
        else if (verb_is(lines[i], "end"))
            depth--;
        else if (depth == 0 && (verb_is(lines[i], "elseif") || verb_is(lines[i], "else"))) {
            if (*branch_count < MAX_IF_BRANCHES)
                branch_pcs[(*branch_count)++] = i;
        }
    }
}

/* Backward bracket match for `done`/`break`: scans back from `before_pc`
 * (exclusive) for the nearest while/switch not already closed by a `done`
 * seen along the way. Used both to resolve which loop a `done` belongs to
 * (loop back vs. just exit) and which loop/switch a `break` should exit. */
static int find_enclosing_open(char *const lines[], int before_pc) {
    int depth = 0;
    for (int i = before_pc - 1; i >= 0; i--) {
        if (verb_is(lines[i], "done"))
            depth++;
        else if (verb_is(lines[i], "while") || verb_is(lines[i], "switch")) {
            if (depth == 0)
                return i;
            depth--;
        }
    }
    return -1;
}

/* ---- execution ----------------------------------------------------------
 */

static void do_set(trig_ctx_t *ctx, const char *rawarg) {
    char name[TRIG_VAR_NAME_LEN];
    const char *rest = first_token(rawarg, name, sizeof(name));
    if (!name[0])
        return;
    char value[TRIG_VAR_VALUE_LEN];
    subst(ctx, rest, value, sizeof(value));
    var_set(ctx, name, value);
}

/* Implements the `unset <name>` script command: removes a local variable. */
static void do_unset(trig_ctx_t *ctx, const char *rawarg) {
    char name[TRIG_VAR_NAME_LEN];
    first_token(rawarg, name, sizeof(name));
    if (name[0])
        var_unset(ctx, name);
}

/* Implements the `eval <name> <expr>` script command: %-substitutes and
 * numerically evaluates `expr` via eval_num(), then stores the result
 * (formatted as a decimal string) into local variable `name`. */
static void do_eval(trig_ctx_t *ctx, const char *rawarg) {
    char name[TRIG_VAR_NAME_LEN];
    const char *rest = first_token(rawarg, name, sizeof(name));
    if (!name[0])
        return;
    char expr[256];
    subst(ctx, rest, expr, sizeof(expr));
    long result = eval_num(expr);
    char value[TRIG_VAR_VALUE_LEN];
    snprintf(value, sizeof(value), "%ld", result);
    var_set(ctx, name, value);
}

/* Implements the `global <name> <value>` script command: %-substitutes the
 * value and persists it via trigger_global_set() (trigger_var_repo.c),
 * unlike `set`'s ctx-local, non-persisted variables. */
static void do_global(trig_ctx_t *ctx, const char *rawarg) {
    char name[TRIG_VAR_NAME_LEN];
    const char *rest = first_token(rawarg, name, sizeof(name));
    if (!name[0])
        return;
    char value[TRIG_VAR_VALUE_LEN];
    subst(ctx, rest, value, sizeof(value));
    trigger_global_set(name, value);
}

trig_exec_result_t trig_script_exec(trig_ctx_t *ctx, char *const lines[], int nlines,
                                    int start_pc, int *out_resume_pc, int *out_wait_secs) {
    int pc = start_pc;
    while (pc < nlines) {
        char verb[16];
        const char *rawarg = first_token(lines[pc], verb, sizeof(verb));

        if (strcasecmp(verb, "if") == 0) {
            char expr[256];
            subst(ctx, rawarg, expr, sizeof(expr));
            int end_pc = find_if_end(lines, nlines, pc);
            if (eval_bool(expr)) {
                pc++;
                continue;
            }
            int branches[MAX_IF_BRANCHES], bc;
            find_if_branches(lines, pc, end_pc, branches, &bc);
            bool taken = false;
            for (int i = 0; i < bc; i++) {
                char bverb[16];
                const char *brest = first_token(lines[branches[i]], bverb, sizeof(bverb));
                if (strcasecmp(bverb, "else") == 0) {
                    pc = branches[i] + 1;
                    taken = true;
                    break;
                }
                char bexpr[256];
                subst(ctx, brest, bexpr, sizeof(bexpr));
                if (eval_bool(bexpr)) {
                    pc = branches[i] + 1;
                    taken = true;
                    break;
                }
            }
            if (!taken)
                pc = end_pc + 1;
            continue;
        }
        if (strcasecmp(verb, "elseif") == 0 || strcasecmp(verb, "else") == 0) {
            /* Reached by falling through a taken branch's body -- skip the
             * remaining siblings and the closing `end`. */
            pc = find_if_end(lines, nlines, pc) + 1;
            continue;
        }
        if (strcasecmp(verb, "end") == 0) {
            pc++;
            continue;
        }
        if (strcasecmp(verb, "while") == 0) {
            char expr[256];
            subst(ctx, rawarg, expr, sizeof(expr));
            if (eval_bool(expr)) {
                pc++;
            } else {
                pc = find_done(lines, nlines, pc) + 1;
            }
            continue;
        }
        if (strcasecmp(verb, "switch") == 0) {
            char val[256];
            subst(ctx, rawarg, val, sizeof(val));
            trim(val);
            int done_pc = find_done(lines, nlines, pc);
            int match_pc = -1, default_pc = -1;
            int depth = 0;
            for (int i = pc + 1; i < done_pc; i++) {
                if (verb_is(lines[i], "while") || verb_is(lines[i], "switch"))
                    depth++;
                else if (verb_is(lines[i], "done"))
                    depth--;
                else if (depth == 0 && verb_is(lines[i], "case")) {
                    char cverb[16];
                    const char *crest = first_token(lines[i], cverb, sizeof(cverb));
                    char cval[256];
                    subst(ctx, crest, cval, sizeof(cval));
                    trim(cval);
                    if (match_pc < 0 && strcmp(cval, val) == 0)
                        match_pc = i;
                } else if (depth == 0 && verb_is(lines[i], "default")) {
                    default_pc = i;
                }
            }
            if (match_pc >= 0)
                pc = match_pc + 1;
            else if (default_pc >= 0)
                pc = default_pc + 1;
            else
                pc = done_pc + 1;
            continue;
        }
        if (strcasecmp(verb, "done") == 0) {
            int open_pc = find_enclosing_open(lines, pc);
            if (open_pc >= 0 && verb_is(lines[open_pc], "while"))
                pc = open_pc; /* loop back to re-check the condition */
            else
                pc++; /* switch's `done`, or an orphaned one -- just exit */
            continue;
        }
        if (strcasecmp(verb, "case") == 0 || strcasecmp(verb, "default") == 0) {
            /* Reached by falling through a preceding case's body without a
             * `break` -- real DG fallthrough: just keep going. */
            pc++;
            continue;
        }
        if (strcasecmp(verb, "break") == 0) {
            int open_pc = find_enclosing_open(lines, pc);
            if (open_pc >= 0) {
                pc = find_done(lines, nlines, open_pc) + 1;
            } else {
                pc++; /* not inside anything -- no-op, typo-tolerant */
            }
            continue;
        }
        if (strcasecmp(verb, "set") == 0) {
            do_set(ctx, rawarg);
            pc++;
            continue;
        }
        if (strcasecmp(verb, "unset") == 0) {
            do_unset(ctx, rawarg);
            pc++;
            continue;
        }
        if (strcasecmp(verb, "eval") == 0) {
            do_eval(ctx, rawarg);
            pc++;
            continue;
        }
        if (strcasecmp(verb, "global") == 0) {
            do_global(ctx, rawarg);
            pc++;
            continue;
        }
        if (strcasecmp(verb, "wait") == 0) {
            if (pc + 1 < nlines) {
                char expr[64];
                subst(ctx, rawarg, expr, sizeof(expr));
                int secs = atoi(expr);
                if (secs < 1)
                    secs = 1;
                if (secs > 3600)
                    secs = 3600;
                *out_resume_pc = pc + 1;
                *out_wait_secs = secs;
                return TRIG_EXEC_WAIT;
            }
            return TRIG_EXEC_DONE;
        }

        /* Fixed action vocabulary (echo/echoroom/emote/say/teleport/give/
         * damage/log) -- substitute the argument, then hand off to
         * trigger.c, which owns the actual side effects. */
        char arg[256];
        subst(ctx, rawarg, arg, sizeof(arg));
        trigger_dispatch_action(ctx, verb, arg);
        pc++;
    }
    return TRIG_EXEC_DONE;
}
