#include "kite.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Semantic analysis, run between parsing and codegen. Everything codegen
   assumes is proven here first:
     - every called function exists, with the right number of arguments
     - every variable is declared and in scope at its use
     - no duplicate functions, parameters, or same-scope declarations
     - a main function exists
   Scopes nest with blocks: a name lives from its declaration to the end
   of the enclosing block, and an inner declaration shadows an outer one. */

#define MAX_SCOPE_DEPTH 128
#define MAX_NAMES_PER_SCOPE 256

struct ctx {
    const struct program *prog;
    char *names[MAX_SCOPE_DEPTH][MAX_NAMES_PER_SCOPE];
    int count[MAX_SCOPE_DEPTH];
    int depth;
    char *err;
    int err_cap;
    int failed;
};

static void failf(struct ctx *c, int line, const char *fmt, ...) {
    if (c->failed) return;   /* report the first error only */
    c->failed = 1;
    int n = 0;
    if (line > 0) n = snprintf(c->err, c->err_cap, "line %d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->err + n, (size_t)(c->err_cap - n), fmt, ap);
    va_end(ap);
}

static const struct func *find_func(struct ctx *c, const char *name) {
    for (int i = 0; i < c->prog->nfuncs; i++)
        if (strcmp(c->prog->funcs[i]->name, name) == 0) return c->prog->funcs[i];
    return NULL;
}

static int in_current_scope(struct ctx *c, const char *name) {
    for (int i = 0; i < c->count[c->depth]; i++)
        if (strcmp(c->names[c->depth][i], name) == 0) return 1;
    return 0;
}

static int lookup(struct ctx *c, const char *name) {
    for (int d = c->depth; d >= 0; d--)
        for (int i = 0; i < c->count[d]; i++)
            if (strcmp(c->names[d][i], name) == 0) return 1;
    return 0;
}

static void declare(struct ctx *c, char *name) {
    c->names[c->depth][c->count[c->depth]++] = name;
}

static void check_expr(struct ctx *c, const struct expr *e) {
    if (c->failed) return;
    switch (e->kind) {
    case E_INT:
        return;
    case E_VAR:
        if (!lookup(c, e->name))
            failf(c, e->line, "undefined variable '%s'", e->name);
        return;
    case E_CALL: {
        const struct func *f = find_func(c, e->name);
        if (!f) {
            failf(c, e->line, "call to undefined function '%s'", e->name);
            return;
        }
        if (e->nargs != f->nparams) {
            failf(c, e->line, "'%s' takes %d argument(s), got %d", e->name, f->nparams, e->nargs);
            return;
        }
        for (int i = 0; i < e->nargs; i++) check_expr(c, e->args[i]);
        return;
    }
    case E_BINOP:
        check_expr(c, e->lhs);
        check_expr(c, e->rhs);
        return;
    }
}

static void push_scope(struct ctx *c) {
    if (c->depth + 1 >= MAX_SCOPE_DEPTH) return;
    c->depth++;
    c->count[c->depth] = 0;
}

static void pop_scope(struct ctx *c) {
    if (c->depth > 0) c->depth--;
}

static void check_body(struct ctx *c, struct stmt **body, int n) {
    push_scope(c);
    for (int i = 0; i < n && !c->failed; i++) {
        struct stmt *s = body[i];
        switch (s->kind) {
        case S_LET:
            check_expr(c, s->expr);          /* the initializer cannot see the new name */
            if (c->failed) break;
            if (in_current_scope(c, s->name)) {
                failf(c, s->line, "'%s' is already defined in this scope", s->name);
                break;
            }
            declare(c, s->name);
            break;
        case S_ASSIGN:
            if (!lookup(c, s->name)) {
                failf(c, s->line, "undefined variable '%s'", s->name);
                break;
            }
            check_expr(c, s->expr);
            break;
        case S_IF:
            check_expr(c, s->cond);
            check_body(c, s->then_body, s->nthen);
            check_body(c, s->else_body, s->nelse);
            break;
        case S_WHILE:
            check_expr(c, s->cond);
            check_body(c, s->body, s->nbody);
            break;
        case S_RETURN:
        case S_PRINT:
        case S_EXPR:
            if (s->expr) check_expr(c, s->expr);
            break;
        }
    }
    pop_scope(c);
}

int sema_check(const struct program *prog, char *err, int err_cap) {
    struct ctx c;
    memset(&c, 0, sizeof c);
    c.prog = prog;
    c.err = err;
    c.err_cap = err_cap;
    err[0] = '\0';

    for (int i = 0; i < prog->nfuncs; i++)
        for (int j = i + 1; j < prog->nfuncs; j++)
            if (strcmp(prog->funcs[i]->name, prog->funcs[j]->name) == 0) {
                failf(&c, 0, "function '%s' is defined more than once", prog->funcs[i]->name);
                return -1;
            }

    if (!find_func(&c, "main")) {
        failf(&c, 0, "no main function");
        return -1;
    }

    for (int i = 0; i < prog->nfuncs && !c.failed; i++) {
        struct func *f = prog->funcs[i];
        for (int a = 0; a < f->nparams; a++)
            for (int b = a + 1; b < f->nparams; b++)
                if (strcmp(f->params[a], f->params[b]) == 0) {
                    failf(&c, 0, "function '%s' has duplicate parameter '%s'", f->name, f->params[a]);
                    return -1;
                }
        c.depth = 0;
        c.count[0] = 0;
        for (int a = 0; a < f->nparams; a++) declare(&c, f->params[a]);
        /* the body walk pushes its own scope; params live one level out */
        check_body(&c, f->body, f->nbody);
    }
    return c.failed ? -1 : 0;
}
