/* Browser demo glue: run the real compiler front-to-back on source from
   the page and hand back the generated assembly (or the diagnostic).
   Assembling and linking stay native: they need a system assembler and
   target x86-64 Linux, which a browser sandbox cannot run. */
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/kite.h"

/* Compile src to x86-64 assembly. Returns the assembly text, or NULL and
   *err_out receives a static-ish diagnostic string the caller must free. */
EMSCRIPTEN_KEEPALIVE char *kite_compile(const char *src) {
    struct lexer lx = {.src = src};
    struct token *toks = NULL;
    int ntoks = 0;
    if (lex_all(&lx, &toks, &ntoks) != 0)
        return strdup(lx.err);

    struct parser ps = {.toks = toks, .ntoks = ntoks};
    struct program *prog = parse_program(&ps);
    tokens_free(toks, ntoks);
    if (!prog)
        return strdup(ps.err);

    char sema_err[256];
    if (sema_check(prog, sema_err, sizeof sema_err) != 0) {
        program_free(prog);
        return strdup(sema_err);
    }

    char *buf = NULL;
    size_t len = 0;
    FILE *out = open_memstream(&buf, &len);
    if (!out) { program_free(prog); return strdup("out of memory"); }
    char err[256];
    int rc = gen_program(prog, out, err, sizeof err);
    fclose(out);
    program_free(prog);
    if (rc != 0) { free(buf); return strdup(err); }
    return buf;   /* JS reads with UTF8ToString; freed via kite_free */
}

EMSCRIPTEN_KEEPALIVE void kite_free(char *p) { free(p); }
