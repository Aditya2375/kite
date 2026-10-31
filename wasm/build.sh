#!/bin/sh
# Rebuild the browser demo in docs/ (requires emsdk on PATH).
set -e
cd "$(dirname "$0")/.."
emcc -O2 -Isrc src/lex.c src/parse.c src/sema.c src/gen.c wasm/kite_wasm.c -o docs/kite.js \
    -sEXPORTED_FUNCTIONS=_kite_compile,_kite_free \
    -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString -sMODULARIZE=1 -sEXPORT_NAME=KiteModule \
    -sSTACK_SIZE=4194304 -sALLOW_MEMORY_GROWTH=1
