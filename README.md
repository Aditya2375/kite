# kite

A small compiled programming language. kite programs compile to native
x86-64 Linux executables - the compiler lexes, parses, and type-checks
names, then emits real assembly, assembled and linked with the system C
toolchain. No runtime, no garbage collector, no dependencies: the output
binary is static and starts at `_start` with raw syscalls.

```
fn fib(n) {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let i = 0;
    while i < 10 {
        print(fib(i));
        i = i + 1;
    }
    return 0;
}
```

## Quick start

```bash
make
./build/kite examples/fib.kt -o fib
./fib
```

`kite program.kt -o out` compiles and links; `kite program.kt --asm-only -o
out.s` stops at the generated assembly so you can read exactly what the
compiler emitted.

## The language

- **Values** are signed 64-bit integers. That's the point: one type, fully
  understood, all the way down to the register.
- **Functions** with up to six parameters (the SysV register convention),
  recursion, and lexical scope inside a function body.
- **`let` bindings and assignment** with definite-name checking: using an
  undefined variable is a compile error, and redeclaring a name is too.
- **`if`/`else` and `while`** with any expression as the condition
  (zero is false, everything else is true).
- **Arithmetic and comparison**: `+ - * / %`, `== != < <= > >=`, parentheses,
  unary minus, and conventional precedence.
- **`print(expr)`** writes the decimal value and a newline to stdout.
- **Comments** start with `#`.

The program's exit code is `main`'s return value.

## Errors

Compile errors name the file, the line, and what was expected:

```text
kite: bad.kt: undefined variable 'nosuchvar'
kite: bad.kt: line 3: expected ';'
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the pipeline, the
stack-machine codegen model, and the calling convention.

## Tests

```bash
./tests/run-tests.sh
```

Lexer and parser unit suites (26 checks), then every program in
`tests/programs/` and `examples/` is compiled and run for real; stdout is
diffed against the expected file and exit codes are checked. Negative tests
assert that bad programs fail to compile instead of crashing. CI runs the
same script on every push.

## Layout

```text
src/      lex.c, parse.c (AST), gen.c (x86-64 codegen), main.c
tests/    unit suites and compile-and-run programs with golden output
examples/ fib.kt, fizzbuzz.kt
docs/     architecture notes
```

## Tested / not tested

Tested here: 26 lexer/parser unit checks, and every fixture compiled and
executed for real on x86-64 Linux with golden stdout and asserted exit
codes, plus negative tests that bad programs fail to compile.

Not tested: non-x86-64 targets (codegen is x86-64 Linux only), programs
large enough to stress the fixed 256-local limit, and any optimization
(there is no optimizer by design - correctness first).
