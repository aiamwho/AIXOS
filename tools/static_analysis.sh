#!/bin/sh
set -eu

CLANG=${CLANG:-clang}
FLAGS="-Itests/host -Iposix/include -I. -Iinclude -Iarch/include -std=c99 -DAIXOS_HOST_TEST=1"
REPORT=$(mktemp)
trap 'rm -f "$REPORT"' EXIT

find kernel compat/posix posix/src tests -name '*.c' -type f |
    sort | while IFS= read -r source; do
    if ! "$CLANG" --analyze -Xanalyzer -analyzer-output=text \
        $FLAGS "$source" -o /dev/null 2>> "$REPORT"; then
        printf '%s\n' "analysis failed: $source" >> "$REPORT"
    fi
done

if [ -s "$REPORT" ]; then
    cat "$REPORT"
    exit 1
fi
printf 'clang static analysis: PASS\n'
