#!/bin/sh
# Doc-claims guard: numbers stated in README.md must match the suite.
set -e
cd "$(dirname "$0")/.."
OUT=build/docs-check
mkdir -p "$OUT"

cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc \
    tests/unit/test_lex.c src/lex.c -o "$OUT/test_lex"
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc \
    tests/unit/test_parse.c src/lex.c src/parse.c -o "$OUT/test_parse"

LEX_N=$("$OUT/test_lex" | grep -o '^[0-9]* checks passed' | grep -o '^[0-9]*')
PARSE_N=$("$OUT/test_parse" | grep -o '^[0-9]* checks passed' | grep -o '^[0-9]*')
UNIT=$((LEX_N + PARSE_N))
STATED_UNIT=$(grep -o '([0-9]* checks)' README.md | head -1 | grep -o '[0-9]*')
[ "$UNIT" = "$STATED_UNIT" ] || { echo "DOC DRIFT: README says $STATED_UNIT unit checks, suites run $UNIT"; exit 1; }

PROGRAMS=$(ls tests/programs/*.kt examples/*.kt | wc -l)
STATED_PROGRAMS=$(grep -o '[0-9]* programs in' README.md | head -1 | grep -o '^[0-9]*')
[ "$PROGRAMS" = "$STATED_PROGRAMS" ] || { echo "DOC DRIFT: README says $STATED_PROGRAMS programs, $PROGRAMS exist"; exit 1; }

ERRORS=$(ls tests/errors/*.kt | wc -l)
STATED_ERRORS=$(grep -oi '[0-9]* error fixtures' README.md | head -1 | grep -o '^[0-9]*')
[ "$ERRORS" = "$STATED_ERRORS" ] || { echo "DOC DRIFT: README says $STATED_ERRORS error fixtures, $ERRORS exist"; exit 1; }

echo "doc counts verified: $UNIT checks, $PROGRAMS programs, $ERRORS error fixtures"
