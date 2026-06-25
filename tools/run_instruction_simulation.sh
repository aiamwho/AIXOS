#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
RESULTS=${AIXOS_SIM_RESULTS_DIR:-"$ROOT/test-results/instruction-simulation"}
cd "$ROOT"

EXP="tools/renode_metrics.exp"
CONFIG="tools/renode.config"
mkdir -p "$RESULTS"
rm -f "$RESULTS"/*.log "$RESULTS/results.csv"

run_case()
{
    name=$1
    platform=$2
    elf=$3
    shift 3
    rm -f "$CONFIG.lock"
    "$EXP" "$CONFIG" "$platform" "$elf" 0.5 "$@" > "$RESULTS/$name.log"
    cat "$RESULTS/$name.log"
}

skip_case()
{
    name=$1
    reason=$2
    printf "SKIP %s %s\n" "$name" "$reason" | tee "$RESULTS/$name.log"
}

run_optional_case()
{
    name=$1
    platform=$2
    elf=$3
    shift 3
    if [ -f "$elf" ]; then
        run_case "$name" "$platform" "$elf" "$@"
    else
        skip_case "$name" "missing ELF: $elf"
    fi
}

run_a55_case()
{
    name=$1
    platform=$2
    elf=$3
    shift 3
    rm -f "$CONFIG.lock"
    RENODE_ARMV8A_EL1=1 "$EXP" "$CONFIG" "$platform" "$elf" 0.5 "$@" > "$RESULTS/$name.log"
    cat "$RESULTS/$name.log"
}

run_a55_case aixos-a55 \
    "simulation/cortex_a55.repl" \
    "build/arm/cortex-a55/AIXOS.elf" \
    heartbeat test_heartbeat user test_user_heartbeat \
    errors test_user_syscall_errors ticks total_ticks

if [ "${AIXOS_SIM_INCLUDE_BENCHMARKS:-0}" = "1" ]; then
    run_case aixos-arm \
        "benchmarks/stm32f103_benchmark.repl" \
        "build/benchmarks/aixos/arm/AIXOS.elf" \
        heartbeat aixos_bench_heartbeat messages aixos_bench_messages \
        errors aixos_bench_errors ticks total_ticks switches switch_count

    run_optional_case freertos-arm \
        "benchmarks/stm32f103_benchmark.repl" \
        "build/freertos/arm/FreeRTOS.elf" \
        heartbeat freertos_heartbeat messages freertos_messages \
        errors freertos_errors ticks freertos_ticks switches freertos_switches

    run_case aixos-riscv \
        "simulation/riscv32_virt.repl" \
        "build/benchmarks/aixos/riscv/AIXOS.elf" \
        heartbeat aixos_bench_heartbeat messages aixos_bench_messages \
        errors aixos_bench_errors ticks total_ticks switches switch_count

    run_optional_case freertos-riscv \
        "simulation/riscv32_virt.repl" \
        "build/freertos/riscv/FreeRTOS.elf" \
        heartbeat freertos_heartbeat messages freertos_messages \
        errors freertos_errors ticks freertos_ticks switches freertos_switches
fi

for log in "$RESULTS"/*.log; do
    case_name=$(basename "$log" .log)
    awk -v name="$case_name" '
        /^SKIP / {
            status = "skipped"
        }
        /METRIC / {
            status = "ran"
            line = $0
            sub(/^.*METRIC /, "", line)
            split(line, fields, " ")
            values[fields[1]] = fields[2]
        }
        END {
            if (status == "") {
                status = "unknown"
            }
            printf "%s,%s,%s,%s,%s,%s,%s,%s\n", name, status,
                values["heartbeat"], values["messages"], values["user"],
                values["errors"], values["ticks"], values["switches"]
        }
    ' "$log"
done | {
    printf 'case,status,heartbeat,messages,user,errors,ticks,switches\n'
    cat
} > "$RESULTS/results.csv"

cat "$RESULTS/results.csv"
