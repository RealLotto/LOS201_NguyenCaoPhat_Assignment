#!/bin/bash
# HOST. Part B.2: boot U-Boot shell (exit QEMU: Ctrl+A then X). Commands to type: uboot_commands.txt
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic -kernel ~/embedded_lab1/output/u-boot
