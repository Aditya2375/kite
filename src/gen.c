#include "kite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stack-machine codegen: every expression leaves its value on the stack.
   Locals and parameters live in fixed rbp-relative slots. */

struct local { char *name; int offset; };

struct gen {
    FILE *out;
    struct local locals[256];
    int nlocals;
    int marks[128];         /* per-block start index into locals[]; sema caps depth at 128 */
    int depth;
    int pushes;             /* live expression values on the machine stack */
    int stack_size;
    int label;
    char err[256];
};

static int find_local(struct gen *g, const char *name) {
    for (int i = g->nlocals - 1; i >= 0; i--)
        if (strcmp(g->locals[i].name, name) == 0) return g->locals[i].offset;
    return 0;
}

static int add_local(struct gen *g, const char *name) {
    if (g->nlocals >= 256) { snprintf(g->err, sizeof g->err, "too many locals"); return 0; }
    g->locals[g->nlocals].name = strdup(name);
    g->locals[g->nlocals].offset = -8 * ++g->stack_size;
    g->nlocals++;
    return g->locals[g->nlocals - 1].offset;
}

static void push(struct gen *g, const char *reg) { fprintf(g->out, "\tpush %%%s\n", reg); g->pushes++; }
static void pop(struct gen *g, const char *reg) { fprintf(g->out, "\tpop %%%s\n", reg); g->pushes--; }

/* The SysV ABI wants rsp 16-byte aligned at the call instruction. The body
   starts aligned; every live pushed value shifts it by 8, so an odd count
   needs an 8-byte pad around the call (e.g. a nested call argument). */
static void call_aligned(struct gen *g, const char *target) {
    int pad = g->pushes % 2;
    if (pad) fprintf(g->out, "\tsub $8, %%rsp\n");
    fprintf(g->out, "\tcall %s\n", target);
    if (pad) fprintf(g->out, "\tadd $8, %%rsp\n");
}

static int gen_expr(struct gen *g, const struct expr *e) {
    FILE *o = g->out;
    switch (e->kind) {
    case E_INT:
        fprintf(o, "\tmov $%ld, %%rax\n", e->value);
        push(g, "rax");
        return 0;
    case E_VAR: {
        int off = find_local(g, e->name);
        if (!off) { snprintf(g->err, sizeof g->err, "undefined variable '%s'", e->name); return -1; }
        fprintf(o, "\tmov %d(%%rbp), %%rax\n", off);
        push(g, "rax");
        return 0;
    }
    case E_CALL: {
        if (e->nargs > 6) { snprintf(g->err, sizeof g->err, "call '%s' has more than 6 arguments", e->name); return -1; }
        static const char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        for (int i = 0; i < e->nargs; i++)
            if (gen_expr(g, e->args[i]) != 0) return -1;
        for (int i = e->nargs - 1; i >= 0; i--) pop(g, argregs[i]);
        char target[256];
        snprintf(target, sizeof target, "kite_%s", e->name);
        call_aligned(g, target);
        push(g, "rax");
        return 0;
    }
    case E_BINOP: {
        if (gen_expr(g, e->lhs) != 0) return -1;
        if (gen_expr(g, e->rhs) != 0) return -1;
        pop(g, "rdi");     /* rhs */
        pop(g, "rax");     /* lhs */
        switch (e->op) {
        case T_PLUS:  fprintf(o, "\tadd %%rdi, %%rax\n"); break;
        case T_MINUS: fprintf(o, "\tsub %%rdi, %%rax\n"); break;
        case T_STAR:  fprintf(o, "\timul %%rdi, %%rax\n"); break;
        case T_SLASH:
        case T_PERCENT:
            fprintf(o, "\tcqo\n\tidiv %%rdi\n");
            if (e->op == T_PERCENT) fprintf(o, "\tmov %%rdx, %%rax\n");
            break;
        case T_EQ: case T_NE: case T_LT: case T_LE: case T_GT: case T_GE: {
            static const char *setcc[] = {[T_EQ] = "sete", [T_NE] = "setne", [T_LT] = "setl",
                                          [T_LE] = "setle", [T_GT] = "setg", [T_GE] = "setge"};
            fprintf(o, "\tcmp %%rdi, %%rax\n\t%s %%al\n\tmovzb %%al, %%rax\n", setcc[e->op]);
            break;
        }
        default:
            snprintf(g->err, sizeof g->err, "internal: bad operator");
            return -1;
        }
        push(g, "rax");
        return 0;
    }
    }
    return -1;
}

