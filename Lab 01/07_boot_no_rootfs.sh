#!/bin/bash
# HOST. Part B.4: expected to end in "Kernel panic - not syncing: VFS: Unable to mount root fs".
mkdir -p ~/embedded_lab1/logs
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic \
  -kernel ~/embedded_lab1/output/zImage -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -append 'console=ttyAMA0,115200 root=/dev/ram0 rw mem=512M' | tee ~/embedded_lab1/logs/B_panic_log.txt
