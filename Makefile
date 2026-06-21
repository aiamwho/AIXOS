TARGET := AIXOS
BUILD_DIR := build
APP_SRCS ?= examples/smoke/main.c

COMMON_SRCS := \
	kernel/object.c \
	kernel/isr.c \
	kernel/crash.c \
	kernel/sched.c \
	kernel/task.c \
	kernel/heap.c \
	kernel/mempool.c \
	kernel/mpu.c \
	kernel/timer.c \
	kernel/trace.c \
	kernel/string.c \
	kernel/microkernel.c \
	kernel/namespace.c \
	kernel/timewheel.c \
	kernel/ipc/sem.c \
	kernel/ipc/mutex.c \
	kernel/ipc/mq.c \
	kernel/ipc/event.c \
	kernel/ipc/pipe.c \
	kernel/ipc/notify.c \
	compat/posix/posix.c \
	posix/src/mqueue.c \
	posix/src/semaphore.c \
	posix/src/unistd.c

COMMON_INCLUDES := -I. -Iinclude -Iarch/include
COMMON_WARNINGS := -Wall -Wextra -Werror -Wshadow -Wno-unused-parameter
CONFIG_CFLAGS ?=
COMMON_CFLAGS := $(COMMON_INCLUDES) $(COMMON_WARNINGS) $(CONFIG_CFLAGS) -std=c99 \
	-ffunction-sections -fdata-sections -ffreestanding -MMD -MP

ARM_PREFIX ?= arm-none-eabi-
ARM_CC := $(ARM_PREFIX)gcc
ARM_AS := $(ARM_PREFIX)gcc
ARM_LD := $(ARM_PREFIX)ld
ARM_SIZE := $(ARM_PREFIX)size
ARM_MCU := -mcpu=cortex-m3 -mthumb
ARM_BUILD := $(BUILD_DIR)/arm
ARM_C_SRCS := $(COMMON_SRCS) arch/arm/cortex-m3/port.c $(APP_SRCS)
ARM_S_SRCS := arch/arm/cortex-m3/startup_stm32f103.s arch/arm/cortex-m3/portasm.s
ARM_OBJS := $(addprefix $(ARM_BUILD)/,$(ARM_C_SRCS:.c=.o) $(ARM_S_SRCS:.s=.o))
ARM_CFLAGS := $(COMMON_CFLAGS) $(ARM_MCU) -Os
ARM_ASFLAGS := $(ARM_MCU) -Wall
ARM_LDFLAGS := -EL --gc-sections -Map=$(ARM_BUILD)/$(TARGET).map \
	-T arch/arm/cortex-m3/link.ld
ARM_LIBGCC := $(shell $(ARM_CC) $(ARM_MCU) -print-libgcc-file-name 2>/dev/null)

RISCV_TOOLCHAIN_DIR ?=
RISCV_PREFIX ?= riscv-none-elf-
RISCV_BIN := $(if $(RISCV_TOOLCHAIN_DIR),$(RISCV_TOOLCHAIN_DIR)/bin/,)
RISCV_CC := $(RISCV_BIN)$(RISCV_PREFIX)gcc
RISCV_SIZE := $(RISCV_BIN)$(RISCV_PREFIX)size
RISCV_OBJDUMP := $(RISCV_BIN)$(RISCV_PREFIX)objdump
RISCV_READELF := $(RISCV_BIN)$(RISCV_PREFIX)readelf
RISCV_BUILD := $(BUILD_DIR)/riscv
RISCV_C_SRCS := $(COMMON_SRCS) arch/risc-v/port.c $(APP_SRCS)
RISCV_S_SRCS := arch/risc-v/startup.s arch/risc-v/portasm.s
RISCV_OBJS := $(addprefix $(RISCV_BUILD)/,$(RISCV_C_SRCS:.c=.o) $(RISCV_S_SRCS:.s=.o))
RISCV_CFLAGS := $(COMMON_CFLAGS) -march=rv32im_zicsr_zifencei \
	-mabi=ilp32 -mcmodel=medany -msmall-data-limit=0 -Os
RISCV_LDFLAGS := -nostdlib -Wl,--gc-sections,-Map=$(RISCV_BUILD)/$(TARGET).map \
	-Wl,--no-relax -T arch/risc-v/link.ld
RENODE_USER_LOCK ?= $(HOME)/Library/Application Support/renode/renode.config.lock
RENODE_CONFIG ?= $(CURDIR)/tools/renode.config

HOST_CC ?= cc
HOST_LDFLAGS ?=
HOST_BUILD ?= $(BUILD_DIR)/host
HOST_OPT ?= -O0 -g3
HOST_SANITIZERS ?=
HOST_TEST_SRCS := tests/test_main.c tests/test_object.c tests/test_heap.c \
	tests/test_kernel.c tests/test_reliability.c tests/test_stress.c \
	tests/test_microkernel.c \
	tests/test_mpu.c \
	tests/test_posix.c tests/test_posix_public.c \
	tests/host/arch_host.c
