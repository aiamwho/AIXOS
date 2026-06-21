# AIXOS v1.0

AIXOS v1.0 is an embedded RTOS kernel package for Cortex-M3, RV32IM, and host
test builds. The package includes the kernel source, public headers,
configuration, architecture ports, POSIX compatibility layer, tests,
simulation files, benchmark inputs, and customer-facing development
documentation.

This package is intended for source-level integration. Generated build output,
internal evolution reports, competitive analysis, and presentation material are
not part of the customer deliverable.

## Package Contents

| Path | Purpose |
|---|---|
| `include/aixos/` | Public AIXOS headers and ABI-visible types |
| `kernel/` | Scheduler, task, heap, timer, trace, crash, object, IPC, and microkernel code |
| `arch/` | Cortex-M3, RV32IM, and shared architecture interfaces |
| `config/` | Build-time configuration defaults |
| `compat/posix/`, `posix/` | POSIX compatibility implementation and public POSIX headers |
| `examples/hello_world/` | Minimal first task and UART hook example |
| `examples/smoke/` | Default smoke-test firmware entry used by `make arm` and `make riscv` |
| `tests/` | Host and Renode regression tests |
| `simulation/` | Renode platform and smoke-test scripts |
| `benchmarks/` | AIXOS and FreeRTOS benchmark input projects |
| `tools/` | Tool checks, static analysis, manifest, RAM report, Renode config, and RISC-V toolchain installer |
| `docs/` | Customer development, architecture, configuration, porting, and release notes |

## Public Version

| Item | Value |
|---|---|
| Product version | `1.0.0` |
| `AIXOS_API_VERSION` | `0x00010000` |
| `AIXOS_ABI_VERSION` | `0x00010000` |
| Crash record version | `5` |
| Author contact | `csulxx@gmail.com` |

Applications should include `aixos/aixos.h` for the full public API, or include
individual headers when a smaller dependency surface is preferred.

## Supported Build Targets

| Target | Command | Notes |
|---|---|---|
| Host tests | `make test` | Builds and runs `build/host/aixos_tests` |
| Cortex-M3 firmware | `make arm` | Uses `arm-none-eabi-gcc` and `arch/arm/cortex-m3/` |
| RV32IM firmware | `make riscv` | Uses `riscv-none-elf-gcc`, `RISCV_PREFIX`, or `RISCV_TOOLCHAIN_DIR` |
| Cortex-M3 Renode | `make renode` | Builds ARM firmware and runs `tests/renode_cortexm3.robot` |
| RV32IM Renode | `make renode-riscv` | Builds, validates, and simulates the RISC-V image |
| Static analysis | `make analyze` | Runs Clang static analysis through `tools/static_analysis.sh` |
| Release quality sweep | `make quality` | Runs the full configured verification matrix |

The firmware targets build `APP_SRCS ?= examples/smoke/main.c` by default.
Customer projects can provide their own application entry without editing the
package:

```sh
make arm APP_SRCS=/path/to/customer/main.c
make riscv APP_SRCS=/path/to/customer/main.c
```

For the minimal Hello World example:

```sh
make arm APP_SRCS=examples/hello_world/main.c
make riscv APP_SRCS=examples/hello_world/main.c RISCV_PREFIX=riscv64-elf-
```

On Apple Silicon, a pinned RISC-V toolchain can be installed with:

```sh
sh tools/install_riscv_toolchain.sh
```

Then pass the install path when building RISC-V:

```sh
make riscv RISCV_TOOLCHAIN_DIR=/path/to/xpack-riscv-none-elf-gcc
make renode-riscv RISCV_TOOLCHAIN_DIR=/path/to/xpack-riscv-none-elf-gcc
```

If the installed toolchain uses another prefix, pass it explicitly:

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

## Integration Sequence

1. Review `config/aixos_cfg.h` and set memory, object, timer, trace, POSIX, and
   architecture limits for the target product.
2. Include `include/`, `config/`, `kernel/`, `arch/include/`, the selected
   `arch/<target>/` port, and any required compatibility layer in the customer
   build system.
3. Provide the linker script and startup flow for the selected target.
4. Initialize the object and scheduler subsystems, then create system and
   application tasks before starting the scheduler.
5. Run the host, cross-build, Renode, and static-analysis checks listed in
   `TESTING.md` before customer firmware qualification.

## Documentation

- [API contract](API_CONTRACT.md)
- [API reference](docs/API_REFERENCE.md)
- [Hello World](docs/HELLO_WORLD.md)
- [Integration guide](docs/INTEGRATION_GUIDE.md)
- [Requirements traceability](docs/REQUIREMENTS_TRACEABILITY.md)
- [POSIX compatibility](docs/POSIX_COMPATIBILITY.md)
- [Testing and verification](TESTING.md)
- [Architecture guide](docs/ARCHITECTURE.md)
- [Development guide](docs/DEVELOPMENT_GUIDE.md)
- [Configuration guide](docs/CONFIGURATION.md)
- [Porting guide](docs/PORTING_GUIDE.md)
- [Troubleshooting guide](docs/TROUBLESHOOTING.md)
- [Release notes](docs/RELEASE_NOTES.md)

## Release Status

This package is a v1.0 source release for customer evaluation and integration.
Production use requires product-specific hardware validation,
worst-case execution-time evidence, long-run stability testing, and safety or
security assessment appropriate to the target device.
