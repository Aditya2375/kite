#include "kite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

static void usage(FILE *f) {
    fprintf(f,
        "kite - a small compiled language\n"
        "\n"
        "usage: kite <program.kt> [-o output] [--asm-only]\n"
        "\n"
        "Compiles the program to a native x86-64 Linux executable via\n"
        "assembly. --asm-only writes the generated assembly to the output\n"
        "path instead of assembling and linking it.\n");
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { perror("malloc"); exit(1); }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(buf); return NULL; }
    fclose(f);
    buf[size] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *input = NULL, *output = "a.out";
    int asm_only = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--asm-only") == 0) asm_only = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(stdout); return 0; }
        else if (!input) input = argv[i];
        else { usage(stderr); return 2; }
    }
    if (!input) { usage(stderr); return 2; }

    char *src = read_file(input);
    if (!src) { fprintf(stderr, "kite: cannot read %s\n", input); return 1; }

    struct lexer lx = {.src = src};
    struct token *toks = NULL;
    int ntoks = 0;
    if (lex_all(&lx, &toks, &ntoks) != 0) {
        fprintf(stderr, "kite: %s: %s\n", input, lx.err);
        return 1;
    }

    struct parser ps = {.toks = toks, .ntoks = ntoks};
    struct program *prog = parse_program(&ps);
    if (!prog) {
        fprintf(stderr, "kite: %s: %s\n", input, ps.err);
        return 1;
    }

    char sema_err[256];
    if (sema_check(prog, sema_err, sizeof sema_err) != 0) {
        fprintf(stderr, "kite: %s: %s\n", input, sema_err);
        program_free(prog);
        return 1;
    }

    char asm_path[4096];
    if (asm_only) {
        snprintf(asm_path, sizeof asm_path, "%s", output);
    } else {
        snprintf(asm_path, sizeof asm_path, "/tmp/kite-%d.s", (int)getpid());
    }
    FILE *out = fopen(asm_path, "w");
    if (!out) { fprintf(stderr, "kite: cannot write %s\n", asm_path); return 1; }
    char err[256];
    if (gen_program(prog, out, err, sizeof err) != 0) {
        fclose(out);
        unlink(asm_path);
        fprintf(stderr, "kite: %s: %s\n", input, err);
        program_free(prog);
        return 1;
    }
    fclose(out);
    program_free(prog);

    if (!asm_only) {
        /* no shell: paths with quotes or spaces cannot inject anything */
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); unlink(asm_path); return 1; }
        if (pid == 0) {
            execlp("cc", "cc", "-nostdlib", "-static", "-o", output, asm_path, (char *)NULL);
            _exit(127);
        }
        int status = 0;
        while (waitpid(pid, &status, 0) < 0)
            if (errno != EINTR) { perror("waitpid"); unlink(asm_path); return 1; }
        unlink(asm_path);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "kite: assembler/linker failed\n");
            return 1;
        }
    }
    return 0;
}
