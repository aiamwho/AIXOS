TARGET := AIXOS
BUILD_DIR := build
APP_SRCS ?= examples/smoke/main.c
CONFIG_MK := config/aixos_user_cfg.mk

-include $(CONFIG_MK)

AIXOS_PLATFORM ?= cortex-m3

ARM_PLATFORMS := cortex-m0 cortex-m3 cortex-m4 cortex-m33 cortex-a55
ARM_BOOT_PLATFORMS := cortex-m0 cortex-m3 cortex-m4 cortex-m33
ARM_CPU_cortex-m0 := cortex-m0
ARM_CPU_cortex-m3 := cortex-m3
ARM_CPU_cortex-m4 := cortex-m4
ARM_CPU_cortex-m33 := cortex-m33
ARM_CPU_cortex-a55 := cortex-a55
ARM_RENODE_PLATFORM_cortex-m0 := simulation/cortex_m0.repl
ARM_RENODE_PLATFORM_cortex-m3 := simulation/stm32f103.repl
ARM_RENODE_PLATFORM_cortex-m4 := simulation/cortex_m4.repl
ARM_RENODE_PLATFORM_cortex-m33 := simulation/cortex_m33.repl
ARM_RENODE_PLATFORM_cortex-a55 := simulation/cortex_a55.repl
ARM_PLATFORM_DEFINE_cortex-m0 := AIXOS_CFG_PLATFORM_CORTEX_M0
ARM_PLATFORM_DEFINE_cortex-m3 := AIXOS_CFG_PLATFORM_CORTEX_M3
ARM_PLATFORM_DEFINE_cortex-m4 := AIXOS_CFG_PLATFORM_CORTEX_M4
ARM_PLATFORM_DEFINE_cortex-m33 := AIXOS_CFG_PLATFORM_CORTEX_M33
ARM_PLATFORM_DEFINE_cortex-a55 := AIXOS_CFG_PLATFORM_CORTEX_A55

ARM_CPU := $(ARM_CPU_$(AIXOS_PLATFORM))
ARM_RENODE_PLATFORM := $(ARM_RENODE_PLATFORM_$(AIXOS_PLATFORM))
ARM_PLATFORM_DEFINE := $(ARM_PLATFORM_DEFINE_$(AIXOS_PLATFORM))
ARM_PLATFORM_CFLAGS := -DAIXOS_CFG_PLATFORM=$(ARM_PLATFORM_DEFINE)

ifeq ($(ARM_CPU),)
$(error Unknown AIXOS_PLATFORM '$(AIXOS_PLATFORM)'; expected one of $(ARM_PLATFORMS))
endif

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
COMMON_CFLAGS := $(COMMON_INCLUDES) $(COMMON_WARNINGS) $(CONFIG_CFLAGS) \
	$(ARM_PLATFORM_CFLAGS) -std=c99 -ffunction-sections -fdata-sections \
	-ffreestanding -MMD -MP

ARM_PREFIX ?= arm-none-eabi-
ARM_CC := $(ARM_PREFIX)gcc
ARM_AS := $(ARM_PREFIX)gcc
ARM_LD := $(ARM_PREFIX)ld
ARM_SIZE := $(ARM_PREFIX)size
ARM_MCU := -mcpu=$(ARM_CPU) -mthumb
ARM_BUILD ?= $(BUILD_DIR)/arm/$(AIXOS_PLATFORM)
ARM_C_SRCS := $(COMMON_SRCS) arch/arm/cortex-m3/port.c $(APP_SRCS)
ifeq ($(AIXOS_PLATFORM),cortex-m0)
ARM_PORTASM := arch/arm/cortex-m3/portasm_m0.s
else
ARM_PORTASM := arch/arm/cortex-m3/portasm.s
endif
ARM_S_SRCS := arch/arm/cortex-m3/startup_stm32f103.s $(ARM_PORTASM)
ARM_OBJS := $(addprefix $(ARM_BUILD)/,$(ARM_C_SRCS:.c=.o) $(ARM_S_SRCS:.s=.o))
ARM_CFLAGS := $(COMMON_CFLAGS) $(ARM_MCU) -Os
ARM_ASFLAGS := $(COMMON_INCLUDES) $(CONFIG_CFLAGS) $(ARM_MCU) -Wall \
	-x assembler-with-cpp
