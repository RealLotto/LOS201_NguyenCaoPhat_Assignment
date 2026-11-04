#!/usr/bin/env bash
set -euo pipefail

LAB1="$HOME/embedded_lab1"
LAB2="$HOME/embedded_lab2"
KDIR="$LAB1/kernel/linux-5.15"

for path in \
  "$LAB1/output/zImage" \
  "$LAB1/output/vexpress-v2p-ca9.dtb" \
  "$LAB1/output/initramfs.cpio.gz" \
  "$LAB1/configs/kernel.config" \
  "$KDIR/Module.symvers" \
  "$KDIR/include/config/auto.conf"; do
  if [[ ! -f "$path" ]]; then
    echo "Required LAB-01 artifact is missing: $path" >&2
    exit 1
  fi
done

if ! grep -q '^CONFIG_MODULES=y$' "$LAB1/configs/kernel.config"; then
  echo "The LAB-01 kernel was not configured with loadable module support." >&2
  exit 1
fi

KERNEL_RELEASE="$(sed -n 's/^#define UTS_RELEASE "\(.*\)"/\1/p' "$KDIR/include/generated/utsrelease.h")"
if [[ "$KERNEL_RELEASE" != 5.15.0 ]]; then
  echo "Expected the LAB-01 5.15.0 kernel; found '$KERNEL_RELEASE'." >&2
  exit 1
fi

mkdir -p "$LAB2"/{driver,rootfs/initramfs,output,logs,configs,bao_cao,mtd}
printf 'LAB1_KERNEL_RELEASE=%s\n' "$KERNEL_RELEASE" > "$LAB2/configs/lab1-integration.env"
echo "LAB-01 integration verified: $KERNEL_RELEASE at $KDIR"
echo "LAB-02 workspace: $LAB2"
