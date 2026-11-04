#!/bin/bash
# Part C.1: static BusyBox 1.35.0, ARMv7 hard-float (menuconfig settings applied non-interactively).
set -euo pipefail
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
L=$HOME/embedded_lab1
cd $L/rootfs/busybox-1.35.0
make defconfig
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's|^CONFIG_CROSS_COMPILER_PREFIX=.*|CONFIG_CROSS_COMPILER_PREFIX="arm-linux-gnueabihf-"|' .config
sed -i 's|^CONFIG_EXTRA_CFLAGS=.*|CONFIG_EXTRA_CFLAGS="-march=armv7-a -mfpu=neon-vfpv3 -mfloat-abi=hard"|' .config
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
( set +o pipefail; yes "" | make oldconfig >/dev/null )
grep -E 'CONFIG_STATIC|CROSS_COMPILER_PREFIX|EXTRA_CFLAGS|CONFIG_TC' .config
( make -j"$(nproc)" && make install ) 2>&1 | tee $L/logs/C_busybox_build.log
ls _install/
file _install/bin/busybox
cp .config $L/configs/busybox.config
echo "BusyBox done."
