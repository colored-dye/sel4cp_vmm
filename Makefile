#
# Copyright 2021, Breakaway Consulting Pty. Ltd.
# Copyright 2022, UNSW (ABN 57 195 873 179)
#
# SPDX-License-Identifier: BSD-2-Clause
#
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(SEL4CP_SDK)),)
$(error SEL4CP_SDK must be specified)
endif

ifeq ($(strip $(SEL4CP_BOARD)),)
$(error SEL4CP_BOARD must be specified)
endif

ifeq ($(strip $(SEL4CP_CONFIG)),)
$(error SEL4CP_CONFIG must be specified)
endif

ifeq ($(strip $(SYSTEM)),)
$(error SYSTEM must be specified)
endif

# @ivanv: Check for dependencies and make sure they are installed/in the path

TOOLCHAIN := aarch64-linux-gnu
ARCH := aarch64

ifeq ($(SEL4CP_BOARD),qemu_riscv_virt)
	TOOLCHAIN = riscv64-unknown-elf
    ARCH = riscv64
endif

# ifeq ($(ARCH),riscv64)
# 	ifeq ($(strip $(OPENSBI)),)
# 	$(error Path to OPENSBI source must be specified)
# 	endif
# endif

OPENSBI := opensbi

CPU := cortex-a53

DTC := dtc
CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
SEL4CP_TOOL ?= $(SEL4CP_SDK)/bin/sel4cp

VMM_OBJS := vmm.o printf.o psci.o smc.o fault.o util.o vgic.o global_data.o

# @ivanv: hack...
ifeq ($(SEL4CP_BOARD),imx8mm_evk)
	VMM_OBJS += vgic_v3.o
else
	VMM_OBJS += vgic_v2.o
endif

ifeq ($(SEL4CP_BOARD),qemu_riscv_virt)
	VMM_OBJS = vmm_riscv.o printf.o util.o
endif

# @ivanv: need to have a step for putting in the initrd node into the DTB,
# 		  right now it is unfortunately hard-coded.

# @ivanv: Have a list of supported boards to check with, if it's not one of those
# have a helpful message that lists all the support boards.

SEL4CP_BOARD_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)
SRC_DIR := src
IMAGE_DIR := board/$(SEL4CP_BOARD)/images
SYSTEM_DESCRIPTION := board/$(SEL4CP_BOARD)/systems/$(SYSTEM)

KERNEL_IMAGE := $(IMAGE_DIR)/linux
DTB_SOURCE := $(IMAGE_DIR)/linux.dts
DTB_IMAGE := linux.dtb
INITRD_IMAGE := $(IMAGE_DIR)/rootfs.cpio.gz

LINUX_IMAGES := $(DTB_IMAGE)

ifeq ($(SEL4CP_BOARD),qemu_riscv_virt)
	LINUX_IMAGES = linux_yanyan.dtb
endif

IMAGES := vmm.elf $(LINUX_IMAGES)

# @ivanv: no optimisation level for now because I don't trust the compiler to not mess with the code
CFLAGS := -mstrict-align -nostdlib -ffreestanding -g3 -Wall -Wno-unused-function -Werror -I$(SEL4CP_BOARD_DIR)/include -DBOARD_$(SEL4CP_BOARD) -DARCH_$(ARCH)
ifeq ($(ARCH),aarch64)
	CFLAGS +=  -mcpu=$(CPU)
endif
LDFLAGS := -L$(SEL4CP_BOARD_DIR)/lib
LIBS := -lsel4cp -Tsel4cp.ld

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt
# PAYLOAD_FILE = $(BUILD_DIR)/platform/generic/firmware/fw_payload.elf

all: directories $(BUILD_DIR)/$(DTB_IMAGE) $(IMAGE_FILE)

directories:
	$(shell mkdir -p $(BUILD_DIR))

run: directories $(BUILD_DIR)/$(DTB_IMAGE) $(IMAGE_FILE)
# 	ifeq ($(SEL4CP_BOARD),qemu_arm_virt_hyp)
	qemu-system-aarch64 -machine virt,virtualization=on,highmem=off,secure=off -cpu $(CPU) -serial mon:stdio -device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 -m size=2G -nographic
# 	else ifeq ($(SEL4CP_BOARD),qemu_riscv_virt)
# 	qemu-system-riscv64 -machine virt -cpu rv64 -nographic -serial mon:stdio -m size=3072M -bios $(BUILD_DIR)/platform/generic/firmware/fw_payload.elf

$(BUILD_DIR)/$(DTB_IMAGE): $(DTB_SOURCE)
	# @ivanv: Shouldn't supress warnings
	$(DTC) -q -I dts -O dtb $< > $@

$(BUILD_DIR)/global_data.o: $(SRC_DIR)/global_data.S
	$(CC) -c -g3 -x assembler-with-cpp -mcpu=$(CPU) -DVM_KERNEL_IMAGE_PATH=\"$(KERNEL_IMAGE)\" -DVM_DTB_IMAGE_PATH=\"$(BUILD_DIR)/linux.dtb\" -DVM_INITRD_IMAGE_PATH=\"$(INITRD_IMAGE)\" $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/util/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/vgic/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/vmm.elf: $(addprefix $(BUILD_DIR)/, $(VMM_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_DESCRIPTION)
	$(SEL4CP_TOOL) $(SYSTEM_DESCRIPTION) --search-path $(BUILD_DIR) $(IMAGE_DIR) --board $(SEL4CP_BOARD) --config $(SEL4CP_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

# $(PAYLOAD_FILE): $(IMAGE_FILE)
# 	make -C $(OPENSBI) -j12 PLATFORM=generic CROSS_COMPILE=riscv64-unknown-elf- FW_PAYLOAD_PATH=$(IMAGE_FILE) \
# 	PLATFORM_RISCV_XLEN=64 PLATFORM_RISCV_ISA=rv64imac PLATFORM_RISCV_ABI=lp64 O=$(BUILD_DIR)
