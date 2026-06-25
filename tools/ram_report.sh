#!/bin/sh
set -eu

ARM_SIZE=${ARM_SIZE:-arm-none-eabi-size}
RISCV_SIZE=${RISCV_SIZE:-riscv-none-elf-size}
HOST_CC=${HOST_CC:-cc}
OUT=${1:-build/reports/ram-budget.txt}
ARM_ELF=${ARM_ELF:-build/arm/cortex-m3/AIXOS.elf}
RISCV_ELF=${RISCV_ELF:-build/riscv/AIXOS.elf}

print_config()
{
    profile=$1
    printf '#include "config/aixos_cfg.h"\n' |
        "$HOST_CC" -E -dM -x c -I. \
            -DAIXOS_CFG_PROFILE_MINIMAL="$profile" - |
        awk '/^#define AIXOS_CFG_(MAX_|TASK_HANDLE_LIMIT|TASK_SLOT_PAGE_SIZE|HEAP_SIZE|TRACE_BUFFER_SIZE|IDLE_STACK_SIZE|TIMER_STACK_SIZE|DEFAULT_STACK_SIZE|ISR_COPY_MAX_BYTES|ISR_MQ_SHIFT_MAX)/ {
            sub(/^#define[[:space:]]+/, "")
            print
        }' |
        sort
}

mkdir -p "$(dirname "$OUT")"
{
    printf 'AIXOS RAM/ROM budget report\n'
    printf 'Generated: '
    date -u '+%Y-%m-%dT%H:%M:%SZ'
    printf '\nDefault profile limits\n'
    print_config 0
    printf '\nMinimal profile limits\n'
    print_config 1
    printf '\nCortex-M3 image\n'
    "$ARM_SIZE" "$ARM_ELF"
    printf '\nRV32IM image\n'
    "$RISCV_SIZE" "$RISCV_ELF"
} > "$OUT"
printf '%s\n' "$OUT"
