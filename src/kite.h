#ifndef KITE_H
#define KITE_H

/* kite - a small compiled language: lex -> parse -> x86-64 assembly. */

#include <stdio.h>

/* ---- tokens ---- */
enum tok_kind {
    T_EOF, T_IDENT, T_INT,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_RETURN, T_PRINT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_COMMA,
    T_ASSIGN,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
};

struct token {
    enum tok_kind kind;
    char *text;         /* identifier text or literal digits */
    long value;         /* T_INT */
    int line;
};

struct lexer {
    const char *src;
    const char *p;
    int line;
    char err[256];
};

int lex_all(struct lexer *lx, struct token **out, int *count);
void tokens_free(struct token *toks, int count);

/* ---- AST ---- */
enum expr_kind { E_INT, E_VAR, E_BINOP, E_CALL };

struct expr {
    enum expr_kind kind;
    int line;                   /* source line for diagnostics */
    long value;                 /* E_INT */
    char *name;                 /* E_VAR, E_CALL */
    enum tok_kind op;           /* E_BINOP */
    struct expr *lhs, *rhs;     /* E_BINOP */
    struct expr **args;         /* E_CALL */
    int nargs;
};

enum stmt_kind { S_LET, S_ASSIGN, S_IF, S_WHILE, S_RETURN, S_PRINT, S_EXPR };

struct stmt {
    enum stmt_kind kind;
    int line;                   /* source line for diagnostics */
    char *name;                 /* S_LET, S_ASSIGN */
    struct expr *expr;          /* S_LET, S_ASSIGN, S_RETURN, S_PRINT, S_EXPR */
    struct expr *cond;          /* S_IF, S_WHILE */
    struct stmt **then_body, **else_body;  /* S_IF */
    struct stmt **body;         /* S_WHILE */
    int nthen, nelse, nbody;
};

struct func {
    char *name;
    int line;                   /* source line of the definition */

    char **params;
    int nparams;
    struct stmt **body;
    int nbody;
};

struct program {
    struct func **funcs;
    int nfuncs;
};

struct parser {
    struct token *toks;
    int ntoks;
    int pos;
    char err[256];
};

struct program *parse_program(struct parser *ps);
void program_free(struct program *prog);

/* ---- semantic analysis ---- */
/* Resolve every name, check arity, duplicates, and scoping before codegen.
   Returns 0, or -1 with a line-numbered message in err. */
int sema_check(const struct program *prog, char *err, int err_cap);

/* ---- codegen ---- */
/* Emit x86-64 AT&T assembly for the program to out. Returns 0 or -1 with err. */
int gen_program(const struct program *prog, FILE *out, char *err, int err_cap);

#endif
