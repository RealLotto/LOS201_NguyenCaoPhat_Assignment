#!/bin/bash
# Part A: configure + build Linux 5.15 for vexpress-a9. (Runs inside container or on host with gnueabihf toolchain.)
set -euo pipefail
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
# Keep kernel code soft-float, but force ARMv7 code generation. The fallback
# selected by this Linux 5.15 Kbuild/toolchain combination otherwise emits
# ARMv5 assembly that cannot encode the vexpress-a9 barriers.
export KCFLAGS=-march=armv7-a
L="$HOME/embedded_lab1"
K="$L/kernel/linux-5.15"
mkdir -p "$L/logs" "$L/output" "$L/configs"
cd $K
echo $ARCH $CROSS_COMPILE; arm-linux-gnueabihf-gcc -Q --help=target | grep march | head -2
make vexpress_defconfig 2>&1 | tee "$L/logs/A_defconfig.log"
grep -E 'CONFIG_ARCH_VEXPRESS|CONFIG_ARM_LPAE|CONFIG_CPU_V7' .config | tee -a "$L/logs/A_defconfig.log"
# menuconfig changes done non-interactively:
#  initramfs (required), PL011 serial console, SMP (bonus), modules (needed by LAB-02), devtmpfs
./scripts/config -e BLK_DEV_INITRD -e SERIAL_AMBA_PL011 -e SERIAL_AMBA_PL011_CONSOLE \
                 -e SMP -e MODULES -e MODULE_UNLOAD -e DEVTMPFS
make olddefconfig
grep -E '^CONFIG_(BLK_DEV_INITRD|SERIAL_AMBA_PL011|SERIAL_AMBA_PL011_CONSOLE|SMP|MODULES|MODULE_UNLOAD|DEVTMPFS)=' .config
# The lab's hard-float KCFLAGS are intentionally not used: ARM kernel code uses
# the soft-float calling convention. The ARMv7-only KCFLAGS above do not change
# the ABI or override the kernel's -msoft-float setting.
( time make -j"$(nproc)" zImage dtbs modules ) 2>&1 | tee "$L/logs/A_build.log"
make modules_prepare
ls -lh arch/arm/boot/zImage arch/arm/boot/dts/vexpress-v2p-ca9.dtb | tee "$L/logs/A_result.log"
file arch/arm/boot/zImage | tee -a "$L/logs/A_result.log"
arm-linux-gnueabihf-readelf -A arch/arm/boot/compressed/vmlinux | grep -i 'arch\|cpu' | tee -a "$L/logs/A_result.log"
cp arch/arm/boot/zImage arch/arm/boot/dts/vexpress-v2p-ca9.dtb "$L/output/"
cp .config "$L/configs/kernel.config"
echo "Kernel done."
