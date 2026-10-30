#!/bin/sh
# kite test suite: lexer/parser unit tests, then compile every program in
# tests/programs, run it for real, and diff stdout and the exit code.
set -e
cd "$(dirname "$0")/.."
OUT=build/tests
mkdir -p "$OUT"

cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc \
    tests/unit/test_lex.c src/lex.c -o "$OUT/test_lex"
"$OUT/test_lex"

cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc \
    tests/unit/test_parse.c src/lex.c src/parse.c -o "$OUT/test_parse"
"$OUT/test_parse"

for src in tests/programs/*.kt examples/*.kt; do
    name=$(basename "$src" .kt)
    bin="$OUT/$name"
    ./build/kite "$src" -o "$bin"
    if [ -f "tests/programs/$name.expected" ]; then
        set +e
        got=$("$bin")
        set -e
        want=$(cat "tests/programs/$name.expected")
        if [ "$got" != "$want" ]; then
            echo "FAIL $name: output differs" >&2
            printf '%s\n' "$want" > "$OUT/want.txt"
            printf '%s\n' "$got" > "$OUT/got.txt"
            diff "$OUT/want.txt" "$OUT/got.txt" >&2 || true
            exit 1
        fi
        echo "PASS $name"
    else
        "$bin" > /dev/null
        echo "PASS $name (runs)"
    fi
    if [ -f "tests/programs/$name.exitcode" ]; then
        "$bin" > /dev/null 2>&1 || code=$?
        code=${code:-0}
        want=$(cat "tests/programs/$name.exitcode")
        if [ "$code" != "$want" ]; then
            echo "FAIL $name: exit code $code, want $want" >&2
            exit 1
        fi
        echo "PASS $name exit code"
    fi
    unset code
done

# error cases: compilation must fail, not crash
printf 'fn main() { return nosuchvar; }' > "$OUT/bad.kt"
if ./build/kite "$OUT/bad.kt" -o "$OUT/bad" 2>/dev/null; then
    echo "FAIL: undefined variable compiled" >&2
    exit 1
fi
printf 'fn other() { return 0; }' > "$OUT/nomain.kt"
if ./build/kite "$OUT/nomain.kt" -o "$OUT/nomain" 2>/dev/null; then
    echo "FAIL: missing main compiled" >&2
    exit 1
fi
echo "PASS error handling"
echo "ALL TESTS PASSED"