static int gen_stmt(struct gen *g, const struct stmt *s);

static int gen_block(struct gen *g, struct stmt **body, int n) {
    /* A block is a scope: names declared inside die at its end. */
    int mark = g->nlocals;
    if (g->depth >= 128) { snprintf(g->err, sizeof g->err, "blocks nested too deeply"); return -1; }
    g->marks[g->depth++] = mark;
    int rc = 0;
    for (int i = 0; i < n; i++)
        if (gen_stmt(g, body[i]) != 0) { rc = -1; break; }
    for (int i = mark; i < g->nlocals; i++) { free(g->locals[i].name); g->locals[i].name = NULL; }
    g->nlocals = mark;
    if (g->depth > 0) g->depth--;
    return rc;
}

/* current scope starts at the innermost block mark */
static int scope_start(struct gen *g) { return g->depth > 0 ? g->marks[g->depth - 1] : 0; }

static int gen_stmt(struct gen *g, const struct stmt *s) {
    FILE *o = g->out;
    switch (s->kind) {
    case S_LET: {
        for (int i = scope_start(g); i < g->nlocals; i++)
            if (strcmp(g->locals[i].name, s->name) == 0) {
                snprintf(g->err, sizeof g->err, "'%s' is already defined in this scope", s->name);
                return -1;
            }
        if (gen_expr(g, s->expr) != 0) return -1;
        int off = add_local(g, s->name);
        if (!off) return -1;
        pop(g, "rax");
        fprintf(o, "\tmov %%rax, %d(%%rbp)\n", off);
        return 0;
    }
    case S_ASSIGN: {
        int off = find_local(g, s->name);
        if (!off) { snprintf(g->err, sizeof g->err, "undefined variable '%s'", s->name); return -1; }
        if (gen_expr(g, s->expr) != 0) return -1;
        pop(g, "rax");
        fprintf(o, "\tmov %%rax, %d(%%rbp)\n", off);
        return 0;
    }
    case S_IF: {
        int lelse = ++g->label, lend = ++g->label;
        if (gen_expr(g, s->cond) != 0) return -1;
        pop(g, "rax");
        fprintf(o, "\ttest %%rax, %%rax\n\tje .L%d\n", lelse);
        if (gen_block(g, s->then_body, s->nthen) != 0) return -1;
        fprintf(o, "\tjmp .L%d\n.L%d:\n", lend, lelse);
        if (s->else_body && gen_block(g, s->else_body, s->nelse) != 0) return -1;
        fprintf(o, ".L%d:\n", lend);
        return 0;
    }
    case S_WHILE: {
        int ltop = ++g->label, lend = ++g->label;
        fprintf(o, ".L%d:\n", ltop);
        if (gen_expr(g, s->cond) != 0) return -1;
        pop(g, "rax");
        fprintf(o, "\ttest %%rax, %%rax\n\tje .L%d\n", lend);
        if (gen_block(g, s->body, s->nbody) != 0) return -1;
        fprintf(o, "\tjmp .L%d\n.L%d:\n", ltop, lend);
        return 0;
    }
    case S_RETURN:
        if (s->expr) {
            if (gen_expr(g, s->expr) != 0) return -1;
            pop(g, "rax");
        } else {
            fprintf(o, "\txor %%eax, %%eax\n");
        }
        fprintf(o, "\tleave\n\tret\n");
        return 0;
    case S_PRINT:
        if (gen_expr(g, s->expr) != 0) return -1;
        pop(g, "rdi");
        call_aligned(g, "kite_print");
        return 0;
    case S_EXPR:
        if (gen_expr(g, s->expr) != 0) return -1;
        pop(g, "rax");   /* discard */
        return 0;
    }
    return -1;
}

/* count let statements in a block, including nested blocks, to size the
   stack frame */
