# kite architecture

## Pipeline

```text
source -> tokens -> AST -> checked AST -> x86-64 assembly -> assembler/linker -> static executable
         (lex.c)  (parse.c)  (sema.c)      (gen.c)
```

Four passes with plain data between them: lexing never sees the grammar,
parsing never sees registers, the semantic pass never sees instructions,
codegen never sees characters. Each pass reports errors with a line number
and stops.

## lex.c

A hand-rolled scanner: keywords, identifiers, decimal integers, two-character
operators (`== != <= >=`), and `#` comments. One pass, no backtracking; the
token array owns its text.

## parse.c

Recursive descent with four precedence levels, outermost first: equality,
relational, additive, multiplicative. `a + 1 < b * 2 == c` parses the way a
C programmer expects, and parentheses override. Statements are a tree of
explicit kinds (`S_LET`, `S_IF`, `S_WHILE`, ...); there is no desugaring, so
the AST you read is the program codegen sees.

## sema.c - the semantic pass

Everything codegen assumes is proven here first, with line-numbered errors:

- every called function exists, and the argument count matches its arity
- every variable is declared and in scope at its use
- no duplicate function definitions or duplicate parameter names
- no redeclaration of a name in the same scope
- a `main` function exists

Scoping is lexical: a scope stack is pushed per block, a name is visible
from its declaration to the end of that block, and an inner declaration
shadows an outer one. Codegen mirrors the same discipline with a mark stack
over its local-slot table, so a name declared in an `if` body stops existing
at the closing brace.

## gen.c - the codegen model

**Stack machine on the real stack.** Every expression pushes its 64-bit
result; binary operators pop rhs into `rdi` and lhs into `rax`, operate,
and push `rax`. Comparisons use `cmp` + `setcc`. Division uses `cqo; idiv`,
with `%` taking the remainder from `rdx`.

**Locals in fixed rbp slots.** Each function's parameters and `let` names
get fixed negative offsets from `rbp`; the frame size is computed before
emitting the body by counting bindings, so `let` inside any nesting depth
has a home. Shadowing follows block scope - an inner `let` reusing a name
gets the innermost slot, and the outer slot is visible again after the
block.

**Calling convention.** System V AMD64, the subset kite needs: first six
arguments in `rdi, rsi, rdx, rcx, r8, r9`, result in `rax`. kite functions
are emitted as `kite_<name>` and call each other directly; recursion works
because everything is on the stack.

**No libc.** The runtime is two hand-written routines: `kite_print`
converts a signed 64-bit value to decimal and writes it with the `write`
syscall, and `_start` calls `kite_main` and passes its return value to the
`exit` syscall. The output links `-nostdlib -static`: the binary that runs
is exactly what the compiler emitted.

## Testing philosophy

Unit tests pin the lexer (token streams, values, error lines) and the parser
(precedence shapes, statement forms, error recovery). The end-to-end suite
compiles every fixture and runs the binary - stdout is golden-filed and the
exit code is asserted, so a codegen regression shows up as a wrong number
printed by a real process, not as a diff in assembly text. Negative tests
compile invalid programs and require a clean error, not a crash.