HOST_KERNEL_SRCS := $(COMMON_SRCS)
HOST_OBJS := $(addprefix $(HOST_BUILD)/,$(HOST_KERNEL_SRCS:.c=.o) $(HOST_TEST_SRCS:.c=.o))
HOST_MPU_TEST_SRCS := tests/test_mpu_main.c tests/test_mpu.c tests/host/arch_host.c
HOST_MPU_OBJS := $(addprefix $(BUILD_DIR)/host-mpu/,$(HOST_KERNEL_SRCS:.c=.o) $(HOST_MPU_TEST_SRCS:.c=.o))
HOST_CFLAGS := -Itests/host $(COMMON_INCLUDES) $(COMMON_WARNINGS) \
	$(CONFIG_CFLAGS) -std=c99 \
	-Wno-unneeded-internal-declaration $(HOST_OPT) $(HOST_SANITIZERS) \
	-DAIXOS_HOST_TEST=1 -MMD -MP

all: arm

arm: $(ARM_BUILD)/$(TARGET).elf

$(ARM_BUILD)/$(TARGET).elf: $(ARM_OBJS) arch/arm/cortex-m3/link.ld
	@mkdir -p $(dir $@)
	$(ARM_LD) $(ARM_LDFLAGS) -o $@ $(ARM_OBJS) $(ARM_LIBGCC)
	$(ARM_SIZE) $@

