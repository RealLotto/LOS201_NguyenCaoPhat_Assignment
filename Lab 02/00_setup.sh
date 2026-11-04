#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if ! podman image exists lab1-build; then
  echo "LAB-01 build image 'lab1-build' is missing. Complete LAB-01 setup first." >&2
  exit 1
fi

"$SCRIPT_DIR/00_environment.sh"
podman build -t lab2-build -f "$SCRIPT_DIR/Containerfile" "$SCRIPT_DIR"
echo "LAB-02 container ready (ARM build tools from LAB-01 and mtd-utils)."
