#!/bin/sh
#
# -> 1
# Create a blob with hash-object -w
#
# Run cat-file and the compare the output with the original
#
# -> 2
# Create the same blob with .git and compare the hash
#

# -e -> exit on error; -u -> treat undef variables as errors
set -eu

# >&2 -> redirect to stderr
# &* -> positional character. Takes all the cli args as one string
fail() { echo "FAIL t0001-basic.sh: $*" >&2; exit 1; }
pass() { echo "PASS t0001-basic.sh"; }

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
FILE="$T/file.txt"
printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE"

# Create a base repo in the tmp folder
"$BIN" init >/dev/null || fail "init failed"

# Capture output of hash-object -w
FILE="$T/file.txt"
HASH="$($VALGRIND "$BIN" hash-object -w "$FILE")" || fail "hash-object failed"

# Capture output of cat-file
OUT="$T/output.txt"
$VALGRIND "$BIN" cat-file -p "$HASH" > $OUT || fail "cat-file failed"

if ! cmp -s $FILE $OUT
then
    fail "files do not match"
fi

# Create a normal git repo
git init >/dev/null || fail "init failed"

# Hash the same file
GIT_HASH="$(git hash-object "$FILE")" || fail "hash-object failed"

if [ "$HASH" != "$GIT_HASH" ]
then
    fail "git and notsogit diff hash"
fi

cleanup

pass