static int count_lets(struct stmt **body, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) {
        struct stmt *s = body[i];
        if (s->kind == S_LET) c++;
        c += count_lets(s->then_body, s->nthen);
        c += count_lets(s->else_body, s->nelse);
        c += count_lets(s->body, s->nbody);
    }
    return c;
}

static int gen_func(struct gen *g, const struct func *f) {
    FILE *o = g->out;
    g->nlocals = 0;
    g->stack_size = 0;
    g->depth = 0;
    g->pushes = 0;
    /* parameters become locals */
    static const char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    if (f->nparams > 6) { snprintf(g->err, sizeof g->err, "function '%s' has more than 6 parameters", f->name); return -1; }
    for (int i = 0; i < f->nparams; i++)
        if (!add_local(g, f->params[i])) return -1;

    /* first pass to size the stack frame: count all lets (incl. nested) */
    int extra = count_lets(f->body, f->nbody);

    fprintf(o, ".globl kite_%s\nkite_%s:\n", f->name, f->name);
    fprintf(o, "\tpush %%rbp\n\tmov %%rsp, %%rbp\n");
    int frame = 8 * (f->nparams + extra) + 64;  /* locals + expression scratch */
    frame = (frame + 15) & ~15;
    fprintf(o, "\tsub $%d, %%rsp\n", frame);
    for (int i = 0; i < f->nparams; i++)
        fprintf(o, "\tmov %%%s, %d(%%rbp)\n", argregs[i], g->locals[i].offset);
    int rc = gen_block(g, f->body, f->nbody);
    /* the block walk frees locals declared inside; parameters remain */
    for (int i = 0; i < g->nlocals; i++) free(g->locals[i].name);
    g->nlocals = 0;
    if (rc != 0) return -1;
    /* fall through: return 0 */
    fprintf(o, "\txor %%eax, %%eax\n\tleave\n\tret\n");
    return 0;
}

int gen_program(const struct program *prog, FILE *out, char *err, int err_cap) {
    struct gen g = {0};
    g.out = out;
    int has_main = 0;
    for (int i = 0; i < prog->nfuncs; i++)
        if (strcmp(prog->funcs[i]->name, "main") == 0) has_main = 1;
    if (!has_main) { snprintf(err, err_cap, "no main function"); return -1; }

    fprintf(out, ".text\n");
    /* runtime: kite_print(rdi) prints a signed decimal + newline via write(2) */
    fprintf(out,
        "kite_print:\n"
        "\tpush %%rbp\n\tmov %%rsp, %%rbp\n\tsub $48, %%rsp\n"
        "\tmov %%rdi, %%rax\n\tlea 47(%%rsp), %%rsi\n"
        "\tmovb $10, (%%rsi)\n\tmov $1, %%r8\n"       /* length, newline included */
        "\tmov $10, %%r9\n"
        "\ttest %%rax, %%rax\n\tjns 1f\n\tneg %%rax\n"
        "1:\txor %%edx, %%edx\n\tdiv %%r9\n"
        "\tadd $'0', %%dl\n\tdec %%rsi\n\tmov %%dl, (%%rsi)\n\tinc %%r8\n"
        "\ttest %%rax, %%rax\n\tjnz 1b\n"
        "\ttest %%rdi, %%rdi\n\tjns 2f\n\tdec %%rsi\n\tmovb $'-', (%%rsi)\n\tinc %%r8\n"
        "2:\tmov $1, %%eax\n\tmov $1, %%edi\n\tmov %%r8, %%rdx\n\tsyscall\n"
        "\tleave\n\tret\n");
    /* entry: call main, exit with its return value */
    fprintf(out,
        ".globl _start\n_start:\n"
        "\tcall kite_main\n"
        "\tmov %%rax, %%rdi\n\tmov $60, %%eax\n\tsyscall\n");

    for (int i = 0; i < prog->nfuncs; i++) {
        if (gen_func(&g, prog->funcs[i]) != 0) {
            snprintf(err, err_cap, "in %s: %s", prog->funcs[i]->name, g.err);
            return -1;
        }
    }
    fprintf(out, ".section .note.GNU-stack,\"\",@progbits\n");
    return 0;
}
