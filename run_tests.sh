#!/bin/sh

set -eu

TEST_DIR="./tests"

fail=0

for t in "$TEST_DIR"/t*.sh; do
    [ -f "$t" ] || continue
    echo "> RUN "$t""

    if ! sh "$t"; then
        echo "> FAIL "$t""
        fail=1
    fi
done

exit "$fail"
