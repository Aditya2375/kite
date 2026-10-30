#include "kite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct token *peek(struct parser *ps) { return &ps->toks[ps->pos]; }
static struct token *next(struct parser *ps) { return ps->toks[ps->pos].kind == T_EOF ? &ps->toks[ps->pos] : &ps->toks[ps->pos++]; }
static int accept(struct parser *ps, enum tok_kind k) {
    if (peek(ps)->kind == k) { next(ps); return 1; }
    return 0;
}
static int expect(struct parser *ps, enum tok_kind k, const char *what) {
    if (accept(ps, k)) return 0;
    snprintf(ps->err, sizeof ps->err, "line %d: expected %s", peek(ps)->line, what);
    return -1;
}

static void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { perror("malloc"); exit(1); } return p; }

/* append helper for statement lists */
#define LIST_APPEND(list, count, item) do { \
    (list) = realloc((list), (size_t)((count) + 1) * sizeof *(list)); \
    if (!(list)) { perror("realloc"); exit(1); } \
    (list)[(count)++] = (item); \
} while (0)

static struct expr *parse_expr(struct parser *ps);

static struct expr *expr_new(enum expr_kind k) {
    struct expr *e = xmalloc(sizeof *e);
    memset(e, 0, sizeof *e);
    e->kind = k;
    return e;
}

static struct expr *parse_primary(struct parser *ps) {
    struct token *t = peek(ps);
    if (t->kind == T_INT) {
        next(ps);
        struct expr *e = expr_new(E_INT);
        e->line = t->line;
        e->value = t->value;
        return e;
    }
    if (t->kind == T_IDENT) {
        next(ps);
        struct expr *e = expr_new(E_VAR);
        e->line = t->line;
        e->name = strdup(t->text);
        if (accept(ps, T_LPAREN)) {          /* call */
            e->kind = E_CALL;
            while (peek(ps)->kind != T_RPAREN) {
                LIST_APPEND(e->args, e->nargs, parse_expr(ps));
                if (!accept(ps, T_COMMA)) break;
            }
            if (expect(ps, T_RPAREN, "')' after arguments")) return NULL;
        }
        return e;
    }
    if (accept(ps, T_LPAREN)) {
        struct expr *e = parse_expr(ps);
        if (expect(ps, T_RPAREN, "')'")) return NULL;
        return e;
    }
    if (accept(ps, T_MINUS)) {               /* unary minus: 0 - x */
        struct expr *z = expr_new(E_INT);
        z->value = 0;
        struct expr *x = parse_primary(ps);
        struct expr *e = expr_new(E_BINOP);
        e->op = T_MINUS;
        e->lhs = z;
        e->rhs = x;
        return e;
    }
    snprintf(ps->err, sizeof ps->err, "line %d: expected an expression", t->line);
    return NULL;
}

/* binary levels, tightest first */
static struct expr *parse_level(struct parser *ps, int level) {
    /* outermost (loosest) first; operands come from the next level in */
    static const enum tok_kind ops[][4] = {
        {T_EQ, T_NE, T_EOF, T_EOF},
        {T_LT, T_LE, T_GT, T_GE},
        {T_PLUS, T_MINUS, T_EOF, T_EOF},
        {T_STAR, T_SLASH, T_PERCENT, T_EOF},
    };
    if (level >= 4) return parse_primary(ps);
    struct expr *lhs = parse_level(ps, level + 1);
    if (!lhs) return NULL;
    for (;;) {
        enum tok_kind k = peek(ps)->kind;
        int match = 0;
        for (int i = 0; i < 4 && ops[level][i] != T_EOF; i++)
            if (ops[level][i] == k) match = 1;
        if (!match) break;
        next(ps);
        struct expr *rhs = parse_level(ps, level + 1);
        if (!rhs) return NULL;
        struct expr *e = expr_new(E_BINOP);
        e->op = k;
        e->lhs = lhs;
        e->rhs = rhs;
        lhs = e;
    }
    return lhs;
}

static struct expr *parse_expr(struct parser *ps) { return parse_level(ps, 0); }

static struct stmt *stmt_new(enum stmt_kind k) {
    struct stmt *s = xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->kind = k;
    return s;
}

static struct stmt *parse_stmt(struct parser *ps);

static struct stmt **parse_block(struct parser *ps, int *count) {
    *count = 0;
    struct stmt **body = NULL;
    if (expect(ps, T_LBRACE, "'{'")) return NULL;
    while (peek(ps)->kind != T_RBRACE && peek(ps)->kind != T_EOF) {
        struct stmt *s = parse_stmt(ps);
        if (!s) { free(body); return NULL; }
        LIST_APPEND(body, *count, s);
    }
    if (expect(ps, T_RBRACE, "'}'")) { free(body); return NULL; }
    return body;
}

