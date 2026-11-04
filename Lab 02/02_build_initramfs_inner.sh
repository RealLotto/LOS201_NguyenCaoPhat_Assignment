#!/usr/bin/env bash
set -euo pipefail

LAB1="$HOME/embedded_lab1"
LAB2="$HOME/embedded_lab2"
ROOTFS="$LAB2/rootfs/initramfs"
KERNEL_RELEASE="$(sed -n 's/^#define UTS_RELEASE "\(.*\)"/\1/p' \
  "$LAB1/kernel/linux-5.15/include/generated/utsrelease.h")"

rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"
(
  cd "$ROOTFS"
  gzip -dc "$LAB1/output/initramfs.cpio.gz" | cpio -id --quiet
)

cp -a /scripts/rootfs/overlay/. "$ROOTFS/"
mkdir -p "$ROOTFS/lib/modules/$KERNEL_RELEASE"
install -m 0644 /scripts/driver/lab2_driver.ko \
  "$ROOTFS/lib/modules/$KERNEL_RELEASE/lab2_driver.ko"
for test_script in test_driver.sh test_procfs.sh test_module_lifecycle.sh; do
  install -m 0755 "/scripts/rootfs/overlay/usr/bin/$test_script" \
    "$ROOTFS/usr/bin/$test_script"
done
chmod 0755 "$ROOTFS/etc/init.d/rcS"

# fakeroot preserves these special files in cpio without requiring host root.
rm -f "$ROOTFS/dev/console" "$ROOTFS/dev/null" "$ROOTFS/dev/zero" \
  "$ROOTFS/dev/random"
mknod -m 600 "$ROOTFS/dev/console" c 5 1
mknod -m 666 "$ROOTFS/dev/null" c 1 3
mknod -m 666 "$ROOTFS/dev/zero" c 1 5
mknod -m 444 "$ROOTFS/dev/random" c 1 8

mkdir -p "$LAB2/output" "$LAB2/configs" "$LAB2/logs"
cp "$LAB1/output/zImage" "$LAB2/output/"
cp "$LAB1/output/vexpress-v2p-ca9.dtb" "$LAB2/output/"
cp "$LAB1/configs/kernel.config" "$LAB2/configs/"
cp "$LAB1/configs/busybox.config" "$LAB2/configs/"

(
  cd "$ROOTFS"
  find . -print0 | cpio --null -o -H newc -R 0:0 --quiet |
    gzip -n -9 > "$LAB2/output/initramfs_lab2.cpio.gz"
)

gzip -t "$LAB2/output/initramfs_lab2.cpio.gz"
gzip -dc "$LAB2/output/initramfs_lab2.cpio.gz" |
  cpio -itv 2>/dev/null |
  grep -E 'lab2_driver\.ko|test_(driver|procfs|module_lifecycle)\.sh|dev/(console|null|zero|random)' \
    > "$LAB2/logs/initramfs_inventory.txt"
cat "$LAB2/logs/initramfs_inventory.txt"
ls -lh "$LAB2/output/initramfs_lab2.cpio.gz"
echo "LAB-02 initramfs built with kernel release $KERNEL_RELEASE"
