#!/usr/bin/env python3
import csv
import json
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "build" / "reports"

VARIANTS = [
    {
        "id": "bitmap",
        "label": "Bitmap multi-queue",
        "define": "AIXOS_CFG_SCHED_BITMAP",
        "complexity": "select O(1), add/remove O(1)",
    },
    {
        "id": "simple",
        "label": "Simple sorted ready list",
        "define": "AIXOS_CFG_SCHED_SIMPLE",
        "complexity": "select O(1), add/requeue O(N)",
    },
]


def run(cmd, cwd=ROOT, check=True):
    proc = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if check and proc.returncode != 0:
        raise RuntimeError(
            f"{' '.join(cmd)} failed\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )
    return proc


def parse_size(output):
    lines = [line.split() for line in output.splitlines() if line.strip()]
    for parts in lines:
        if len(parts) >= 6 and parts[-1].endswith(("AIXOS.elf", "sched.o")):
            text, data, bss = (int(parts[0]), int(parts[1]), int(parts[2]))
            return {
                "text": text,
                "data": data,
                "bss": bss,
                "total": text + data + bss,
            }
    raise ValueError(output)


def count_obj_instructions(path):
    proc = run(["arm-none-eabi-objdump", "-d", str(path)])
    count = 0
    by_function = {}
    current = None
    for line in proc.stdout.splitlines():
        match = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if match:
            current = match.group(1)
            by_function.setdefault(current, 0)
            continue
        if re.match(r"^\s+[0-9a-f]+:\s+[0-9a-f]{2,8}\s+", line):
            count += 1
            if current is not None:
                by_function[current] = by_function.get(current, 0) + 1
    return count, by_function


def benchmark_variant(variant):
    sched = f"-DAIXOS_CFG_SCHEDULER={variant['define']}"
    arm_build = f"build/arm-{variant['id']}"
    host_build = f"build/host-{variant['id']}"

    host = run([
        "make", "test",
        f"HOST_BUILD={host_build}",
        f"CONFIG_CFLAGS={sched}",
    ])
    checks = re.search(r"(\d+) checks, (\d+) failures", host.stdout)

    run([
        "make", "arm",
        f"ARM_BUILD={arm_build}",
        f"CONFIG_CFLAGS={sched}",
    ])

    elf = ROOT / arm_build / "AIXOS.elf"
    sched_obj = ROOT / arm_build / "kernel" / "sched.o"
    elf_size = parse_size(run(["arm-none-eabi-size", str(elf)]).stdout)
    sched_size = parse_size(run(["arm-none-eabi-size", str(sched_obj)]).stdout)
    instr_total, instr_by_function = count_obj_instructions(sched_obj)

    return {
        "id": variant["id"],
        "label": variant["label"],
        "define": variant["define"],
        "complexity": variant["complexity"],
        "host_checks": int(checks.group(1)) if checks else None,
        "host_failures": int(checks.group(2)) if checks else None,
        "elf": elf_size,
        "sched_object": sched_size,
        "sched_object_instructions": instr_total,
        "sched_function_instructions": instr_by_function,
    }


def main():
    if shutil.which("arm-none-eabi-gcc") is None:
        raise SystemExit("missing arm-none-eabi-gcc")
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    results = [benchmark_variant(v) for v in VARIANTS]

    json_path = REPORT_DIR / "scheduler_compare.json"
    csv_path = REPORT_DIR / "scheduler_compare.csv"
    json_path.write_text(json.dumps(results, indent=2), encoding="utf-8")
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "variant", "complexity", "host_checks", "host_failures",
            "elf_text", "elf_data", "elf_bss", "elf_total",
            "sched_text", "sched_data", "sched_bss", "sched_total",
            "sched_obj_instructions",
        ])
        for row in results:
            writer.writerow([
                row["label"], row["complexity"],
                row["host_checks"], row["host_failures"],
                row["elf"]["text"], row["elf"]["data"],
                row["elf"]["bss"], row["elf"]["total"],
                row["sched_object"]["text"], row["sched_object"]["data"],
                row["sched_object"]["bss"], row["sched_object"]["total"],
                row["sched_object_instructions"],
            ])
    print(json_path)
    print(csv_path)


if __name__ == "__main__":
    main()
