#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/00_environment.sh"
"$SCRIPT_DIR/02_build_initramfs.sh"
python3 "$SCRIPT_DIR/run_qemu_tests.py"
