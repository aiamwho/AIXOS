#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
RESULTS=${AIXOS_SIM_RESULTS_DIR:-"$ROOT/test-results/instruction-simulation"}
EXP="$ROOT/tools/renode_metrics.exp"
CONFIG="$ROOT/tools/renode.config"
mkdir -p "$RESULTS"

run_case()
{
    name=$1
    platform=$2
    elf=$3
    shift 3
    rm -f "$CONFIG.lock"
    "$EXP" "$CONFIG" "$platform" "$elf" 0.5 "$@" |
        tee "$RESULTS/$name.log"
}

run_case aixos-arm \
    "$ROOT/benchmarks/stm32f103_benchmark.repl" \
    "$ROOT/build/benchmarks/aixos/arm/AIXOS.elf" \
    heartbeat aixos_bench_heartbeat messages aixos_bench_messages \
    errors aixos_bench_errors ticks total_ticks switches switch_count

run_case freertos-arm \
    "$ROOT/benchmarks/stm32f103_benchmark.repl" \
    "$ROOT/build/freertos/arm/FreeRTOS.elf" \
    heartbeat freertos_heartbeat messages freertos_messages \
    errors freertos_errors ticks freertos_ticks switches freertos_switches

run_case aixos-riscv \
    "$ROOT/simulation/riscv32_virt.repl" \
    "$ROOT/build/benchmarks/aixos/riscv/AIXOS.elf" \
    heartbeat aixos_bench_heartbeat messages aixos_bench_messages \
    errors aixos_bench_errors ticks total_ticks switches switch_count

run_case freertos-riscv \
    "$ROOT/simulation/riscv32_virt.repl" \
    "$ROOT/build/freertos/riscv/FreeRTOS.elf" \
    heartbeat freertos_heartbeat messages freertos_messages \
    errors freertos_errors ticks freertos_ticks switches freertos_switches

for log in "$RESULTS"/*.log; do
    case_name=$(basename "$log" .log)
    awk -v name="$case_name" '
        /METRIC / {
            line = $0
            sub(/^.*METRIC /, "", line)
            split(line, fields, " ")
            values[fields[1]] = fields[2]
        }
        END {
            printf "%s,%s,%s,%s,%s,%s\n", name, values["heartbeat"],
                values["messages"], values["errors"], values["ticks"],
                values["switches"]
        }
    ' "$log"
done | {
    printf 'case,heartbeat,messages,errors,ticks,switches\n'
    cat
} > "$RESULTS/results.csv"

cat "$RESULTS/results.csv"
