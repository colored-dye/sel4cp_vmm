# QEMU ARM virt images

## Linux kernel

### Details
* Image name: `linux`
* Config name: `linux_config`
* Git remote: https://github.com/torvalds/linux.git
* Tag: v5.18 (commit hash: `4b0986a3613c92f4ec1bdc7f60ec66fea135991f`)
* Toolchain: `aarch64-none-elf`
    * Version: GNU Toolchain for the A-profile Architecture 10.2-2020.11 (arm-10.16)) 10.2.1 20201103

You can also get the Linux config used after booting by running the following
command in userspace: `zcat /proc/config.gz`.

### Instructions for reproducing
```
git clone --depth 1 --branch v5.18 https://github.com/torvalds/linux.git
make ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- linux_config all -j$(nproc)
```

The path to the image is: `linux/arm64/boot/Image`.

## Buildroot RootFS image

### Details
* Image name: `rootfs.cpio.gz`
* Config name: `buildroot_config`
* Version: 2022.08-rc2

### Instructions for reproducing

```
wget https://buildroot.org/downloads/buildroot-2022.08-rc2.tar.xz
tar xvf buildroot-2022.08-rc2.tar.xz
cd buildroot-2022.08-rc2
```
