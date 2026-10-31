#!/bin/sh
# Fuzz the front end: mutated and truncated copies of the example
# programs plus random byte soup. The compiler may accept or reject any
# input, but it must never crash or trip a sanitizer. Run against a
# sanitizer build (make EXTRA=-fsanitize=address,undefined).
set -e
cd "$(dirname "$0")/.."
OUT=build/fuzz
mkdir -p "$OUT"

python3 - "$OUT" <<'PY'
import random, sys, glob, os
out = sys.argv[1]
random.seed(20261031)
seeds = []
for f in glob.glob("examples/*.kt") + glob.glob("tests/programs/*.kt"):
    seeds.append(open(f, "rb").read())
n = 0
for s in seeds:
    for _ in range(40):
        b = bytearray(s)
        for _ in range(random.randint(1, 8)):
            op = random.random()
            if op < 0.4 and b:
                b[random.randrange(len(b))] = random.randrange(256)
            elif op < 0.7 and len(b) > 8:
                del b[random.randrange(len(b)):]
            else:
                b.insert(random.randrange(len(b) + 1), random.randrange(256))
        open(os.path.join(out, f"fuzz{n}.kt"), "wb").write(bytes(b))
        n += 1
for _ in range(60):
    open(os.path.join(out, f"fuzz{n}.kt"), "wb").write(
        bytes(random.randrange(256) for _ in range(random.randrange(1, 400))))
    n += 1
print(n)
PY

for f in "$OUT"/fuzz*.kt; do
    if ! ./build/kite "$f" -o "$OUT/a.out" > /dev/null 2> "$OUT/err.txt"; then
        if grep -q "Sanitizer\|runtime error" "$OUT/err.txt"; then
            echo "FUZZ FAIL: sanitizer report on $f" >&2
            cat "$OUT/err.txt" >&2
            exit 1
        fi
    fi
done
echo "FUZZ PASSED"
