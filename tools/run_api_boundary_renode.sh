#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

APP_SRCS="examples/api_boundary/main.c"
EXP="tools/renode_metrics.exp"
CONFIG="tools/renode.config"
OUT_DIR="test-results/api-boundary"
CSV="$OUT_DIR/results.csv"
REPORT="$OUT_DIR/report.md"
REPORT_ZH="$OUT_DIR/report.zh-CN.md"
RISCV_PREFIX_VALUE=${RISCV_PREFIX:-riscv64-elf-}
MAKE_CMD=${MAKE:-make}

mkdir -p "$OUT_DIR"
rm -f "$CSV" "$REPORT" "$REPORT_ZH"
printf "target,checks,failures,done,heartbeat,task_checks,ipc_checks,timer_checks,memory_checks,mpu_checks,user_checks,user_failures,user_done,user_heartbeat,phase\n" > "$CSV"

metric_value()
{
    name=$1
    text=$2
    printf "%s\n" "$text" |
    awk -v metric="$name" '{
        for (i = 1; i + 2 <= NF; i++) {
            if (($i == "METRIC" || $i ~ /METRIC$/) && $(i + 1) == metric) {
                value = $(i + 2)
            }
        }
    } END { print value + 0 }'
}

run_metrics()
{
    target=$1
    platform=$2
    elf=$3
    duration=$4

    rm -f "$CONFIG.lock"
    output=$("$EXP" "$CONFIG" "$platform" "$elf" "$duration" \
        checks boundary_checks \
        failures boundary_failures \
        done boundary_done \
        heartbeat boundary_heartbeat \
        task_checks boundary_task_checks \
        ipc_checks boundary_ipc_checks \
        timer_checks boundary_timer_checks \
        memory_checks boundary_memory_checks \
        mpu_checks boundary_mpu_checks \
        user_checks boundary_user_checks \
        user_failures boundary_user_failures \
        user_done boundary_user_done \
        user_heartbeat boundary_user_heartbeat \
        phase boundary_phase)
    printf "%s\n" "$output"

    checks=$(metric_value checks "$output")
    failures=$(metric_value failures "$output")
    done=$(metric_value done "$output")
    heartbeat=$(metric_value heartbeat "$output")
    task_checks=$(metric_value task_checks "$output")
    ipc_checks=$(metric_value ipc_checks "$output")
    timer_checks=$(metric_value timer_checks "$output")
    memory_checks=$(metric_value memory_checks "$output")
    mpu_checks=$(metric_value mpu_checks "$output")
    user_checks=$(metric_value user_checks "$output")
    user_failures=$(metric_value user_failures "$output")
    user_done=$(metric_value user_done "$output")
    user_heartbeat=$(metric_value user_heartbeat "$output")
    phase=$(metric_value phase "$output")

    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
        "$target" "$checks" "$failures" "$done" "$heartbeat" \
        "$task_checks" "$ipc_checks" "$timer_checks" "$memory_checks" \
        "$mpu_checks" "$user_checks" "$user_failures" "$user_done" \
        "$user_heartbeat" "$phase" >> "$CSV"

    if [ "$done" -ne 1 ] || [ "$failures" -ne 0 ] || \
       [ "$heartbeat" -le 0 ] || [ "$checks" -lt 100 ] || \
       [ "$task_checks" -lt 15 ] || [ "$ipc_checks" -lt 60 ] || \
       [ "$timer_checks" -lt 10 ] || [ "$memory_checks" -lt 15 ] || \
       [ "$mpu_checks" -lt 6 ] || [ "$user_done" -ne 1 ] || \
       [ "$user_heartbeat" -le 0 ] || [ "$user_checks" -le 0 ] || \
       [ "$user_failures" -ne 0 ]; then
        printf "api boundary failed target=%s checks=%s failures=%s done=%s heartbeat=%s user_done=%s user_heartbeat=%s user_failures=%s phase=%s\n" \
            "$target" "$checks" "$failures" "$done" "$heartbeat" \
            "$user_done" "$user_heartbeat" "$user_failures" "$phase" >&2
        exit 1
    fi

    printf "api-boundary PASS target=%s checks=%s failures=%s heartbeat=%s user_checks=%s user_heartbeat=%s\n" \
        "$target" "$checks" "$failures" "$heartbeat" "$user_checks" "$user_heartbeat"
}

printf "Building Cortex-M3 API boundary image...\n"
"$MAKE_CMD" arm AIXOS_PLATFORM=cortex-m3 APP_SRCS="$APP_SRCS" \
    ARM_BUILD=build/arm/api-boundary-cortex-m3
run_metrics "cortex-m3" "simulation/stm32f103.repl" \
    "build/arm/api-boundary-cortex-m3/AIXOS.elf" 0.8

printf "Building RISC-V API boundary image...\n"
"$MAKE_CMD" riscv-validate APP_SRCS="$APP_SRCS" \
    RISCV_BUILD=build/riscv-api-boundary RISCV_PREFIX="$RISCV_PREFIX_VALUE"
run_metrics "riscv32-virt" "simulation/riscv32_virt.repl" \
    "build/riscv-api-boundary/AIXOS.elf" 0.8

