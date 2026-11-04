#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/00_check_prerequisites.sh"
make -C "$SCRIPT_DIR" clean all

for binary in \
  "$SCRIPT_DIR/apps/buggy_app" \
  "$SCRIPT_DIR/apps/leaky_app_arm" \
  "$SCRIPT_DIR/apps/perf_target_arm"; do
  description="$(file -b "$binary")"
  printf '%s: %s\n' "$(basename "$binary")" "$description"
  if [[ "$description" != *"ARM"* ]]; then
    echo "Cross-compiled binary is not an ARM executable: $binary" >&2
    exit 1
  fi
done

file "$SCRIPT_DIR/apps/leaky_app" "$SCRIPT_DIR/apps/leaky_app_fixed" \
  "$SCRIPT_DIR/apps/perf_target" | tee "$SCRIPT_DIR/logs/host_binaries.txt"
echo "Host and ARM application builds passed."
