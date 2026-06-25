#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EXP="tools/renode_metrics.exp"
CONFIG="tools/renode.config"
PLATFORM="simulation/riscv32_virt.repl"
ELF="build/riscv/AIXOS.elf"
REPEAT=${1:-1}

run_once()
{
    iteration=$1
    rm -f "$CONFIG.lock"
    output=$("$EXP" "$CONFIG" "$PLATFORM" "$ELF" 0.4 \
        heartbeat test_heartbeat \
        ticks total_ticks \
        timer_irqs aixos_riscv_timer_interrupts \
        soft_irqs aixos_riscv_software_interrupts \
        bad_cause aixos_riscv_unhandled_mcause \
        reg_errors test_riscv_register_errors \
        user test_user_heartbeat \
        user_errors test_user_syscall_errors)
    printf "%s\n" "$output"

    heartbeat=$(printf "%s\n" "$output" | awk '/METRIC heartbeat / { value = $NF } END { print value + 0 }')
    ticks=$(printf "%s\n" "$output" | awk '/METRIC ticks / { value = $NF } END { print value + 0 }')
    timer_irqs=$(printf "%s\n" "$output" | awk '/METRIC timer_irqs / { value = $NF } END { print value + 0 }')
    soft_irqs=$(printf "%s\n" "$output" | awk '/METRIC soft_irqs / { value = $NF } END { print value + 0 }')
    bad_cause=$(printf "%s\n" "$output" | awk '/METRIC bad_cause / { value = $NF } END { print value + 0 }')
    reg_errors=$(printf "%s\n" "$output" | awk '/METRIC reg_errors / { value = $NF } END { print value + 0 }')
    user=$(printf "%s\n" "$output" | awk '/METRIC user / { value = $NF } END { print value + 0 }')
    user_errors=$(printf "%s\n" "$output" | awk '/METRIC user_errors / { value = $NF } END { print value + 0 }')

    if [ "$heartbeat" -le 0 ] || [ "$ticks" -le 0 ] || \
       [ "$timer_irqs" -le 0 ] || [ "$soft_irqs" -le 0 ] || \
       [ "$bad_cause" -ne 0 ] || [ "$reg_errors" -ne 0 ] || \
       [ "$user" -le 0 ] || [ "$user_errors" -ne 0 ]; then
        printf "riscv smoke failed iteration=%s heartbeat=%s ticks=%s timer_irqs=%s soft_irqs=%s bad_cause=%s reg_errors=%s user=%s user_errors=%s\n" \
            "$iteration" "$heartbeat" "$ticks" "$timer_irqs" "$soft_irqs" \
            "$bad_cause" "$reg_errors" "$user" "$user_errors" >&2
        exit 1
    fi

    printf "riscv PASS iteration=%s heartbeat=%s ticks=%s timer_irqs=%s soft_irqs=%s bad_cause=%s reg_errors=%s user=%s user_errors=%s\n" \
        "$iteration" "$heartbeat" "$ticks" "$timer_irqs" "$soft_irqs" \
        "$bad_cause" "$reg_errors" "$user" "$user_errors"
}

i=1
while [ "$i" -le "$REPEAT" ]; do
    run_once "$i"
    i=$((i + 1))
done
