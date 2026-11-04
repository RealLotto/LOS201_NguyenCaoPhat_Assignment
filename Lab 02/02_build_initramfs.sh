#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/00_environment.sh"
"$SCRIPT_DIR/01_build_driver.sh"

exec "$SCRIPT_DIR/container_run.sh" fakeroot bash /scripts/02_build_initramfs_inner.sh
