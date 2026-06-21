FREERTOS := ../../third_party/FreeRTOS-Kernel
BUILD := ../../build/freertos
ARM_PREFIX ?= arm-none-eabi-
RISCV_TOOLCHAIN_DIR ?= /private/tmp/xpack-riscv-none-elf-gcc-15.2.0-1
RISCV_PREFIX ?= riscv-none-elf-

ARM_CC := $(ARM_PREFIX)gcc
ARM_SIZE := $(ARM_PREFIX)size
RISCV_CC := $(RISCV_TOOLCHAIN_DIR)/bin/$(RISCV_PREFIX)gcc
RISCV_SIZE := $(RISCV_TOOLCHAIN_DIR)/bin/$(RISCV_PREFIX)size

KERNEL_SRCS := \
	$(FREERTOS)/tasks.c \
	$(FREERTOS)/list.c \
	$(FREERTOS)/queue.c \
	$(FREERTOS)/timers.c \
	$(FREERTOS)/portable/MemMang/heap_4.c
COMMON_SRCS := main.c ../../kernel/string.c $(KERNEL_SRCS)
COMMON_INCLUDES := -I. -Iinclude -I../.. -I$(FREERTOS)/include
COMMON_FLAGS := $(COMMON_INCLUDES) -std=c99 -Wall -Wextra -Werror \
	-Wno-unused-parameter -ffunction-sections -fdata-sections -ffreestanding -Os

ARM_PORT := $(FREERTOS)/portable/GCC/ARM_CM3
ARM_SRCS := $(COMMON_SRCS) $(ARM_PORT)/port.c arm_startup.s
ARM_FLAGS := $(COMMON_FLAGS) -I$(ARM_PORT) -mcpu=cortex-m3 -mthumb

RISCV_PORT := $(FREERTOS)/portable/GCC/RISC-V
RISCV_CHIP := $(RISCV_PORT)/chip_specific_extensions/RV32I_CLINT_no_extensions
RISCV_SRCS := $(COMMON_SRCS) $(RISCV_PORT)/port.c \
	$(RISCV_PORT)/portASM.S riscv_startup.s
RISCV_FLAGS := $(COMMON_FLAGS) -I$(RISCV_PORT) -I$(RISCV_CHIP) \
	-march=rv32im_zicsr_zifencei -mabi=ilp32 -mcmodel=medany \
	-msmall-data-limit=0

all: arm riscv

arm: $(BUILD)/arm/FreeRTOS.elf

riscv: $(BUILD)/riscv/FreeRTOS.elf

$(BUILD)/arm/FreeRTOS.elf: $(ARM_SRCS) arm_link.ld FreeRTOSConfig.h
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_FLAGS) -nostdlib -Wl,--gc-sections,-Map=$(@:.elf=.map) \
		-T arm_link.ld -o $@ $(ARM_SRCS) -lgcc
	$(ARM_SIZE) $@

$(BUILD)/riscv/FreeRTOS.elf: $(RISCV_SRCS) riscv_link.ld FreeRTOSConfig.h
	@mkdir -p $(dir $@)
	$(RISCV_CC) $(RISCV_FLAGS) -nostdlib \
		-Wl,--gc-sections,-Map=$(@:.elf=.map),--no-relax \
		-T riscv_link.ld -o $@ $(RISCV_SRCS) -lgcc
	$(RISCV_SIZE) $@

clean:
	rm -rf $(BUILD)

.PHONY: all arm riscv clean
