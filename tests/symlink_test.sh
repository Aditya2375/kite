#!/bin/sh
# Symlink-attack regression: the compiler writes generated assembly to a
# temp file. A predictable name lets another local user pre-create it as
# a symlink and make kite overwrite a file they own.
set -e
cd "$(dirname "$0")/.."
OUT=build/tests
mkdir -p "$OUT"

VICTIM="$PWD/$OUT/victim.txt"
echo "precious data" > "$VICTIM"
# blanket the pid space ahead of us with symlinks to the victim. A shell
# loop would fork per link and outrun its own blanket, so do it in one
# python process: pids stay put and the compiler's fork lands inside.
VICTIM="$VICTIM" python3 - <<'PYEOF'
import os
victim = os.environ["VICTIM"]
me = os.getpid()
for p in range(me, me + 3000):
    try:
        os.symlink(victim, f"/tmp/kite-{p}.s")
    except FileExistsError:
        pass
PYEOF

cat > "$OUT/hello.kt" <<'KT'
fn main() {
    return 0;
}
KT
./build/kite "$OUT/hello.kt" -o "$OUT/hello.bin" >/dev/null 2>&1 || {
    echo "SYMLINK TEST: compile failed"; rm -f /tmp/kite-*.s; exit 1
}
rm -f /tmp/kite-*.s

if [ "$(cat "$VICTIM")" != "precious data" ]; then
    echo "SYMLINK TEST FAILED: kite wrote through a pre-planted symlink"
    exit 1
fi
# no temp assembly left behind: compiler temps are /tmp/kite-<pid>.s (old)
# or /tmp/kite-XXXXXX (new); ignore unrelated kite-* files
leftovers=$(ls -d /tmp/kite-[0-9]*.s /tmp/kite-?????? 2>/dev/null || true)
if [ -n "$leftovers" ]; then
    echo "SYMLINK TEST FAILED: temp assembly file left in /tmp: $leftovers"
    exit 1
fi
echo "SYMLINK TEST PASSED"
