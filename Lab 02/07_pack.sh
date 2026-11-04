#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_NAME="${1:-SE201044_NguyenCaoPhat_Lab02}"
if [[ ! "$PACKAGE_NAME" =~ ^[[:alnum:]][[:alnum:]_-]*_Lab02$ ]]; then
  echo "Usage: $0 SE201044_NguyenCaoPhat_Lab02" >&2
  exit 2
fi

"$SCRIPT_DIR/00_environment.sh"
"$SCRIPT_DIR/02_build_initramfs.sh"
"$SCRIPT_DIR/container_run.sh" env "LAB2_PACKAGE_OUTPUT=$SCRIPT_DIR/$PACKAGE_NAME.tar.gz" \
  fakeroot bash /scripts/07_pack_inner.sh "$PACKAGE_NAME"
