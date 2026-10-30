#include "../harness.h"
#include "kite.h"

#include <string.h>

static struct program *parse_ok(const char *src) {
    struct lexer lx = {.src = src};
    struct token *ts;
    int n;
    if (lex_all(&lx, &ts, &n) != 0) return NULL;
    struct parser ps = {.toks = ts, .ntoks = n};
    return parse_program(&ps);
}

int main(void) {
    /* precedence: 1 + 2 * 3 is 1 + (2 * 3) */
    struct program *p = parse_ok("fn main() { return 1 + 2 * 3; }");
    CHECK(p != NULL, "parses");
    struct stmt *ret = p->funcs[0]->body[0];
    CHECK(ret->kind == S_RETURN, "return stmt");
    CHECK(ret->expr->kind == E_BINOP && ret->expr->op == T_PLUS, "plus on top");
    CHECK(ret->expr->rhs->kind == E_BINOP && ret->expr->rhs->op == T_STAR, "times nested right");
    program_free(p);

    /* comparisons looser than arithmetic: a + 1 < b * 2 */
    p = parse_ok("fn main() { return a + 1 < b * 2; }");
    CHECK(p->funcs[0]->body[0]->expr->op == T_LT, "comparison on top");
    program_free(p);

    /* equality loosest: x == y + 1 */
    p = parse_ok("fn main() { return x == y + 1; }");
    struct expr *e = p->funcs[0]->body[0]->expr;
    CHECK(e->op == T_EQ && e->rhs->op == T_PLUS, "== outermost");
    program_free(p);

    /* parens override: (1 + 2) * 3 */
    p = parse_ok("fn main() { return (1 + 2) * 3; }");
    e = p->funcs[0]->body[0]->expr;
    CHECK(e->op == T_STAR && e->lhs->op == T_PLUS, "parens override precedence");
    program_free(p);

    /* functions, params, calls */
    p = parse_ok("fn add(a, b) { return a + b; } fn main() { print(add(1, 2)); }");
    CHECK(p->nfuncs == 2, "two functions");
    CHECK(p->funcs[0]->nparams == 2, "two params");
    struct stmt *pr = p->funcs[1]->body[0];
    CHECK(pr->kind == S_PRINT && pr->expr->kind == E_CALL && pr->expr->nargs == 2, "call with 2 args");
    program_free(p);

    /* if/else and while shapes */
    p = parse_ok("fn main() { if x { let y = 1; } else { y = 2; } while x { print(x); } return 0; }");
    struct stmt *ifs = p->funcs[0]->body[0];
    CHECK(ifs->kind == S_IF && ifs->nthen == 1 && ifs->nelse == 1, "if/else bodies");
    CHECK(p->funcs[0]->body[1]->kind == S_WHILE, "while stmt");
    program_free(p);

    /* errors are reported with lines */
    struct lexer lx = {.src = "fn main() { let = 1; }"};
    struct token *ts;
    int n;
    lex_all(&lx, &ts, &n);
    struct parser ps = {.toks = ts, .ntoks = n};
    CHECK(parse_program(&ps) == NULL, "let without a name fails");
    CHECK(strstr(ps.err, "line 1") != NULL, "error names the line");

    lx = (struct lexer){.src = "fn main() { return 1 }"};
    lex_all(&lx, &ts, &n);
    ps = (struct parser){.toks = ts, .ntoks = n};
    CHECK(parse_program(&ps) == NULL, "missing semicolon fails");

    TEST_RESULTS();
}
