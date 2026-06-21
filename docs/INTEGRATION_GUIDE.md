# AIXOS v1.0 Integration Guide

This guide is the customer-side procedure for integrating AIXOS into product
firmware. It assumes the customer owns the board startup files, product linker
script, device drivers, and application tasks.

## Integration Scope

Use AIXOS as a source package. Do not copy generated build directories,
Renode result directories, or local tool reports into the product repository.

Minimum source set:

| Path | Required when |
|---|---|
| `include/aixos/` | Always |
| `config/aixos_cfg.h` | Always, or replace with product-specific equivalent |
| `kernel/` | Always |
| `kernel/ipc/` | Always when IPC APIs are used |
| `arch/include/` | Always |
| `arch/arm/cortex-m3/` | Cortex-M3 target |
| `arch/risc-v/` | RV32IM target |
| `compat/posix/`, `posix/` | POSIX compatibility is required |
| `examples/hello_world/` | Optional minimal first task and UART hook example |
| `examples/smoke/` | Optional smoke application |
| `tests/`, `simulation/` | Qualification and regression only |

## Toolchain Setup

Run the tool inventory first:

```sh
make toolcheck
```

The default RISC-V compiler prefix is `riscv-none-elf-`. If the installed
compiler uses another prefix, pass it explicitly:

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make riscv-validate RISCV_PREFIX=riscv64-elf-
```

If the compiler is installed outside `PATH`, use `RISCV_TOOLCHAIN_DIR`:

```sh
make riscv RISCV_TOOLCHAIN_DIR=/opt/xpack-riscv-none-elf-gcc
```

## Application Entry

The package Makefile builds `APP_SRCS ?= examples/smoke/main.c` by default.
Product firmware should provide its own application entry:

```sh
make arm APP_SRCS=product/main.c
make riscv APP_SRCS=product/main.c RISCV_PREFIX=riscv64-elf-
```

Start with the minimal UART example when bringing up a new board:

```sh
make arm APP_SRCS=examples/hello_world/main.c
```

See `docs/HELLO_WORLD.md` for the weak UART hook contract.

The application entry must initialize the kernel before starting the scheduler:

```c
#include "aixos/aixos.h"

static void app_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* Product work. */
        aixos_task_sleep(1000);
    }
}

int main(void)
{
    static aixos_tcb_t app_tcb;
    static uint8_t app_stack[512] __attribute__((aligned(8)));

    aixos_object_init();
    aixos_sched_init();
    aixos_heap_init(product_heap, product_heap_size);
    aixos_task_init();
    aixos_timer_init();

    (void)aixos_task_create_static("app", app_task, NULL,
                                   app_stack, sizeof(app_stack),
                                   10, &app_tcb);

    aixos_start();
}
```

Use the target startup code to call `main()` after data/BSS initialization.

## Static and Dynamic Objects

For production firmware, prefer static creation and fixed-block pools. This
keeps runtime behavior deterministic and allows `AIXOS_CFG_HEAP_LOCK_ON_START`
to remain enabled.

Use dynamic creation during initialization when the product resource model is
stable:

```c
aixos_handle_t sem = aixos_sem_create(0);
aixos_handle_t task = aixos_task_create("worker", worker_entry, NULL, 512, 12);
```

Use static creation for long-lived application objects:

```c
static aixos_tcb_t worker_tcb;
static uint8_t worker_stack[512] __attribute__((aligned(8)));

aixos_handle_t worker = aixos_task_create_static("worker", worker_entry, NULL,
                                                 worker_stack,
                                                 sizeof(worker_stack),
                                                 12, &worker_tcb);
```

Static storage must remain valid until the object is deleted or the system
resets.

## MPU User Tasks

Create user tasks when the product needs memory isolation between application
code and kernel-owned objects:

```c
static aixos_tcb_t user_tcb;
static uint8_t user_stack[512] __attribute__((aligned(512)));
static uint8_t user_buffer[256] __attribute__((aligned(256)));

aixos_handle_t user = aixos_user_task_create_static("user", user_entry, NULL,
                                                    user_stack,
                                                    sizeof(user_stack),
                                                    20, &user_tcb);

aixos_task_mpu_region_add(user, (uintptr_t)user_buffer, sizeof(user_buffer),
                          AIXOS_MPU_READ | AIXOS_MPU_WRITE);
```

Region rules:

- Base address must be aligned to region size.
- Size must be a power of two and at least `AIXOS_CFG_MPU_MIN_REGION_SIZE`.
- Writable regions must also be readable.
- Do not grant user access to kernel TCBs, object pools, allocator metadata,
  kernel stacks, or device registers unless the product security model allows
  it and the port supports the required attributes.

Kernel integrations that add syscall handlers must copy user buffers through
`aixos_copy_from_user()`, `aixos_copy_to_user()`, or `aixos_zero_to_user()`.
Direct pointer dereference is reserved for kernel-owned storage.

## Board Port Integration

For a supported architecture, the customer board integration normally provides:

- Reset vector and startup file.
- Memory map and linker script.
- Clock initialization before the kernel tick starts.
- Interrupt vector table or trap vector placement.
- Product heap storage.
- Device drivers and ISR registration.
- Optional board-specific fault export path.

For a new architecture or board model, follow `docs/PORTING_GUIDE.md`.

## Configuration Workflow

Start from `config/aixos_cfg.h`, then size the product:

1. Count worst-case task, semaphore, mutex, queue, pipe, event, and timer
   objects.
2. Reserve slots for idle and timer service tasks.
3. Set stack sizes from measured high-water marks, not only estimates.
4. Keep ISR copy limits small enough for latency requirements.
5. Size the heap for initialization-time dynamic objects and controlled
   kernel exceptions.
6. Choose trace and crash settings according to the product diagnostic policy.
7. Rebuild and run the verification matrix in `TESTING.md`.

Do not change `AIXOS_CFG_TASK_HANDLE_LIMIT` from `256` in v1.0 because the
public handle format uses an 8-bit slot index.

## ISR Integration

Only call APIs documented as `*_from_isr` or explicitly ISR-safe from interrupt
context. ISR paths must not block.

Typical ISR handoff:

```c
void UART_IRQHandler(void)
{
    uint8_t byte = read_uart_byte();
    (void)aixos_pipe_write_from_isr(rx_pipe, &byte, 1);
}
```

Move parsing, allocation, and blocking waits into task context.

## Diagnostics

During integration, enable trace and crash record export:

```c
aixos_trace_info_t info;
aixos_trace_get_info(&info);

const aixos_crash_record_t *crash = aixos_crash_record_get();
if (crash != NULL && aixos_crash_record_validate(crash) == AIXOS_OK) {
    product_store_crash_record(crash);
}
```

Archive the qualified ELF, map file, `config/aixos_cfg.h`, compiler identity,
source manifest, and test logs for each customer release.

## Release Qualification

Before customer release, run at least:

```sh
make test
make test-mpu
make test-asan
make test-o2
make test-os
make analyze
make posix-api-check RISCV_PREFIX=riscv64-elf-
make arm
make riscv-validate RISCV_PREFIX=riscv64-elf-
make renode
make coverage
make ram-report RISCV_PREFIX=riscv64-elf-
make manifest RISCV_PREFIX=riscv64-elf-
```

Run `make clean` before packaging source-only customer deliverables.
