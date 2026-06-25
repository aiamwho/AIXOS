ROOT := ../..
BUILD := $(ROOT)/build/benchmarks/latency

COMMON_SRCS := \
	$(ROOT)/kernel/object.c \
	$(ROOT)/kernel/isr.c \
	$(ROOT)/kernel/crash.c \
	$(ROOT)/kernel/sched.c \
	$(ROOT)/kernel/task.c \
	$(ROOT)/kernel/heap.c \
	$(ROOT)/kernel/mempool.c \
	$(ROOT)/kernel/mpu.c \
	$(ROOT)/kernel/timer.c \
	$(ROOT)/kernel/trace.c \
	$(ROOT)/kernel/string.c \
	$(ROOT)/kernel/microkernel.c \
	$(ROOT)/kernel/namespace.c \
	$(ROOT)/kernel/timewheel.c \
	$(ROOT)/kernel/ipc/sem.c \
	$(ROOT)/kernel/ipc/mutex.c \
	$(ROOT)/kernel/ipc/mq.c \
	$(ROOT)/kernel/ipc/event.c \
	$(ROOT)/kernel/ipc/pipe.c \
	$(ROOT)/kernel/ipc/notify.c \
	$(ROOT)/compat/posix/posix.c \
	$(ROOT)/posix/src/mqueue.c \
	$(ROOT)/posix/src/semaphore.c \
	$(ROOT)/posix/src/unistd.c

INCLUDES := -I$(ROOT) -I$(ROOT)/include -I$(ROOT)/arch/include
CFLAGS := $(INCLUDES) -std=c99 -Wall -Wextra -Werror -Wshadow \
	-Wno-unused-parameter -ffunction-sections -fdata-sections -ffreestanding \
	-Os

ARM_PREFIX ?= arm-none-eabi-
ARM_CC := $(ARM_PREFIX)gcc
ARM_SIZE := $(ARM_PREFIX)size

RISCV_TOOLCHAIN_DIR ?=
RISCV_PREFIX ?= $(if $(shell command -v riscv-none-elf-gcc 2>/dev/null),riscv-none-elf-,$(if $(shell command -v riscv64-elf-gcc 2>/dev/null),riscv64-elf-,riscv-none-elf-))
RISCV_BIN := $(if $(RISCV_TOOLCHAIN_DIR),$(RISCV_TOOLCHAIN_DIR)/bin/,)
RISCV_CC := $(RISCV_BIN)$(RISCV_PREFIX)gcc
RISCV_SIZE := $(RISCV_BIN)$(RISCV_PREFIX)size

ARM_C_SRCS := $(COMMON_SRCS) main.c \
	$(ROOT)/arch/arm/cortex-m3/port.c
ARM_S_SRCS := \
	$(ROOT)/arch/arm/cortex-m3/startup_stm32f103.s \
	$(ROOT)/arch/arm/cortex-m3/portasm.s
ARM_SRCS := $(ARM_C_SRCS) $(ARM_S_SRCS)
ARM_AS_OBJS := \
	$(BUILD)/arm/startup_stm32f103.o \
	$(BUILD)/arm/portasm.o

RISCV_SRCS := $(COMMON_SRCS) main.c \
	$(ROOT)/arch/risc-v/port.c \
	$(ROOT)/arch/risc-v/startup.s \
	$(ROOT)/arch/risc-v/portasm.s

all: arm riscv

arm: $(BUILD)/arm/AIXOS-latency.elf

riscv: $(BUILD)/riscv/AIXOS-latency.elf

$(BUILD)/arm/AIXOS-latency.elf: $(ARM_SRCS)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(CFLAGS) -mcpu=cortex-m3 -mthumb -x assembler-with-cpp \
		-c -o $(BUILD)/arm/startup_stm32f103.o \
		$(ROOT)/arch/arm/cortex-m3/startup_stm32f103.s
	$(ARM_CC) $(CFLAGS) -mcpu=cortex-m3 -mthumb -x assembler-with-cpp \
		-c -o $(BUILD)/arm/portasm.o \
		$(ROOT)/arch/arm/cortex-m3/portasm.s
	$(ARM_CC) $(CFLAGS) -mcpu=cortex-m3 -mthumb -nostdlib \
		-Wl,--gc-sections,-Map=$(@:.elf=.map) \
		-T $(ROOT)/arch/arm/cortex-m3/link.ld -o $@ \
		$(ARM_C_SRCS) $(ARM_AS_OBJS) -lgcc
	$(ARM_SIZE) $@

$(BUILD)/riscv/AIXOS-latency.elf: $(RISCV_SRCS)
	@mkdir -p $(dir $@)
	$(RISCV_CC) $(CFLAGS) -march=rv32im_zicsr_zifencei -mabi=ilp32 \
		-mcmodel=medany -msmall-data-limit=0 -nostdlib \
		-Wl,--gc-sections,-Map=$(@:.elf=.map),--no-relax \
		-T $(ROOT)/arch/risc-v/link.ld -o $@ $(RISCV_SRCS) -lgcc
	$(RISCV_SIZE) $@

.PHONY: all arm riscv
