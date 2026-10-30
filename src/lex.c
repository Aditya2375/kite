#include "kite.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct token tok(enum tok_kind k, const char *text, size_t len, int line) {
    struct token t;
    t.kind = k;
    t.text = strndup(text, len);
    t.value = k == T_INT ? strtol(text, NULL, 10) : 0;
    t.line = line;
    return t;
}

static const struct { const char *word; enum tok_kind k; } keywords[] = {
    {"fn", T_FN}, {"let", T_LET}, {"if", T_IF}, {"else", T_ELSE},
    {"while", T_WHILE}, {"return", T_RETURN}, {"print", T_PRINT},
};

int lex_all(struct lexer *lx, struct token **out, int *count) {
    int cap = 64, n = 0;
    struct token *ts = malloc((size_t)cap * sizeof *ts);
    if (!ts) { perror("malloc"); exit(1); }
    lx->err[0] = '\0';
    lx->line = 1;
    lx->p = lx->src;

#define PUSH(t) do { \
    if (n >= cap) { cap *= 2; ts = realloc(ts, (size_t)cap * sizeof *ts); \
        if (!ts) { perror("realloc"); exit(1); } } \
    ts[n++] = (t); \
} while (0)

    while (*lx->p) {
        char c = *lx->p;
        if (c == '\n') { lx->line++; lx->p++; continue; }
        if (isspace((unsigned char)c)) { lx->p++; continue; }
        if (c == '#') { while (*lx->p && *lx->p != '\n') lx->p++; continue; }
        if (isalpha((unsigned char)c) || c == '_') {
            const char *s = lx->p;
            while (isalnum((unsigned char)*lx->p) || *lx->p == '_') lx->p++;
            size_t len = (size_t)(lx->p - s);
            enum tok_kind k = T_IDENT;
            for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; i++)
                if (strlen(keywords[i].word) == len && strncmp(s, keywords[i].word, len) == 0)
                    k = keywords[i].k;
            PUSH(tok(k, s, len, lx->line));
            continue;
        }
        if (isdigit((unsigned char)c)) {
            const char *s = lx->p;
            while (isdigit((unsigned char)*lx->p)) lx->p++;
            PUSH(tok(T_INT, s, (size_t)(lx->p - s), lx->line));
            continue;
        }
        lx->p++;
        switch (c) {
        case '(': PUSH(tok(T_LPAREN, "(", 1, lx->line)); break;
        case ')': PUSH(tok(T_RPAREN, ")", 1, lx->line)); break;
        case '{': PUSH(tok(T_LBRACE, "{", 1, lx->line)); break;
        case '}': PUSH(tok(T_RBRACE, "}", 1, lx->line)); break;
        case ';': PUSH(tok(T_SEMI, ";", 1, lx->line)); break;
        case ',': PUSH(tok(T_COMMA, ",", 1, lx->line)); break;
        case '+': PUSH(tok(T_PLUS, "+", 1, lx->line)); break;
        case '-': PUSH(tok(T_MINUS, "-", 1, lx->line)); break;
        case '*': PUSH(tok(T_STAR, "*", 1, lx->line)); break;
        case '/':
            PUSH(tok(T_SLASH, "/", 1, lx->line));
            break;
        case '%': PUSH(tok(T_PERCENT, "%", 1, lx->line)); break;
        case '=':
            if (*lx->p == '=') { lx->p++; PUSH(tok(T_EQ, "==", 2, lx->line)); }
            else PUSH(tok(T_ASSIGN, "=", 1, lx->line));
            break;
        case '!':
            if (*lx->p == '=') { lx->p++; PUSH(tok(T_NE, "!=", 2, lx->line)); }
            else goto bad;
            break;
        case '<':
            if (*lx->p == '=') { lx->p++; PUSH(tok(T_LE, "<=", 2, lx->line)); }
            else PUSH(tok(T_LT, "<", 1, lx->line));
            break;
        case '>':
            if (*lx->p == '=') { lx->p++; PUSH(tok(T_GE, ">=", 2, lx->line)); }
            else PUSH(tok(T_GT, ">", 1, lx->line));
            break;
        default:
        bad:
            snprintf(lx->err, sizeof lx->err, "line %d: unexpected character '%c'", lx->line, c);
            for (int i = 0; i < n; i++) free(ts[i].text);
            free(ts);
            return -1;
        }
    }
    PUSH(tok(T_EOF, "", 0, lx->line));
#undef PUSH
    *out = ts;
    *count = n;
    return 0;
}
