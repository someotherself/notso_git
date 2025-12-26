#!/bin/sh

set -eu

fail() { echo "FAIL t0003-add.sh: $*" >&2; exit 1; }
pass() { echo "PASS t0003-add.sh"; }


T="$(mktemp -d /tmp/notsogit-test.XXXXXX)"
trap cleanup EXIT INT TERM

cleanup() { rm -rf "$T"; }

VALGRIND="valgrind --quiet --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect,possible \
  --error-exitcode=99"

BIN="$(pwd)/build/notsogit"

cd "$T"

FILE="$T/file.txt"
printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE"

FILE2="$T/file2.txt"
printf "hello hello hello hello \n" > "$FILE2"

FILE3="$T/file3.txt"
printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE3"

FILE4="$T/file4.txt"
printf "hello hello hello hello hello hello \n" > "$FILE4"

FILE5="$T/file5.txt"
printf "hello hello hello hello hello hello hello \n" > "$FILE5"

"$BIN" init >/dev/null || fail "init failed"

$VALGRIND "$BIN" add file.txt

git init >/dev/null || fail "init failed"
git add "$FILE"

INDEX="$T/.notso_git/index"
EXPECTED="$T/.git/index"

cmp -s "$INDEX" "$EXPECTED" || fail "Output does not match on FIRST PASS."

$VALGRIND "$BIN" add file2.txt
git add "$FILE2"

cmp -s "$INDEX" "$EXPECTED" || fail "Output does not match on SECOND PASS."

$VALGRIND "$BIN" add file3.txt
git add "$FILE3"

cmp -s "$INDEX" "$EXPECTED" || fail "Output does not match on THIRD PASS."

$VALGRIND "$BIN" add file4.txt
git add "$FILE4"

cmp -s "$INDEX" "$EXPECTED" || fail "Output does not match on FOURTH PASS."

$VALGRIND "$BIN" add file5.txt
git add "$FILE5"

cmp -s "$INDEX" "$EXPECTED" || fail "Output does not match on FIFTH PASS."

LS_FILES="$($VALGRIND "$BIN" ls-files)"
EXPECTED_LS_FILES="$(git ls-files)"

cmp -s "$INDEX" "$EXPECTED" || fail "LS-FILES do not match."

pass