$(ARM_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c -o $@ $<

$(ARM_BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(ARM_AS) $(ARM_ASFLAGS) -c -o $@ $<

riscv: check-riscv-toolchain $(RISCV_BUILD)/$(TARGET).elf

check-riscv-toolchain:
	@command -v $(RISCV_CC) >/dev/null 2>&1 || test -x "$(RISCV_CC)" || { \
		echo "missing RISC-V compiler: $(RISCV_CC)"; exit 2; }

$(RISCV_BUILD)/$(TARGET).elf: $(RISCV_OBJS) arch/risc-v/link.ld
	@mkdir -p $(dir $@)
	$(RISCV_CC) $(RISCV_CFLAGS) $(RISCV_LDFLAGS) -o $@ $(RISCV_OBJS) -lgcc
	$(RISCV_SIZE) $@

riscv-disasm: riscv
	$(RISCV_OBJDUMP) -d -S $(RISCV_BUILD)/$(TARGET).elf \
		> $(RISCV_BUILD)/$(TARGET).dis

riscv-validate: riscv
	@$(RISCV_READELF) -h $(RISCV_BUILD)/$(TARGET).elf | \
		grep -q 'Class:.*ELF32'
	@$(RISCV_READELF) -h $(RISCV_BUILD)/$(TARGET).elf | \
		grep -q 'Machine:.*RISC-V'
	@$(RISCV_READELF) -s $(RISCV_BUILD)/$(TARGET).elf | \
		grep -q ' trap_handler'
	@$(RISCV_READELF) -s $(RISCV_BUILD)/$(TARGET).elf | \
		grep -q ' test_heartbeat'

$(RISCV_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(RISCV_CC) $(RISCV_CFLAGS) -c -o $@ $<

$(RISCV_BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(RISCV_CC) $(RISCV_CFLAGS) -c -o $@ $<

test: $(HOST_BUILD)/aixos_tests
	$(HOST_BUILD)/aixos_tests

test-mpu: $(BUILD_DIR)/host-mpu/aixos_mpu_tests
	$(BUILD_DIR)/host-mpu/aixos_mpu_tests

test-asan:
	$(MAKE) test HOST_BUILD=$(BUILD_DIR)/host-asan \
		HOST_OPT="-O1 -g3" \
		HOST_SANITIZERS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
		HOST_LDFLAGS="-fsanitize=address,undefined"

test-o2:
	$(MAKE) test HOST_BUILD=$(BUILD_DIR)/host-o2 HOST_OPT="-O2 -g"

test-os:
	$(MAKE) test HOST_BUILD=$(BUILD_DIR)/host-os HOST_OPT="-Os -g"

coverage:
	$(MAKE) $(BUILD_DIR)/host-coverage/aixos_tests \
		HOST_CC=clang HOST_BUILD=$(BUILD_DIR)/host-coverage \
		HOST_OPT="-O0 -g" \
		HOST_SANITIZERS="-fprofile-instr-generate -fcoverage-mapping" \
		HOST_LDFLAGS="-fprofile-instr-generate -fcoverage-mapping"
	LLVM_PROFILE_FILE=$(BUILD_DIR)/host-coverage/aixos.profraw \
		$(BUILD_DIR)/host-coverage/aixos_tests
	xcrun llvm-profdata merge -sparse \
		$(BUILD_DIR)/host-coverage/aixos.profraw \
		-o $(BUILD_DIR)/host-coverage/aixos.profdata
	xcrun llvm-cov report $(BUILD_DIR)/host-coverage/aixos_tests \
		-instr-profile=$(BUILD_DIR)/host-coverage/aixos.profdata \
		kernel compat/posix posix/src tests \
		> $(BUILD_DIR)/host-coverage/coverage.txt
	@tail -n 2 $(BUILD_DIR)/host-coverage/coverage.txt

analyze:
	@sh tools/static_analysis.sh

posix-api-check: check-riscv-toolchain
	$(HOST_CC) -Iposix/include $(filter-out -MMD -MP,$(COMMON_CFLAGS)) \
		-fsyntax-only \
		tests/posix_api_compile.c
	$(ARM_CC) -Iposix/include $(filter-out -MMD -MP,$(ARM_CFLAGS)) \
		-fsyntax-only \
		tests/posix_api_compile.c
	$(RISCV_CC) -Iposix/include $(filter-out -MMD -MP,$(RISCV_CFLAGS)) \
		-fsyntax-only \
		tests/posix_api_compile.c

ram-report: arm riscv
	@ARM_SIZE=$(ARM_SIZE) RISCV_SIZE=$(RISCV_SIZE) \
		sh tools/ram_report.sh

manifest: arm riscv
	@HOST_CC=$(HOST_CC) ARM_CC=$(ARM_CC) RISCV_CC=$(RISCV_CC) \
		node tools/build_manifest.mjs

evidence-package:
	@RISCV_PREFIX=$(RISCV_PREFIX) sh tools/evidence_package.sh

reproducible:
	@rm -rf $(BUILD_DIR)/repro-arm-a $(BUILD_DIR)/repro-arm-b
	$(MAKE) arm ARM_BUILD=$(BUILD_DIR)/repro-arm-a
	$(MAKE) arm ARM_BUILD=$(BUILD_DIR)/repro-arm-b
	@cmp $(BUILD_DIR)/repro-arm-a/$(TARGET).elf \
		$(BUILD_DIR)/repro-arm-b/$(TARGET).elf
	@echo "Cortex-M3 reproducible build: PASS"

verify: test test-asan test-o2 test-os coverage analyze posix-api-check arm \
	riscv-validate

quality: verify ram-report manifest reproducible

$(HOST_BUILD)/aixos_tests: $(HOST_OBJS)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^

$(BUILD_DIR)/host-mpu/aixos_mpu_tests: $(HOST_MPU_OBJS)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^

$(HOST_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c -o $@ $<

$(HOST_BUILD)/tests/test_posix_public.o: tests/test_posix_public.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -Iposix/include -c -o $@ $<

$(BUILD_DIR)/host-mpu/%.o: %.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c -o $@ $<

toolcheck:
	@sh tools/check_tools.sh

config:
	@python3 tools/menu_config.py

oldconfig:
	@python3 tools/menu_config.py --show

renode: arm
	@mkdir -p test-results/renode
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	renode-test --renode-config $(RENODE_CONFIG) \
		--results-dir $(CURDIR)/test-results/renode \
		tests/renode_cortexm3.robot

renode-riscv: riscv-validate
	@mkdir -p test-results/renode-riscv
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	renode-test --renode-config $(RENODE_CONFIG) \
		--results-dir $(CURDIR)/test-results/renode-riscv \
		tests/renode_riscv.robot

renode-riscv-stress: riscv-validate
	@mkdir -p test-results/renode-riscv-stress
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	renode-test --repeat 5 --renode-config $(RENODE_CONFIG) \
		--results-dir $(CURDIR)/test-results/renode-riscv-stress \
		tests/renode_riscv.robot

test-all: test arm renode riscv-validate renode-riscv

bench-build:
	$(MAKE) -C benchmarks/aixos all
	$(MAKE) -C benchmarks/freertos all

latency-bench:
	$(MAKE) -C benchmarks/latency all RISCV_PREFIX=$(RISCV_PREFIX)

instruction-sim: bench-build
	@sh tools/run_instruction_simulation.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all arm riscv riscv-disasm riscv-validate check-riscv-toolchain \
	test test-mpu test-asan test-o2 test-os verify toolcheck renode renode-riscv \
	renode-riscv-stress test-all coverage analyze ram-report manifest \
	posix-api-check reproducible quality evidence-package bench-build \
	latency-bench instruction-sim config oldconfig clean

-include $(ARM_OBJS:.o=.d) $(RISCV_OBJS:.o=.d) $(HOST_OBJS:.o=.d) \
	$(HOST_MPU_OBJS:.o=.d)
