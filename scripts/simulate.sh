#!/usr/bin/sh
qemu-system-aarch64 \
	-machine virt,virtualization=on,highmem=off,secure=off \
	-cpu cortex-a53 \
	-serial mon:stdio \
	-device loader,file=loader.img,addr=0x70000000,cpu-num=0 \
	-m size=2G \
	-nographic

