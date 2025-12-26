#!/bin/sh
set -eu

fail() { echo "FAIL t0004-add_2.sh: $*" >&2; exit 1; }
pass() { echo "PASS t0004-add_2.sh"; }

T="$(mktemp -d /tmp/notsogit-test.XXXXXX)"
# trap cleanup EXIT INT TERM

# cleanup() { rm -rf "$T"; }

VALGRIND="valgrind --quiet --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect,possible \
  --error-exitcode=99"

BIN="$(pwd)/build/notsogit"

cd "$T"

TARGET=file.txt
FILE="$T/file.txt"
printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE"

"$BIN" init >/dev/null || fail "init failed"

$VALGRIND "$BIN" add file.txt

git init >/dev/null || fail "init failed"

INDEX="$T/.notso_git/index"
GIT="$T/.git"
cp "$INDEX" "$GIT"

OUT="$(git ls-files)"
cmp -s "$OUT" "$TARGET" || fail Wrong output

# cleanup

pass
