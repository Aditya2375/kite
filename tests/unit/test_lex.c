#include "../harness.h"
#include "kite.h"

#include <string.h>

static int kinds_of(const char *src, enum tok_kind *kinds, int cap) {
    struct lexer lx = {.src = src};
    struct token *ts;
    int n;
    if (lex_all(&lx, &ts, &n) != 0) return -1;
    int m = n < cap ? n : cap;
    for (int i = 0; i < m; i++) kinds[i] = ts[i].kind;
    return m;
}

int main(void) {
    enum tok_kind k[64];

    int n = kinds_of("fn main() { let x = 1 + 2; }", k, 64);
    enum tok_kind want[] = {T_FN, T_IDENT, T_LPAREN, T_RPAREN, T_LBRACE,
                            T_LET, T_IDENT, T_ASSIGN, T_INT, T_PLUS, T_INT,
                            T_SEMI, T_RBRACE, T_EOF};
    CHECK(n == 14, "token count");
    CHECK(memcmp(k, want, sizeof want) == 0, "token kinds in order");

    n = kinds_of("if a == b { } else { } while c != d { }", k, 64);
    CHECK(n > 0 && k[1] == T_IDENT && k[2] == T_EQ, "== is one token");
    CHECK(k[11] == T_NE, "!= is one token");

    n = kinds_of("x <= 1 >= 2 < 3 > 4", k, 64);
    CHECK(k[1] == T_LE && k[3] == T_GE && k[5] == T_LT && k[7] == T_GT, "comparison tokens");

    n = kinds_of("# a comment\nlet", k, 64);
    CHECK(n == 2 && k[0] == T_LET, "comments skipped");

    struct lexer lx = {.src = "let x = 99;"};
    struct token *ts;
    lex_all(&lx, &ts, &n);
    CHECK(ts[3].kind == T_INT && ts[3].value == 99, "integer value decoded");
    CHECK(strcmp(ts[1].text, "x") == 0, "identifier text kept");

    lx = (struct lexer){.src = "let @ x"};
    CHECK(lex_all(&lx, &ts, &n) != 0, "bad character is an error");
    CHECK(strstr(lx.err, "line 1") != NULL, "error names the line");

    lx = (struct lexer){.src = "a\nb\n@"};
    lex_all(&lx, &ts, &n);
    CHECK(strstr(lx.err, "line 3") != NULL, "line numbers tracked");

    TEST_RESULTS();
}
