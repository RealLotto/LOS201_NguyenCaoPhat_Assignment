#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

sudo apt-get update
sudo apt-get install -y \
  podman qemu-system-arm qemu-utils curl ca-certificates cpio gzip file \
  tar bzip2 xz-utils make

podman build -t lab1-build -f "$SCRIPT_DIR/Containerfile" "$SCRIPT_DIR"
echo "Ubuntu host and lab1-build container are ready."
