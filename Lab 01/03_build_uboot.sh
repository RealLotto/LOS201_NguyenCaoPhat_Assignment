#!/bin/bash
# Part B.1: build U-Boot 2022.04 for vexpress-a9.
set -euo pipefail
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
L=$HOME/embedded_lab1
cd $L/uboot/u-boot-2022.04
make vexpress_ca9x4_defconfig
( make -j"$(nproc)" ) 2>&1 | tee $L/logs/B_uboot_build.log
ls -lh u-boot u-boot.bin; file u-boot
arm-linux-gnueabihf-readelf -A u-boot | grep 'Tag_CPU_arch'
cp u-boot $L/output/
echo "U-Boot done."
