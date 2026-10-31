# kite

**[Compile programs in your browser](https://aditya2375.github.io/kite/)** - the real compiler compiled to WebAssembly, generating x86-64 assembly live.

A small compiled programming language. kite programs compile to native
x86-64 Linux executables - the compiler lexes, parses, and type-checks
names, then runs a semantic pass (name resolution, arity, scoping), then emits
real assembly, assembled and linked with the system C toolchain - invoked
directly with exec, never through a shell. No runtime, no garbage collector, no dependencies: the output
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
- **`let` bindings and assignment** with block scope: a name lives from its
  declaration to the end of its block, an inner block may shadow an outer
  name, and using a name outside its scope is a compile error.
- **`if`/`else` and `while`** with any expression as the condition
  (zero is false, everything else is true).
- **Arithmetic and comparison**: `+ - * / %`, `== != < <= > >=`, parentheses,
  unary minus, and conventional precedence.

### Division semantics

`/` and `%` lower to a single x86-64 `idiv`. Two inputs make the hardware
trap and the kernel raises `SIGFPE`, exactly as in C on this platform:

- division by zero, and
- `INT64_MIN / -1` (or `%`), whose quotient does not fit in 64 bits.

Both are the program's fault, not the compiler's: kite checks your names and
your scopes, and the machine checks your arithmetic. A program that might
divide by zero should test its divisor first.
- **`print(expr)`** writes the decimal value and a newline to stdout.
- **Comments** start with `#`.

The program's exit code is `main`'s return value.

## Errors

Compile errors name the file, the line, and what was expected:

```text
kite: bad.kt: line 3: undefined variable 'nosuchvar'
kite: bad.kt: line 3: expected ';'
kite: bad.kt: line 5: call to undefined function 'hepler'
kite: bad.kt: 'add' takes 2 argument(s), got 1
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the pipeline, the
stack-machine codegen model, and the calling convention.

## Tests

```bash
./tests/run-tests.sh
```

Lexer and parser unit suites (26 checks), then 13 programs in
`tests/programs/` and `examples/` are compiled and run for real; stdout is
diffed against the expected file and exit codes are checked. 12 error
fixtures in `tests/errors/` assert the exact diagnostic the compiler prints
for each bad program, including line numbers. CI runs the same script on
every push, and `tests/check-docs.sh` fails CI if the counts in this file
drift from the real suite. A sanitizer job (ASan/UBSan/LSan) compiles every
fixture and fuzzes the front end with 580 mutated and random inputs.

## Layout

```text
src/      lex.c, parse.c (AST), sema.c (semantic pass), gen.c (x86-64), main.c
tests/    unit suites, compile-and-run programs, error-message fixtures
examples/ fib.kt, fizzbuzz.kt
docs/     architecture notes
```

## Tested / not tested

Tested here: 26 lexer/parser unit checks, 13 programs compiled and
executed for real on x86-64 Linux with golden stdout and asserted exit
codes, and 12 error fixtures asserting the exact diagnostic text.
Division traps (divide by zero, INT64_MIN / -1) verified to raise SIGFPE
at runtime as documented. Nested-call argument evaluation keeps the stack
16-byte aligned at every call site. The scope limits are enforced with
clean errors and pinned by fixtures: 256 names per scope, 128 nested
blocks, 256 live locals in codegen. The compiler itself runs
ASan/UBSan/LSan-clean over every fixture plus 580 fuzz inputs.

Not tested: non-x86-64 targets (codegen is x86-64 Linux only) and any
optimization (there is no optimizer by design - correctness first).
