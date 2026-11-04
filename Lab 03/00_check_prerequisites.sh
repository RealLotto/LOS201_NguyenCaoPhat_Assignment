#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LAB1="${LAB1:-$HOME/embedded_lab1}"
LAB2="${LAB2:-$HOME/embedded_lab2}"
LAB3="${LAB3:-$SCRIPT_DIR}"
KDIR="$LAB1/kernel/linux-5.15"
KERNEL_RELEASE="$(sed -n 's/^#define UTS_RELEASE "\(.*\)"/\1/p' \
  "$KDIR/include/generated/utsrelease.h" 2>/dev/null || true)"

required=(
  "$LAB1/output/zImage"
  "$LAB1/output/vexpress-v2p-ca9.dtb"
  "$LAB1/output/initramfs.cpio.gz"
  "$LAB1/configs/kernel.config"
  "$KDIR/Module.symvers"
  "$KDIR/include/config/auto.conf"
  "$LAB2/output/initramfs_lab2.cpio.gz"
)

for path in "${required[@]}"; do
  if [[ ! -s "$path" ]]; then
    echo "Required LAB-01/LAB-02 artifact is missing or empty: $path" >&2
    exit 1
  fi
done

if [[ "$KERNEL_RELEASE" != "5.15.0" ]]; then
  echo "Expected the LAB-01 5.15.0 kernel; found '$KERNEL_RELEASE'." >&2
  exit 1
fi

if ! grep -q '^CONFIG_MODULES=y$' "$LAB1/configs/kernel.config"; then
  echo "The LAB-01 kernel was not configured with loadable module support." >&2
  exit 1
fi

for path in \
  "$LAB2/output/zImage" \
  "$LAB2/output/vexpress-v2p-ca9.dtb"; do
  if [[ ! -s "$path" ]]; then
    echo "LAB-02 is missing a copied LAB-01 boot artifact: $path" >&2
    exit 1
  fi
done

cmp -s "$LAB1/output/zImage" "$LAB2/output/zImage" || {
  echo "LAB-02 zImage does not match LAB-01; refusing an incompatible build." >&2
  exit 1
}
cmp -s "$LAB1/output/vexpress-v2p-ca9.dtb" \
  "$LAB2/output/vexpress-v2p-ca9.dtb" || {
  echo "LAB-02 DTB does not match LAB-01; refusing an incompatible build." >&2
  exit 1
}

if ! gzip -dc "$LAB2/output/initramfs_lab2.cpio.gz" |
  cpio -it --quiet 2>/dev/null |
  grep -Fxq 'lib/modules/5.15.0/lab2_driver.ko'; then
  echo "LAB-02 initramfs does not contain its 5.15.0 driver module." >&2
  exit 1
fi

mkdir -p "$LAB3"/{apps,bao_cao,configs,logs,output,rootfs/initramfs}
cp "$LAB1/configs/kernel.config" "$LAB3/configs/kernel.config"
cp "$LAB1/configs/busybox.config" "$LAB3/configs/busybox.config"
printf 'LAB1_KERNEL_RELEASE=%s\nLAB2_INITRAMFS=%s\n' \
  "$KERNEL_RELEASE" "$LAB2/output/initramfs_lab2.cpio.gz" \
  > "$LAB3/configs/lab-integration.env"

{
  echo "LAB-01/LAB-02 compatibility: PASS"
  echo "Kernel release: $KERNEL_RELEASE"
  echo "LAB-01 and LAB-02 zImage/DTB: identical"
  echo "LAB-02 initramfs includes lab2_driver.ko for $KERNEL_RELEASE"
} | tee "$LAB3/logs/prerequisites.txt"
