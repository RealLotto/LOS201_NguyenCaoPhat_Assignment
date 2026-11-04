#!/usr/bin/env bash
set -euo pipefail

N="${1:?usage: ./09_package.sh SE201044_NguyenCaoPhat}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
HOME_DIR="${HOME:?HOME is not set}"

if [[ $EUID -ne 0 && -z ${FAKEROOTKEY:-} ]]; then
  if command -v fakeroot >/dev/null 2>&1; then
    exec fakeroot bash "$0" "$@"
  elif command -v podman >/dev/null 2>&1 && podman image exists lab1-build; then
    exec podman run --rm --userns=keep-id --security-opt label=disable \
      -e HOME="$HOME_DIR" \
      -v "$HOME_DIR/embedded_lab1:$HOME_DIR/embedded_lab1" \
      -v "$SCRIPT_DIR:$SCRIPT_DIR:rw" \
      -w "$HOME_DIR/embedded_lab1" \
      lab1-build fakeroot bash "$SCRIPT_DIR/09_package.sh" "$@"
  else
    echo "Packaging device nodes requires root or fakeroot. Run ./00_setup_ubuntu.sh first." >&2
    exit 1
  fi
fi

SOURCE_DIR="$HOME_DIR/embedded_lab1"
REPORT_DIR="$SCRIPT_DIR/bao_cao"
ARCHIVE="$SCRIPT_DIR/${N}_Lab01.tar.gz"
STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

if [[ ! "$N" =~ ^[[:alnum:]][[:alnum:]_-]*$ ]]; then
  echo "Use a package name containing only letters, digits, underscores, and hyphens." >&2
  exit 1
fi

for path in \
  "$SOURCE_DIR/output/zImage" \
  "$SOURCE_DIR/output/vexpress-v2p-ca9.dtb" \
  "$SOURCE_DIR/output/u-boot" \
  "$SOURCE_DIR/output/initramfs.cpio.gz" \
  "$SOURCE_DIR/configs/kernel.config" \
  "$SOURCE_DIR/configs/busybox.config" \
  "$SOURCE_DIR/rootfs/initramfs/init"; do
  if [[ ! -f "$path" ]]; then
    echo "Required Lab 1 artifact is missing: $path" >&2
    exit 1
  fi
done

if [[ ! -d "$REPORT_DIR" ]] || ! compgen -G "$REPORT_DIR/*.pdf" >/dev/null; then
  echo "Place the required Lab 1 report PDF in $REPORT_DIR before packaging." >&2
  exit 1
fi

STAGED_PACKAGE="$STAGING_DIR/${N}_Lab01"
mkdir -p "$STAGED_PACKAGE/bao_cao"
cp -a "$SOURCE_DIR/output" "$SOURCE_DIR/configs" "$STAGED_PACKAGE/"
mkdir -p "$STAGED_PACKAGE/rootfs"
mkdir -p "$STAGED_PACKAGE/rootfs/initramfs"
(
  cd "$STAGED_PACKAGE/rootfs/initramfs"
  gzip -dc "$SOURCE_DIR/output/initramfs.cpio.gz" | cpio -id --quiet
)
cp -a "$REPORT_DIR/." "$STAGED_PACKAGE/bao_cao/"
if [[ -d "$SOURCE_DIR/logs" ]]; then
  cp -a "$SOURCE_DIR/logs" "$STAGED_PACKAGE/"
fi

tar -czf "$ARCHIVE" -C "$STAGING_DIR" "${N}_Lab01"
echo "Created $ARCHIVE"
