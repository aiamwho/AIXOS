#!/bin/sh
set -u

OUT=${1:-build/reports/toolchain-report.txt}
mkdir -p "$(dirname "$OUT")"
: > "$OUT"

check() {
    name="$1"
    shift
    if command -v "$name" >/dev/null 2>&1; then
        path=$(command -v "$name")
        version=$("$name" "$@" 2>&1 | head -n 1)
        printf "%-28s OK       %s | %s\n" "$name" "$path" "$version" | tee -a "$OUT"
    else
        printf "%-28s MISSING\n" "$name" | tee -a "$OUT"
    fi
}

check arm-none-eabi-gcc --version
check arm-none-eabi-size --version
check riscv64-elf-gcc --version
check riscv64-unknown-elf-gcc --version
check riscv32-unknown-elf-gcc --version
check riscv-none-elf-gcc --version
if [ -n "${RISCV_TOOLCHAIN_DIR:-}" ]; then
    check "$RISCV_TOOLCHAIN_DIR/bin/riscv-none-elf-gcc" --version
fi
check renode --version
check renode-test --help
check qemu-system-arm --version
check qemu-system-riscv32 --version
check spike --version
check cc --version

printf "\nRequired now: arm-none-eabi-gcc, renode, cc\n" | tee -a "$OUT"
printf "Required for RISC-V execution: a RV32 compiler and Renode/QEMU platform model\n" | tee -a "$OUT"
