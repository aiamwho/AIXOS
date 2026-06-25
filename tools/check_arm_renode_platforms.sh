#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CONFIG="$ROOT/tools/renode.config"

check_platform()
{
    name=$1
    platform=$2
    rm -f "$CONFIG.lock"
    renode --plain --console --disable-xwt --hide-log --config "$CONFIG" \
        -e "mach create" \
        -e "machine LoadPlatformDescription @$platform" \
        -e "quit" >/dev/null
    printf "%-14s OK  %s\n" "$name" "$platform"
}

run_named_check()
{
    case "$1" in
        cortex-m0) check_platform cortex-m0 "$ROOT/simulation/cortex_m0.repl" ;;
        cortex-m3) check_platform cortex-m3 "$ROOT/simulation/stm32f103.repl" ;;
        cortex-m4) check_platform cortex-m4 "$ROOT/simulation/cortex_m4.repl" ;;
        cortex-m33) check_platform cortex-m33 "$ROOT/simulation/cortex_m33.repl" ;;
        cortex-a55) check_platform cortex-a55 "$ROOT/simulation/cortex_a55.repl" ;;
        *)
            printf "unknown ARM Renode platform: %s\n" "$1" >&2
            exit 2
            ;;
    esac
}

if [ "$#" -gt 0 ]; then
    for name in "$@"; do
        run_named_check "$name"
    done
else
    run_named_check cortex-m0
    run_named_check cortex-m3
    run_named_check cortex-m4
    run_named_check cortex-m33
    run_named_check cortex-a55
fi
