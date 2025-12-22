#!/bin/sh

set -eu

# >&2 -> redirect to stderr
# &* -> positional character. Takes all the cli args as one string
fail() { echo "FAIL t0003-add.sh: $*" >&2; exit 1; }
pass() { echo "PASS t0003-add.sh"; }


T="$(mktemp -d /tmp/notsogit-test.XXXXXX)"
trap cleanup EXIT INT TERM

cleanup() { rm -rf "$T"; }

VALGRIND="valgrind --quiet --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect,possible \
  --error-exitcode=99"

# There is no global working directory, or env var for the repo. Always use absolute paths.
BIN="$(pwd)/build/notsogit"

cd "$T"

# Create a file to test with
#FILE="$T/file.txt"
#printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE"

# Create a base repo in the tmp folder
"$BIN" init >/dev/null || fail "init failed"

# For now, a mock input
FILE="file"

OUT="$T/out.txt"
$VALGRIND "$BIN" add "$FILE" >"$OUT"

EXPECTED="$T/expected.txt"
echo "Received path: file" > "$EXPECTED"
if ! cmp -s "$OUT" "$EXPECTED"; then
    fail "Output does not match: "$OUT""
fi

pass
