# AIXOS v1.0 Release Notes

Date: 2026-06-29

## Release Type

AIXOS v1.0 is a customer source release. It is prepared as a source package
with customer-facing documentation and without internal process reports or
generated presentation material.

## Baseline Provenance

This v1.0 package was initialized from the qualified v0.9 source package and
then advanced to the v1.0 version, API, and ABI identifiers. It preserves the
evidence, traceability, POSIX boundary clarity, latency benchmarking, and trace
visualization work prepared in the v0.9 baseline.

## Included

- Public headers under `include/aixos/`.
- Kernel source under `kernel/`.
- Cortex-M3 and RV32IM architecture ports.
- Host test architecture shim.
- POSIX compatibility headers and implementation.
- Renode simulation scripts.
- Benchmark input projects.
- Build, analysis, manifest, RAM report, and tool-check scripts.
- Customer documentation under `docs/`.

## Added in This Cut

- `make evidence-package` for auditable release evidence collection.
- `make config` terminal menu for customer-facing kernel裁剪.
- Selectable scheduler backend: default bitmap multi-queue or simple sorted
  ready list for small-system footprint optimization.
- Configurable Cortex-M3 kernel IRQ priority threshold for high-response ISR
  deployments.
- Atomic ISR nesting accounting with high-watermark diagnostics, configurable
  nesting budget, and crash-record overflow reporting.
- `tools/scheduler_benchmark.py` for Cortex-M3 scheduler size/instruction
  comparison.
- Requirement-to-test traceability matrix.
- POSIX compatibility and limitation matrix.
- Latency benchmark firmware entry for Cortex-M3 and RV32IM.
- Trace viewer tool that converts trace JSON to CSV or HTML.
- Benchmark and latency applications now initialize enabled namespace and
  timing-wheel subsystems before scheduler start, matching the smoke and Hello
  World application sequence and preventing timeout-list corruption during
  instruction-level simulation.
- Timing-wheel level-2 slot placement and cascade handling were corrected after
  white-box path tests exposed late wakeups for far-future timeouts.
- Host white-box and black-box regression coverage was expanded with
  namespace/resource-manager paths, timing-wheel level/cascade paths,
  microkernel synchronous IPC paths, public API invalid-parameter matrices, and
  repeated user workflow scenarios. The 2026-06-29 host suite now reports 23
  named tests and `7207 checks, 0 failures`.
- Runtime API boundary simulation now runs on Cortex-M3 and RV32IM Renode with
  929 boundary checks per target and 0 failures.
- Local 2026-06-29 verification passed `make quality`, MPU focused tests,
  latency benchmark build, ARM Cortex Renode platform matrix, RV32IM Renode
  stress, AIXOS instruction benchmark collection, API boundary simulation, and
  evidence-package generation.

## Baseline Contents

- Per-user-task memory protection metadata and validation.
- Public MPU region API in `include/aixos/mpu.h`.
- Cortex-M3 MPU programming on task switch.
- RV32 PMP programming policy on task switch.
- Default user stack protection and explicit user buffer grants.
- Host regression coverage for MPU region validation and software access
  checks.
- Customer integration guide, API reference, troubleshooting guide, and
  development handoff checklist.
- Minimal Hello World example with board UART hook documentation.

## Removed From Customer Package

- Internal work reports and historical migration notes.
- QNX, FreeRTOS, and CPU/SOC comparison reports.
- Generated PPTX/DOCX outputs.
- Build output and analysis output.
- Machine-local toolchain inventory.
- Report-generation scripts that are not needed for customer integration.

## Version Identifiers

| Identifier | Value |
|---|---|
| `AIXOS_VERSION_MAJOR` | `1` |
| `AIXOS_VERSION_MINOR` | `0` |
| `AIXOS_VERSION_PATCH` | `0` |
| `AIXOS_API_VERSION` | `0x00010000` |
| `AIXOS_ABI_VERSION` | `0x00010000` |

## Known Follow-Up Items

- Rerun full `make quality` in the customer integration environment.
- Rerun `make renode-arm-platforms`, `make renode-riscv-stress`, and
  `make instruction-bench` when toolchains, Renode, platform files, or
  benchmark workloads change.
- Complete product-specific hardware-in-the-loop validation.
- Validate MPU/PMP fault behavior on target hardware and simulator models.
- Measure worst-case interrupt latency, context-switch latency, timer callback
  latency, and memory high-water marks on the customer target.
- Archive qualified ELF, map, compiler identity, configuration, source
  manifest, and test logs for each customer release.

## Compatibility Notes

Applications should treat v1.0 as a new API/ABI baseline. Any customer code
compiled against earlier internal snapshots should be rebuilt and retested
against this package.
