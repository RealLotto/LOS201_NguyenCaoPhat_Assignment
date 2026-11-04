#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  echo "Usage: $0 {02_build_kernel.sh|03_build_uboot.sh|04_build_busybox.sh}" >&2
}

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

case "$1" in
  02_build_kernel.sh|03_build_uboot.sh|04_build_busybox.sh)
    BUILD_SCRIPT="$SCRIPT_DIR/$1"
    ;;
  *)
    usage
    echo "Unsupported build step: $1" >&2
    exit 2
    ;;
esac

if [[ ! -f "$BUILD_SCRIPT" ]]; then
  echo "Build script not found: $BUILD_SCRIPT" >&2
  exit 1
fi

if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
  exec bash "$BUILD_SCRIPT"
fi

if ! command -v podman >/dev/null 2>&1; then
  echo "Neither the ARM cross-compiler nor Podman is installed. Run ./00_setup_ubuntu.sh." >&2
  exit 1
fi

if ! podman image exists lab1-build; then
  echo "Build image 'lab1-build' is missing. Run ./00_setup_ubuntu.sh first." >&2
  exit 1
fi

exec "$SCRIPT_DIR/container_run.sh" bash "/scripts/$1"
