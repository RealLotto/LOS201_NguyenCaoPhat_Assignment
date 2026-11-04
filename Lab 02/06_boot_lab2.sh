#!/usr/bin/env bash
set -euo pipefail

CPUS="${1:-2}"
if [[ ! "$CPUS" =~ ^[1-4]$ ]]; then
  echo "Usage: $0 [1-4]" >&2
  exit 2
fi

HOME_DIR="${HOME:?HOME is not set}"
LAB1="$HOME_DIR/embedded_lab1"
LAB2="$HOME_DIR/embedded_lab2"
mkdir -p "$LAB2/logs"
exec qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp "$CPUS" \
  -nographic \
  -kernel "$LAB1/output/zImage" \
  -dtb "$LAB1/output/vexpress-v2p-ca9.dtb" \
  -initrd "$LAB2/output/initramfs_lab2.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' \
  2>&1 | tee "$LAB2/logs/boot_manual_smp${CPUS}.txt"
