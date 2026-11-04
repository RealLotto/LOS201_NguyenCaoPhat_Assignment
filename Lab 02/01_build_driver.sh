#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/00_environment.sh"
mkdir -p "$SCRIPT_DIR/logs"

if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
  make -C "$SCRIPT_DIR/driver" \
    KDIR="$HOME/embedded_lab1/kernel/linux-5.15" \
    ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCFLAGS=-march=armv7-a
else
  "$SCRIPT_DIR/container_run.sh" make -C "$SCRIPT_DIR/driver" \
    KDIR="$HOME/embedded_lab1/kernel/linux-5.15" \
    ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCFLAGS=-march=armv7-a
fi

MODULE="$SCRIPT_DIR/driver/lab2_driver.ko"
if [[ ! -s "$MODULE" ]]; then
  echo "Kernel module build did not produce $MODULE." >&2
  exit 1
fi

if command -v modinfo >/dev/null 2>&1; then
  modinfo "$MODULE" | tee "$SCRIPT_DIR/logs/module_modinfo.txt"
  RELEASE="$(modinfo -F vermagic "$MODULE" | awk '{print $1}')"
else
  RELEASE="$("$SCRIPT_DIR/container_run.sh" modinfo -F vermagic \
    "$MODULE" | awk '{print $1}')"
fi
EXPECTED_RELEASE="$(sed -n 's/^#define UTS_RELEASE "\(.*\)"/\1/p' \
  "$HOME/embedded_lab1/kernel/linux-5.15/include/generated/utsrelease.h")"
if [[ "$RELEASE" != "$EXPECTED_RELEASE" ]]; then
  echo "Module vermagic release '$RELEASE' does not match LAB-01 '$EXPECTED_RELEASE'." >&2
  exit 1
fi

file "$MODULE" | tee "$SCRIPT_DIR/logs/module_file.txt"
echo "Module release check passed: $RELEASE"
