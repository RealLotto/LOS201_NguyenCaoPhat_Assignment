#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LAB2="$HOME/embedded_lab2"
PACKAGE_NAME="${1:?package name is required}"
PACKAGE_OUTPUT="${LAB2_PACKAGE_OUTPUT:?package output path is required}"
STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

for path in \
  "$SCRIPT_DIR/driver/lab2_driver.c" \
  "$SCRIPT_DIR/driver/Makefile" \
  "$SCRIPT_DIR/driver/lab2_driver.ko" \
  "$LAB2/output/initramfs_lab2.cpio.gz" \
  "$LAB2/output/zImage" \
  "$LAB2/output/vexpress-v2p-ca9.dtb" \
  "$LAB2/mtd/rootfs.jffs2" \
  "$SCRIPT_DIR/bao_cao/SE201044_Lab02_BaoCao.pdf"; do
  if [[ ! -s "$path" ]]; then
    echo "Required LAB-02 deliverable is missing: $path" >&2
    exit 1
  fi
done

STAGED="$STAGING_DIR/$PACKAGE_NAME"
mkdir -p "$STAGED"/{driver,rootfs/initramfs,output,logs,bao_cao,configs,mtd,project-scripts}
mkdir -p "$SCRIPT_DIR/logs"
cp "$SCRIPT_DIR/driver/lab2_driver.c" "$SCRIPT_DIR/driver/Makefile" \
  "$SCRIPT_DIR/driver/lab2_driver.ko" "$STAGED/driver/"
cp "$LAB2/output/"{initramfs_lab2.cpio.gz,zImage,vexpress-v2p-ca9.dtb} "$STAGED/output/"
cp "$LAB2/mtd/rootfs.jffs2" "$STAGED/mtd/"
cp "$SCRIPT_DIR/bao_cao/SE201044_Lab02_BaoCao.pdf" "$STAGED/bao_cao/"
cp "$SCRIPT_DIR/bao_cao/SE201044_Lab02_BaoCao.html" "$STAGED/bao_cao/"
cp "$SCRIPT_DIR/README.txt" "$STAGED/"
cp -a "$SCRIPT_DIR/rootfs/overlay" "$STAGED/rootfs/"
cp "$SCRIPT_DIR/"*.sh "$SCRIPT_DIR/run_qemu_tests.py" "$STAGED/project-scripts/"
cp "$LAB2/configs/"* "$STAGED/configs/"
cp -a "$LAB2/logs/." "$STAGED/logs/"
cp -a "$SCRIPT_DIR/logs/." "$STAGED/logs/"
(
  cd "$STAGED/rootfs/initramfs"
  gzip -dc "$LAB2/output/initramfs_lab2.cpio.gz" | cpio -id --quiet
)

tar -czf "$PACKAGE_OUTPUT" -C "$STAGING_DIR" "$PACKAGE_NAME"
echo "Created $PACKAGE_OUTPUT"
