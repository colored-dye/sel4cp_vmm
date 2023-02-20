#!/usr/bin/sh
make run \
    BUILD_DIR=build \
    SEL4CP_SDK=`realpath ./sel4cp/release/sel4cp-sdk-1.2.6` \
    SEL4CP_CONFIG=debug \
    SEL4CP_BOARD=qemu_arm_virt_hyp \
    SYSTEM=simple.system