static struct stmt *parse_stmt(struct parser *ps) {
    struct token *t = peek(ps);
    int line = t->line;
    if (accept(ps, T_LET)) {
        struct stmt *s = stmt_new(S_LET);
        s->line = line;
        struct token *name = next(ps);
        if (name->kind != T_IDENT) { snprintf(ps->err, sizeof ps->err, "line %d: expected a name after 'let'", t->line); return NULL; }
        s->name = strdup(name->text);
        if (expect(ps, T_ASSIGN, "'=' in let")) return NULL;
        s->expr = parse_expr(ps);
        if (!s->expr || expect(ps, T_SEMI, "';'")) return NULL;
        return s;
    }
    if (accept(ps, T_IF)) {
        struct stmt *s = stmt_new(S_IF);
        s->line = line;
        s->cond = parse_expr(ps);
        if (!s->cond) return NULL;
        s->then_body = parse_block(ps, &s->nthen);
        if (!s->then_body) return NULL;
        if (accept(ps, T_ELSE)) {
            s->else_body = parse_block(ps, &s->nelse);
            if (!s->else_body) return NULL;
        }
        return s;
    }
    if (accept(ps, T_WHILE)) {
        struct stmt *s = stmt_new(S_WHILE);
        s->line = line;
        s->cond = parse_expr(ps);
        if (!s->cond) return NULL;
        s->body = parse_block(ps, &s->nbody);
        if (!s->body) return NULL;
        return s;
    }
    if (accept(ps, T_RETURN)) {
        struct stmt *s = stmt_new(S_RETURN);
        s->line = line;
        if (peek(ps)->kind != T_SEMI) {
            s->expr = parse_expr(ps);
            if (!s->expr) return NULL;
        }
        if (expect(ps, T_SEMI, "';'")) return NULL;
        return s;
    }
    if (accept(ps, T_PRINT)) {
        struct stmt *s = stmt_new(S_PRINT);
        s->line = line;
        if (expect(ps, T_LPAREN, "'(' after print")) return NULL;
        s->expr = parse_expr(ps);
        if (!s->expr) return NULL;
        if (expect(ps, T_RPAREN, "')'") || expect(ps, T_SEMI, "';'")) return NULL;
        return s;
    }
    if (t->kind == T_IDENT && ps->toks[ps->pos + 1].kind == T_ASSIGN) {
        next(ps);
        next(ps);
        struct stmt *s = stmt_new(S_ASSIGN);
        s->line = line;
        s->name = strdup(t->text);
        s->expr = parse_expr(ps);
        if (!s->expr || expect(ps, T_SEMI, "';'")) return NULL;
        return s;
    }
    /* expression statement */
    struct stmt *s = stmt_new(S_EXPR);
    s->line = line;
    s->expr = parse_expr(ps);
    if (!s->expr || expect(ps, T_SEMI, "';'")) return NULL;
    return s;
}

struct program *parse_program(struct parser *ps) {
    struct program *prog = xmalloc(sizeof *prog);
    prog->funcs = NULL;
    prog->nfuncs = 0;
    ps->err[0] = '\0';
    while (peek(ps)->kind != T_EOF) {
        if (expect(ps, T_FN, "'fn'")) goto fail;
        struct token *name = next(ps);
        if (name->kind != T_IDENT) {
            snprintf(ps->err, sizeof ps->err, "line %d: expected a function name", name->line);
            goto fail;
        }
        struct func *f = xmalloc(sizeof *f);
        memset(f, 0, sizeof *f);
        f->name = strdup(name->text);
        if (expect(ps, T_LPAREN, "'('")) goto fail;
        while (peek(ps)->kind != T_RPAREN) {
            struct token *p = next(ps);
            if (p->kind != T_IDENT) {
                snprintf(ps->err, sizeof ps->err, "line %d: expected a parameter name", p->line);
                goto fail;
            }
            f->params = realloc(f->params, (size_t)(f->nparams + 1) * sizeof *f->params);
            if (!f->params) { perror("realloc"); exit(1); }
            f->params[f->nparams++] = strdup(p->text);
            if (!accept(ps, T_COMMA)) break;
        }
        if (expect(ps, T_RPAREN, "')'")) goto fail;
        f->body = parse_block(ps, &f->nbody);
        if (!f->body) goto fail;
        prog->funcs = realloc(prog->funcs, (size_t)(prog->nfuncs + 1) * sizeof *prog->funcs);
        if (!prog->funcs) { perror("realloc"); exit(1); }
        prog->funcs[prog->nfuncs++] = f;
    }
    return prog;
fail:
    return NULL;
}

static void expr_free(struct expr *e) {
    if (!e) return;
    expr_free(e->lhs);
    expr_free(e->rhs);
    for (int i = 0; i < e->nargs; i++) expr_free(e->args[i]);
    free(e->args);
    free(e->name);
    free(e);
}

static void stmt_free(struct stmt *s) {
    if (!s) return;
    expr_free(s->expr);
    expr_free(s->cond);
    for (int i = 0; i < s->nthen; i++) stmt_free(s->then_body[i]);
    for (int i = 0; i < s->nelse; i++) stmt_free(s->else_body[i]);
    for (int i = 0; i < s->nbody; i++) stmt_free(s->body[i]);
    free(s->then_body);
    free(s->else_body);
    free(s->body);
    free(s->name);
    free(s);
}

void program_free(struct program *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->nfuncs; i++) {
        struct func *f = prog->funcs[i];
        for (int j = 0; j < f->nparams; j++) free(f->params[j]);
        free(f->params);
        for (int j = 0; j < f->nbody; j++) stmt_free(f->body[j]);
        free(f->body);
        free(f->name);
        free(f);
    }
    free(prog->funcs);
    free(prog);
}
