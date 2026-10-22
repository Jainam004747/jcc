#!/bin/bash
# Differential testing: every program in tests/programs is compiled twice, once by gcc (the
# reference) and once by jcc. Both executables must print exactly the same output and exit
# with the same status. Programs in tests/errors must be REJECTED by jcc with the message
# given in their "// expect-error:" comment.
set -u
cd "$(dirname "$0")/.."
JCC=${JCC:-./jcc}
OUT=tests/out
mkdir -p "$OUT"
pass=0
fail=0

for src in tests/programs/*.c; do
  name=$(basename "$src" .c)
  gcc -w -o "$OUT/$name.gcc" "$src" 2>"$OUT/$name.gcc.err" || { echo "SKIP $name (gcc failed)"; continue; }
  if ! $JCC -o "$OUT/$name.jcc" "$src" 2>"$OUT/$name.jcc.err"; then
    echo "FAIL $name: jcc could not compile it"; sed 's/^/     /' "$OUT/$name.jcc.err" | head -8
    fail=$((fail + 1)); continue
  fi
  "$OUT/$name.gcc" >"$OUT/$name.expected" 2>&1; want=$?
  timeout 10 "$OUT/$name.jcc" >"$OUT/$name.actual" 2>&1; got=$?
  if [ "$want" != "$got" ] || ! cmp -s "$OUT/$name.expected" "$OUT/$name.actual"; then
    echo "FAIL $name: exit $got (expected $want)"
    diff "$OUT/$name.expected" "$OUT/$name.actual" | head -10 | sed 's/^/     /'
    fail=$((fail + 1))
  else
    pass=$((pass + 1))
  fi
done

for src in tests/errors/*.c; do
  name=$(basename "$src" .c)
  expected=$(sed -n 's|^// expect-error: ||p' "$src")
  if $JCC -S -o /dev/null "$src" 2>"$OUT/$name.err"; then
    echo "FAIL error/$name: jcc accepted invalid code"; fail=$((fail + 1))
  elif ! grep -qF -- "$expected" "$OUT/$name.err"; then
    echo "FAIL error/$name: wanted \"$expected\", got:"; sed 's/^/     /' "$OUT/$name.err" | head -4
    fail=$((fail + 1))
  else
    pass=$((pass + 1))
  fi
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
