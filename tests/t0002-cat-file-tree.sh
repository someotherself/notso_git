#!/bin/sh
# Create a file
# Run git add & git write-tree
# Copy ./.git/objects into ./.notsogit/objects
# Run notsogit cat-file on the tree
# Compare output vs git output
set -eu

fail() { echo "FAIL t0002-cat-file-tree.sh: $*" >&2; exit 1; }
pass() { echo "PASS t0002-cat-file-tree.sh"; }

T="$(mktemp -d /tmp/notsogit-test.XXXXXX)"

# cleanup() { rm -rf "$T"; }
# trap cleanup EXIT INT TERM

VALGRIND="valgrind --quiet --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect,possible \
  --error-exitcode=99"

# There is no env var for the repo. Always use absolute paths.
BIN="$(pwd)/build/notsogit"

cd "$T"

# Create a file to test with
FILE="$T/file.txt"
printf "hello hello hello hello hello hello hello hello hello \n" > "$FILE"

# Init both repos in the tmp folder
"$BIN" init >/dev/null || fail "init failed"
git init >/dev/null || fail "git failed"
git add "$FILE"

HASH="$(git write-tree)" || fail "git write-tree failed"
GIT_OUT="$T/git_out.txt"
git cat-file -p "$HASH" > "$GIT_OUT" || fail "failed to run git cat-file"

cp -r ./.git/objects/ ./.notso_git/ || fail "failed to copy object"

OUT="$T/out.txt"
$VALGRIND "$BIN" cat-file -p "$HASH" > "$OUT" || fail "failed to run notsogit cat-file"

if ! cmp -s "$OUT" "$GIT_OUT"; then
    fail "files differ"
fi

# cleanup

pass