{
    printf "# AIXOS API Boundary Simulator Test Report\n\n"
    printf "%s\n" "- Date: 2026-06-29"
    printf "%s\n" "- Application: \`$APP_SRCS\`"
    printf "%s\n" "- Simulator: Renode instruction simulation"
    printf "%s\n\n" "- Targets: Cortex-M3 (\`simulation/stm32f103.repl\`), RISC-V virt (\`simulation/riscv32_virt.repl\`)"
    printf "## Scope\n\n"
    printf "The test validates runtime API parameter boundaries and functional boundary conditions after the scheduler starts. Coverage includes task lifecycle, semaphore, mutex, message queue, event, pipe, task notification, software timer, heap lockdown, fixed-block mempool, MPU validation, and user-mode syscall wrappers.\n\n"
    printf "## Results\n\n"
    printf "| Target | Checks | Failures | Kernel heartbeat | User checks | User heartbeat | Result |\n"
    printf "| --- | ---: | ---: | ---: | ---: | ---: | --- |\n"
    tail -n +2 "$CSV" | while IFS=, read -r target checks failures done heartbeat task_checks ipc_checks timer_checks memory_checks mpu_checks user_checks user_failures user_done user_heartbeat phase; do
        result=PASS
        if [ "$failures" -ne 0 ] || [ "$done" -ne 1 ] || [ "$user_failures" -ne 0 ] || [ "$user_done" -ne 1 ]; then
            result=FAIL
        fi
        printf "| %s | %s | %s | %s | %s | %s | %s |\n" \
            "$target" "$checks" "$failures" "$heartbeat" \
            "$user_checks" "$user_heartbeat" "$result"
    done
    printf "\n## Category Counts\n\n"
    printf "| Target | Task | IPC/Notify | Timer | Memory | MPU | Final phase |\n"
    printf "| --- | ---: | ---: | ---: | ---: | ---: | ---: |\n"
    tail -n +2 "$CSV" | while IFS=, read -r target checks failures done heartbeat task_checks ipc_checks timer_checks memory_checks mpu_checks user_checks user_failures user_done user_heartbeat phase; do
        printf "| %s | %s | %s | %s | %s | %s | %s |\n" \
            "$target" "$task_checks" "$ipc_checks" "$timer_checks" \
            "$memory_checks" "$mpu_checks" "$phase"
    done
    printf "\n## Acceptance Criteria\n\n"
    printf "%s\n" "- \`boundary_done == 1\` and \`boundary_failures == 0\` on every target."
    printf "%s\n" "- Kernel and user heartbeats advance after the boundary checks complete."
    printf "%s\n" "- At least 100 kernel boundary checks execute per target."
    printf "%s\n" "- User-mode syscall wrapper checks execute with zero failures."
} > "$REPORT"

{
    printf "# AIXOS API 边界仿真测试报告\n\n"
    printf "%s\n" "- 日期：2026-06-29"
    printf "%s\n" "- 测试应用：\`$APP_SRCS\`"
    printf "%s\n" "- 仿真器：Renode 指令仿真"
    printf "%s\n\n" "- 目标：Cortex-M3（\`simulation/stm32f103.repl\`）、RISC-V virt（\`simulation/riscv32_virt.repl\`）"
    printf "## 覆盖范围\n\n"
    printf "本测试在调度器启动后验证 API 输入参数边界和功能使用边界。覆盖 task lifecycle、semaphore、mutex、message queue、event、pipe、task notification、software timer、heap lockdown、fixed-block mempool、MPU validation 和 user-mode syscall wrapper。\n\n"
    printf "## 结果\n\n"
    printf "| 目标 | 检查数 | 失败数 | 内核 heartbeat | 用户态检查数 | 用户态 heartbeat | 结果 |\n"
    printf "| --- | ---: | ---: | ---: | ---: | ---: | --- |\n"
    tail -n +2 "$CSV" | while IFS=, read -r target checks failures done heartbeat task_checks ipc_checks timer_checks memory_checks mpu_checks user_checks user_failures user_done user_heartbeat phase; do
        result=PASS
        if [ "$failures" -ne 0 ] || [ "$done" -ne 1 ] || [ "$user_failures" -ne 0 ] || [ "$user_done" -ne 1 ]; then
            result=FAIL
        fi
        printf "| %s | %s | %s | %s | %s | %s | %s |\n" \
            "$target" "$checks" "$failures" "$heartbeat" \
            "$user_checks" "$user_heartbeat" "$result"
    done
    printf "\n## 分类计数\n\n"
    printf "| 目标 | Task | IPC/Notify | Timer | Memory | MPU | 最终 phase |\n"
    printf "| --- | ---: | ---: | ---: | ---: | ---: | ---: |\n"
    tail -n +2 "$CSV" | while IFS=, read -r target checks failures done heartbeat task_checks ipc_checks timer_checks memory_checks mpu_checks user_checks user_failures user_done user_heartbeat phase; do
        printf "| %s | %s | %s | %s | %s | %s | %s |\n" \
            "$target" "$task_checks" "$ipc_checks" "$timer_checks" \
            "$memory_checks" "$mpu_checks" "$phase"
    done
    printf "\n## 通过准则\n\n"
    printf "%s\n" "- 每个目标的 \`boundary_done == 1\` 且 \`boundary_failures == 0\`。"
    printf "%s\n" "- 边界检查完成后，内核和用户态 heartbeat 均持续增长。"
    printf "%s\n" "- 每个目标至少执行 100 个内核边界检查。"
    printf "%s\n" "- 用户态 syscall wrapper 检查执行且 failure 为 0。"
} > "$REPORT_ZH"

printf "API boundary reports: %s, %s\n" "$REPORT" "$REPORT_ZH"