ARM_LDFLAGS := -EL --gc-sections -Map=$(ARM_BUILD)/$(TARGET).map \
	-T arch/arm/cortex-m3/link.ld
ARM_LIBGCC := $(shell $(ARM_CC) $(ARM_MCU) -print-libgcc-file-name 2>/dev/null)

LLVM_PREFIX ?= /opt/homebrew/opt/llvm
LLD_PREFIX ?= /opt/homebrew/opt/lld
A55_CC ?= $(if $(wildcard $(LLVM_PREFIX)/bin/clang),$(LLVM_PREFIX)/bin/clang,clang)
A55_LD ?= $(if $(wildcard $(LLD_PREFIX)/bin/ld.lld),$(LLD_PREFIX)/bin/ld.lld,ld.lld)
A55_SIZE ?= $(if $(wildcard $(LLVM_PREFIX)/bin/llvm-size),$(LLVM_PREFIX)/bin/llvm-size,llvm-size)
A55_TARGET ?= aarch64-none-elf
A55_BUILD ?= $(BUILD_DIR)/arm/cortex-a55
A55_C_SRCS := $(COMMON_SRCS) arch/arm/aarch64/port.c $(APP_SRCS)
A55_S_SRCS := arch/arm/aarch64/startup.s arch/arm/aarch64/portasm.s
A55_OBJS := $(addprefix $(A55_BUILD)/,$(A55_C_SRCS:.c=.o) $(A55_S_SRCS:.s=.o))
A55_ELF := $(A55_BUILD)/$(TARGET).elf
A55_CFLAGS := $(COMMON_INCLUDES) $(COMMON_WARNINGS) $(CONFIG_CFLAGS) \
	-DAIXOS_CFG_PLATFORM=AIXOS_CFG_PLATFORM_CORTEX_A55 \
	--target=$(A55_TARGET) -mcpu=cortex-a55 -mgeneral-regs-only \
	-std=c99 -ffunction-sections -fdata-sections -ffreestanding \
	-fno-builtin -MMD -MP -Os
A55_ASFLAGS := $(COMMON_INCLUDES) $(CONFIG_CFLAGS) --target=$(A55_TARGET) \
	-mcpu=cortex-a55 -x assembler-with-cpp -Wall
A55_LDFLAGS := -flavor gnu -EL --gc-sections -Map=$(A55_BUILD)/$(TARGET).map \
	-T arch/arm/aarch64/link.ld

RISCV_TOOLCHAIN_DIR ?=
RISCV_PREFIX ?= $(if $(shell command -v riscv-none-elf-gcc 2>/dev/null),riscv-none-elf-,$(if $(shell command -v riscv64-elf-gcc 2>/dev/null),riscv64-elf-,riscv-none-elf-))
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
ARM_RENODE_PLATFORMS := $(foreach platform,$(ARM_PLATFORMS),$(ARM_RENODE_PLATFORM_$(platform)))

HOST_CC ?= cc
HOST_LDFLAGS ?=
HOST_BUILD ?= $(BUILD_DIR)/host
HOST_OPT ?= -O0 -g3
HOST_SANITIZERS ?=
HOST_TEST_SRCS := tests/test_main.c tests/test_object.c tests/test_heap.c \
	tests/test_kernel.c tests/test_reliability.c tests/test_stress.c \
	tests/test_microkernel.c \
	tests/test_mpu.c \
	tests/test_path_coverage.c \
	tests/test_coverage_expansion.c \
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

ifeq ($(AIXOS_PLATFORM),cortex-a55)
arm: $(A55_ELF)
else
ifneq ($(filter $(AIXOS_PLATFORM),$(ARM_BOOT_PLATFORMS)),)
arm: $(ARM_BUILD)/$(TARGET).elf
else
arm:
	@echo "AIXOS_PLATFORM=$(AIXOS_PLATFORM) has Renode platform coverage only."
	@echo "Add an ARMv8-A/AArch64 architecture port before building AIXOS for Cortex-A55."
	@exit 2
endif
endif

ifneq ($(AIXOS_PLATFORM),cortex-a55)
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
endif

a55-objects: $(A55_OBJS)

$(A55_ELF): $(A55_OBJS) arch/arm/aarch64/link.ld
	@mkdir -p $(dir $@)
	$(A55_LD) $(A55_LDFLAGS) -o $@ $(A55_OBJS)
	$(A55_SIZE) $@

