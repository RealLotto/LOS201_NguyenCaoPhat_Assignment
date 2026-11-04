#!/bin/bash
# HOST. Part C.5: full boot to BusyBox shell. Optional arg: number of CPUs (default 2).
mkdir -p ~/embedded_lab1/logs
SMP=${1:-2}
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp $SMP -nographic \
  -kernel ~/embedded_lab1/output/zImage -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab1/output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' | tee ~/embedded_lab1/logs/C_boot_smp$SMP.txt
