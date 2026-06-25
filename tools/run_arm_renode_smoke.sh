#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EXP="tools/renode_metrics.exp"
CONFIG="tools/renode.config"
DEFAULT_ELF="build/arm/cortex-m3/AIXOS.elf"
ELF=${2:-$DEFAULT_ELF}

run_smoke()
{
    name=$1
    platform=$2
    rm -f "$CONFIG.lock"
    output=$("$EXP" "$CONFIG" "$platform" "$ELF" 0.2 \
        heartbeat test_heartbeat \
        ticks total_ticks \
        user test_user_heartbeat \
        errors test_user_syscall_errors)
    printf "%s\n" "$output"

    heartbeat=$(printf "%s\n" "$output" | awk '/METRIC heartbeat / { value = $NF } END { print value + 0 }')
    ticks=$(printf "%s\n" "$output" | awk '/METRIC ticks / { value = $NF } END { print value + 0 }')
    user=$(printf "%s\n" "$output" | awk '/METRIC user / { value = $NF } END { print value + 0 }')
    errors=$(printf "%s\n" "$output" | awk '/METRIC errors / { value = $NF } END { print value + 0 }')

    if [ "$heartbeat" -le 0 ] || [ "$ticks" -le 0 ] || \
       [ "$user" -le 0 ] || [ "$errors" -ne 0 ]; then
        printf "%s smoke failed: heartbeat=%s ticks=%s user=%s errors=%s\n" \
            "$name" "$heartbeat" "$ticks" "$user" "$errors" >&2
        exit 1
    fi

    printf "%-14s PASS heartbeat=%s ticks=%s user=%s errors=%s\n" \
        "$name" "$heartbeat" "$ticks" "$user" "$errors"
}

run_a55_smoke()
{
    name=cortex-a55
    platform="simulation/cortex_a55.repl"
    rm -f "$CONFIG.lock"
    output=$(RENODE_ARMV8A_EL1=1 "$EXP" "$CONFIG" "$platform" "$ELF" 0.2 \
        heartbeat test_heartbeat \
        ticks total_ticks \
        user test_user_heartbeat \
        errors test_user_syscall_errors)
    printf "%s\n" "$output"

    heartbeat=$(printf "%s\n" "$output" | awk '/METRIC heartbeat / { value = $NF } END { print value + 0 }')
    ticks=$(printf "%s\n" "$output" | awk '/METRIC ticks / { value = $NF } END { print value + 0 }')
    user=$(printf "%s\n" "$output" | awk '/METRIC user / { value = $NF } END { print value + 0 }')
    errors=$(printf "%s\n" "$output" | awk '/METRIC errors / { value = $NF } END { print value + 0 }')

    if [ "$heartbeat" -le 0 ] || [ "$ticks" -le 0 ] || \
       [ "$user" -le 0 ] || [ "$errors" -ne 0 ]; then
        printf "%s smoke failed: heartbeat=%s ticks=%s user=%s errors=%s\n" \
            "$name" "$heartbeat" "$ticks" "$user" "$errors" >&2
        exit 1
    fi

    printf "%-14s PASS heartbeat=%s ticks=%s user=%s errors=%s\n" \
        "$name" "$heartbeat" "$ticks" "$user" "$errors"
}

run_named_smoke()
{
    case "$1" in
        cortex-m0) run_smoke cortex-m0 "simulation/cortex_m0.repl" ;;
        cortex-m3) run_smoke cortex-m3 "simulation/stm32f103.repl" ;;
        cortex-m4) run_smoke cortex-m4 "simulation/cortex_m4.repl" ;;
        cortex-m33) run_smoke cortex-m33 "simulation/cortex_m33.repl" ;;
        cortex-a55) run_a55_smoke ;;
        *)
            printf "unsupported AIXOS ARM smoke target: %s\n" "$1" >&2
            exit 2
            ;;
    esac
}

if [ "$#" -gt 0 ]; then
    run_named_smoke "$1"
else
    run_named_smoke cortex-m3
    run_named_smoke cortex-m4
fi