$(A55_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(A55_CC) $(A55_CFLAGS) -c -o $@ $<

$(A55_BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(A55_CC) $(A55_ASFLAGS) -c -o $@ $<

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
ifneq ($(filter $(AIXOS_PLATFORM),$(ARM_BOOT_PLATFORMS)),)
	$(ARM_CC) -Iposix/include $(filter-out -MMD -MP,$(ARM_CFLAGS)) \
		-fsyntax-only \
		tests/posix_api_compile.c
endif
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

ifeq ($(AIXOS_PLATFORM),cortex-a55)
renode: renode-cortex-a55
else
renode: arm
	@mkdir -p test-results/renode
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	@sh tools/run_arm_renode_smoke.sh $(AIXOS_PLATFORM) $(ARM_BUILD)/$(TARGET).elf
endif

renode-cortex-m0:
	$(MAKE) renode AIXOS_PLATFORM=cortex-m0

renode-cortex-m3:
	$(MAKE) renode AIXOS_PLATFORM=cortex-m3

renode-cortex-m4:
	$(MAKE) renode AIXOS_PLATFORM=cortex-m4

renode-cortex-m33:
	$(MAKE) renode AIXOS_PLATFORM=cortex-m33

renode-cortex-a55: $(A55_ELF)
	@mkdir -p test-results/renode
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	@sh tools/run_arm_renode_smoke.sh cortex-a55 $(A55_ELF)

renode-arm-platform-check: $(ARM_RENODE_PLATFORMS)
	@sh tools/check_arm_renode_platforms.sh

renode-arm-smoke: renode-cortex-m0 renode-cortex-m3 renode-cortex-m4 \
	renode-cortex-m33 renode-cortex-a55

renode-arm-platforms: renode-arm-platform-check renode-arm-smoke

renode-riscv: riscv-validate
	@mkdir -p test-results/renode-riscv
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	@sh tools/run_riscv_renode_smoke.sh 1 | tee test-results/renode-riscv/smoke.log

renode-riscv-stress: riscv-validate
	@mkdir -p test-results/renode-riscv-stress
	@rm -f tools/renode.config.lock
	@rm -f "$(RENODE_USER_LOCK)"
	@sh tools/run_riscv_renode_smoke.sh 5 | tee test-results/renode-riscv-stress/smoke.log

test-all: test arm renode riscv-validate renode-riscv

bench-build:
	$(MAKE) -C benchmarks/aixos all RISCV_TOOLCHAIN_DIR="$(RISCV_TOOLCHAIN_DIR)" RISCV_PREFIX="$(RISCV_PREFIX)"
	@if [ -d third_party/FreeRTOS-Kernel ]; then \
		$(MAKE) -C benchmarks/freertos all RISCV_TOOLCHAIN_DIR="$(RISCV_TOOLCHAIN_DIR)" RISCV_PREFIX="$(RISCV_PREFIX)"; \
	else \
		echo "Skipping FreeRTOS benchmark build: third_party/FreeRTOS-Kernel is not present"; \
	fi

latency-bench:
	$(MAKE) -C benchmarks/latency all RISCV_PREFIX=$(RISCV_PREFIX)

instruction-sim: $(A55_ELF)
	@sh tools/run_instruction_simulation.sh

instruction-bench: bench-build $(A55_ELF)
	@AIXOS_SIM_INCLUDE_BENCHMARKS=1 sh tools/run_instruction_simulation.sh

api-boundary-sim:
	@RISCV_PREFIX=$(RISCV_PREFIX) sh tools/run_api_boundary_renode.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all arm a55-objects riscv riscv-disasm riscv-validate check-riscv-toolchain \
	test test-mpu test-asan test-o2 test-os verify toolcheck renode \
	renode-cortex-m0 renode-cortex-m3 renode-cortex-m4 renode-cortex-m33 \
	renode-cortex-a55 renode-arm-platform-check renode-arm-smoke \
	renode-arm-platforms renode-riscv \
	renode-riscv-stress test-all coverage analyze ram-report manifest \
	posix-api-check reproducible quality evidence-package bench-build \
	latency-bench instruction-sim instruction-bench api-boundary-sim \
	config oldconfig clean

-include $(ARM_OBJS:.o=.d) $(A55_OBJS:.o=.d) $(RISCV_OBJS:.o=.d) \
	$(HOST_OBJS:.o=.d) $(HOST_MPU_OBJS:.o=.d)
