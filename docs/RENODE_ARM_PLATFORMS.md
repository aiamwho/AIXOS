# Renode ARM Cortex Platform Matrix

This package carries Renode platform descriptions for ARM instruction-level
bring-up coverage. Cortex-M0, Cortex-M3, Cortex-M4, and Cortex-M33 are
selectable AIXOS Cortex-M targets with Renode smoke coverage. Cortex-A55 has an
AArch64 source, linked ELF output, Renode platform load coverage, and bootable
Renode smoke coverage.

| Cortex core | Renode platform | Renode CPU model | Current AIXOS status |
|---|---|---|---|
| Cortex-M0 | `simulation/cortex_m0.repl` | `CPU.CortexM`, `cortex-m0` | Supported with an ARMv6-M PRIMASK context-switch path. |
| Cortex-M3 | `simulation/stm32f103.repl` | `CPU.CortexM`, `cortex-m3` | Supported and used by default. |
| Cortex-M4 | `simulation/cortex_m4.repl` | `CPU.CortexM`, `cortex-m4` | Supported through the Cortex-M3-class BASEPRI path. |
| Cortex-M33 | `simulation/cortex_m33.repl` | `CPU.CortexM`, `cortex-m33` | Supported for baseline Renode smoke; MPU/fault-status handling is disabled on this Renode model. |
| Cortex-A55 | `simulation/cortex_a55.repl` | `CPU.ARMv8A`, `cortex-a55` | Supported with `clang --target=aarch64-none-elf`, `ld.lld`, GICv3/generic-timer setup, heartbeat, ticks, user task heartbeat, and zero user syscall errors in Renode smoke. |

## Commands

Load-check every ARM Cortex Renode platform:

```sh
make renode-arm-platform-check
```

Run the ARM smoke tests:

```sh
make renode-arm-smoke
```

`make renode-arm-platforms` runs both checks.

Build the Cortex-A55 AArch64 object set:

```sh
make arm AIXOS_PLATFORM=cortex-a55
```

Run the Cortex-A55 instruction-level smoke:

```sh
make instruction-sim
```
