#!/bin/bash
# HOST. Creates ~/embedded_lab1 and downloads/extracts kernel, U-Boot, BusyBox (lab section 3.3).
set -e
L=$HOME/embedded_lab1
mkdir -p $L/{kernel,uboot,rootfs,output,configs,logs}
cd $L/kernel;  [ -f linux-5.15.tar.xz ] || curl -fLO https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.tar.xz
[ -d linux-5.15 ] || tar -xf linux-5.15.tar.xz
cd $L/uboot;   [ -f u-boot-2022.04.tar.bz2 ] || curl -fLO https://ftp.denx.de/pub/u-boot/u-boot-2022.04.tar.bz2
[ -d u-boot-2022.04 ] || tar -xf u-boot-2022.04.tar.bz2
cd $L/rootfs;  [ -f busybox-1.35.0.tar.bz2 ] || curl -fLO https://busybox.net/downloads/busybox-1.35.0.tar.bz2
[ -d busybox-1.35.0 ] || tar -xf busybox-1.35.0.tar.bz2
echo "Sources ready in $L"
